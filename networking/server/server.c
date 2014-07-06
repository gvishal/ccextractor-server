#include "server.h"
#include "utils.h"
#include "networking.h"
#include "params.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <limits.h> 
#include <assert.h>
#include <errno.h>

#include <sys/file.h>

/* TODO: remove it, or move to utils.h and apply everywhere */
#define FALSE 0
#define TRUE 1

#define PASSW_LEN 16

#define MAX_CONN 10

struct cli_t
{
	unsigned is_logged : 1;
	time_t muted_since;
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	time_t prgrm_statr;

	char buf_file_path[PATH_MAX];
	FILE *buf_fp;
	int cur_line;

	char arch_filepath[PATH_MAX];
	FILE *arch_fp;
} clients[MAX_CONN]; 

struct pollfd fds[MAX_CONN];

int main(int argc, char *argv[])
{
	int rc;

	if ((rc = init_cfg()) < 0)
	{
		e_log(0, rc);
		exit(EXIT_FAILURE);
	}

	if (2 != argc) 
	{
		fprintf(stderr, "Usage: %s port \n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* line buffering in stdout */
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) 
    {
		_log(0, "setvbuf() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	if ((rc = parse_config_file()) < 0)
	{
		e_log(0, rc);
		exit(EXIT_FAILURE);
	}

	int listen_sd = bind_server(cfg.port);
	if (listen_sd < 0) 
	{
		exit(EXIT_FAILURE);
	}

	printf("Server is binded to %d\n", cfg.port);
	printf("Password: %s\n\n", cfg.pwd);

	if (cfg.create_logs)
		open_log_file();

	_signal(SIGALRM, unmute_clients);

	fds[0].fd = listen_sd;
	fds[0].events = POLLIN;
	clients[0].is_logged = 1;
	nfds_t nfds = 1;

	int i, j;
	int compress_array;

	char buffer[BUFFER_SIZE];

	while (1)
	{
		if (poll(fds, nfds, -1) < 0) 
		{ 
			if (EINTR == errno) 
				continue;

			_log(0, "poll() error: %s\n", strerror(errno));
			break;
		}

		int current_size = nfds;
		for (i = 0; i < current_size; i++) {
			if (0 == fds[i].revents)
				continue;

			if (fds[i].revents & POLLHUP)
			{
				compress_array = TRUE;
				close_conn(i);
				continue;
			}

			if (fds[i].revents & POLLERR) 
			{
				_log(0, "poll() error: Revents %d\n", fds[i].revents);
			}

			if (fds[i].fd == listen_sd) 
			{
				struct sockaddr cliaddr;
				socklen_t clilen = sizeof(struct sockaddr);

				int new_sd;
				while((new_sd = accept(listen_sd, &cliaddr, &clilen)) >= 0) 
				{
					if (0 != (rc = getnameinfo(&cliaddr, clilen, 
							clients[nfds].host, sizeof(clients[nfds].host), 
							clients[nfds].serv, sizeof(clients[nfds].serv), 
							0)))
					{
						_log(0, "getnameinfo() error: %s\n", gai_strerror(rc));
					}

					fds[nfds].fd = new_sd;
					fds[nfds].events = POLLIN;
					clients[nfds].is_logged = 0;

					_log(0, "Connected [%d] (%s: %s)\n",
							nfds, clients[nfds].host, clients[nfds].serv);

					nfds++;

					write_byte(new_sd, PASSW);
				}

				if (errno != EWOULDBLOCK) /* accepted all of connections */
				{
					_log(0, "accept() error: %s\n", strerror(errno));
					goto end_server;
				}
			}
			else if (clinet_command(i) < 0)
			{
				compress_array = TRUE;
				close_conn(i);
			} 
		} /* End of loop through pollable descriptors */

		if (compress_array) {
			compress_array = FALSE;
			for (i = 0; i < nfds; i++) {
				if (fds[i].fd >= 0)
					continue;

				for(j = i; j < nfds; j++)
				{
					fds[j].fd = fds[j + 1].fd;
					memcpy(&clients[j], &clients[j + 1], sizeof(struct cli_t));
				}
				i--;
				nfds--;
			}
		}
	}

end_server:
	for (i = 0; i < nfds; i++)
	{
		if(fds[i].fd >= 0)
			close_conn(i);
	}
	return 0;
}

int clinet_command(int id)
{
	char c;
	int rc;
	size_t len = BUFFER_SIZE;

	char buf[BUFFER_SIZE] = {0};
	if ((rc = read_block(fds[id].fd, &c, buf, &len)) <= 0)
	{
		e_log(id, rc);
		return -1;
	}

	if (clients[id].muted_since > 0)
		return 0;

	if (c != PASSW && !clients[id].is_logged)
	{
		_log(id, "No password presented\n");
		return -1;
	}

	switch (c)
	{
	case PASSW:
		_log(id, "Password: %s\n", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			_log(id, "Wrong password\n", buf);

			clients[id].muted_since = time(NULL);

			alarm(cfg.wrong_pwd_delay);

			if (write_byte(fds[id].fd, WRONG_PASSW) != 1)
				return -1;

			return 0;
		}

		clients[id].is_logged = 1;

		_log(id, "Correct password\n", buf);

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		update_users_file();
		break;

	case CC:
		_log(id, "Caption block, size: %d\n", len);

		if (store_cc(id, buf, len) < 0)
			return -1;

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		break;

	default:
		_log(id, "Unsupported command: %d\n", (int) c);

		write_byte(fds[id].fd, WRONG_COMMAND);
		return -1;
	}

	return 1;
}

void close_conn(int id)
{
	_log(0, "Disconnected [%d] (%s: %s)\n",
			id, clients[id].host, clients[id].serv);

	if (clients[id].buf_fp != NULL)
	{
		if (unlink(clients[id].buf_file_path) < 0) 
			_log(0, "unlink() error: %s\n", strerror(errno));
		fclose(clients[id].buf_fp);
	}

	close(fds[id].fd);
	fds[id].fd = -1;

	if (clients[id].arch_fp != NULL)
		fclose(clients[id].arch_fp);

	memset(&clients[id], 0, sizeof(struct cli_t));

	update_users_file();
}

int update_users_file()
{
	FILE *fp = fopen(USERS_FILE_PATH, "w+");
	if (fp == NULL)
	{
		_log(0, "fopen() error: %s\n", strerror(errno));
		return -1;
	}

	int i;
	for (i = 1; i < MAX_CONN; i++) /* 0 for listener sock */
	{
		if (!clients[i].is_logged)
			continue;
		
		fprintf(fp, "%s:%s\n", clients[i].host, clients[i].serv);
	}

	fclose(fp);

	return 0;
}

void unmute_clients()
{
	int i;
	for (i = 1; i < MAX_CONN; i++) /* 0 for listener sock */
	{
		if (clients[i].is_logged || fds[i].fd == 0 || 
				clients[i].muted_since == 0)
		{
			continue;
		}

		if (time(NULL) - clients[i].muted_since < cfg.wrong_pwd_delay) 
			continue;

		clients[i].muted_since = 0;
		/* Ask for password */
		write_byte(fds[i].fd, PASSW); 
	}
}

void open_log_file()
{
	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%F=%H-%M-%S", t_tm);

	char log_filepath[PATH_MAX] = {0};
	snprintf(log_filepath, PATH_MAX, "%s/%s.log", cfg.log_dir, time_buf);

	if (freopen(log_filepath, "w", stderr) == NULL) { 
		/* output to stderr won't work */
		printf("freopen() error: %s\n", strerror(errno)); 
		exit(EXIT_FAILURE);
	}

	printf("strerr is redirected to log file: %s\n", log_filepath);
}

int open_buf_file(int id)
{
	assert(clients[id].buf_fp == NULL);

	snprintf(clients[id].buf_file_path, PATH_MAX, 
			"%s/%s:%s.txt", cfg.buf_dir, clients[id].host, clients[id].serv);

	clients[id].buf_fp = fopen(clients[id].buf_file_path, "w+");
	if (clients[id].buf_fp == NULL)
	{
		_log(0, "fopen() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	if (setvbuf(clients[id].buf_fp, NULL, _IOLBF, 0) < 0) {
		_log(0, "setvbuf() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	_log(id, "tmp file: %s\n", clients[id].buf_file_path);
}

int open_arch_file(int id)
{
	assert(clients[id].arch_fp == NULL);

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%G/%m-%b/%d", t_tm);

	char dir[PATH_MAX] = {0};
	snprintf(dir, PATH_MAX, "%s/%s", cfg.arch_dir, time_buf);

    errno = 0;
    if (_mkdir(dir, S_IRWXU) < 0) {
		_log(0, "_mkdir() error: %s\n", strerror(errno));
		return -1;
    }

	clients[id].prgrm_statr = t;
	memset(time_buf, 0, sizeof(time_buf));
	strftime(time_buf, 30, "%H:%M", t_tm);

	/* XXX: name should be unique */
	snprintf(clients[id].arch_filepath, PATH_MAX, 
			"%s/%s--%s:%s[%d].txt", 
			dir, 
			time_buf,
			clients[id].host, 
			clients[id].serv,
			id); 

	clients[id].arch_fp = fopen(clients[id].arch_filepath, "w+");
	if (NULL == clients[id].arch_fp)
	{
		_log(0, "fopen() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	if (setvbuf(clients[id].arch_fp, NULL, _IOLBF, 0) < 0) {
		_log(0, "setvbuf() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	_log(id, "archive file: %s\n", clients[id].arch_filepath);

	return 0;
}

int store_cc(int id, char *buf, size_t len)
{
	assert(buf != 0);
	assert(len > 0);

	if ((NULL == clients[id].buf_fp) && (open_buf_file(id) < 0))
		return -1;

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_EX)) {
		_log(0, "flock() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	fprintf(clients[id].buf_fp, "%d %d ", clients[id].cur_line, len);
	fwrite(buf, sizeof(char), len, clients[id].buf_fp);
	fprintf(clients[id].buf_fp, "\r\n");

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_UN)) {
		_log(0, "flock() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	clients[id].cur_line++;

	if ((NULL == clients[id].arch_fp) && (open_arch_file(id) < 0))
		return -1;

	fwrite(buf, sizeof(char), len, clients[id].arch_fp);
	fprintf(clients[id].arch_fp, "\r\n");

	return 0;
}
