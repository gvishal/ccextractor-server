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

struct cli_t
{
	char *host;
	char *serv;

	unsigned is_logged : 1;

	unsigned unique_id;

	time_t muted_since;
	time_t prgrm_statr;

	char *buf_file_path;
	FILE *buf_fp;
	int cur_line;

	char *arch_filepath;
	FILE *arch_fp;
} *clients; 

unsigned last_unique_id;

struct pollfd *fds;

int main(int argc, char *argv[])
{
	int rc;

	if ((rc = init_cfg()) < 0)
	{
		e_log(rc);
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
		/* _log("setvbuf() error: %s\n", strerror(errno)); */
		_log("setvbuf() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	if ((rc = parse_config_file()) < 0)
	{
		e_log(rc);
		exit(EXIT_FAILURE);
	}

	int listen_sd = bind_server(cfg.port);
	if (listen_sd < 0) 
	{
		e_log(listen_sd);
		exit(EXIT_FAILURE);
	}

	printf("Server is binded to %d\n", cfg.port);
	printf("Password: %s\n\n", cfg.pwd);

	if (cfg.create_logs)
		open_log_file();

	_signal(SIGALRM, unmute_clients);

	clients = (struct cli_t *) malloc((sizeof(struct cli_t)) * cfg.max_conn);
	if (NULL == clients)
	{
		_log("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	fds = (struct pollfd *) malloc((sizeof(struct pollfd)) * cfg.max_conn);
	if (NULL == clients)
	{
		_log("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

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

			_log("poll() error: %s\n", strerror(errno));
			goto end_server;
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
				_log("poll() error: Revents %d\n", fds[i].revents);
			}

			if (fds[i].fd == listen_sd) 
			{
				struct sockaddr cliaddr;
				socklen_t clilen = sizeof(struct sockaddr);

				int new_sd;
				while((new_sd = accept(listen_sd, &cliaddr, &clilen)) >= 0) 
				{
					fds[nfds].fd = new_sd;
					fds[nfds].events = POLLIN;

					if (add_new_cli(nfds, &cliaddr, clilen) < 0)
					{
						compress_array = TRUE;
						close_conn(i);
						continue;
					}

					nfds++;

					write_byte(new_sd, PASSW);
				}

				if (errno != EWOULDBLOCK) /* accepted all of connections */
				{
					_log("accept() error: %s\n", strerror(errno));
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
					memset(&clients[j + 1], 0, sizeof(struct cli_t));
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

int add_new_cli(int id, struct sockaddr *cliaddr, socklen_t clilen)
{
	assert(clients[id].host == NULL);
	assert(clients[id].serv == NULL);

	char host[NI_MAXHOST] = {0};
	char serv[NI_MAXSERV] = {0};

	int rc;
	if ((rc = getnameinfo(cliaddr, clilen, 
					host, sizeof(host), serv, sizeof(serv), 0)) != 0)
	{
		c_log(clients[id].unique_id, "getnameinfo() error: %s\n", gai_strerror(rc));
		return -1;
	}

	int host_len = strlen(host) + 1; /* +1 for'\0' */
	clients[id].host = (char *) malloc(host_len); 
	if (NULL == clients[id].host)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}
	memcpy(clients[id].host, host, host_len);

	int serv_len = strlen(serv) + 1;
	clients[id].serv = (char *) malloc(serv_len);
	if (NULL == clients[id].serv)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}
	memcpy(clients[id].serv, serv, serv_len);

	clients[id].is_logged = 0;

	_log("Connected [%d] (%s: %s)\n",
			clients[id].unique_id, clients[id].host, clients[id].serv);
}

int clinet_command(int id)
{
	char c;
	int rc;
	size_t len = BUFFER_SIZE;

	char buf[BUFFER_SIZE] = {0};
	if ((rc = read_block(fds[id].fd, &c, buf, &len)) <= 0)
	{
		ec_log(id, rc);
		return -1;
	}

	if (clients[id].muted_since > 0)
		return 0;

	if (c != PASSW && !clients[id].is_logged)
	{
		c_log(clients[id].unique_id, "No password presented\n");
		return -1;
	}

	switch (c)
	{
	case PASSW:
		c_log(clients[id].unique_id, "Password: %s\n", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			c_log(clients[id].unique_id, "Wrong password\n", buf);

			clients[id].muted_since = time(NULL);

			alarm(cfg.wrong_pwd_delay);

			if (write_byte(fds[id].fd, WRONG_PASSW) != 1)
				return -1;

			return 0;
		}

		c_log(clients[id].unique_id, "Correct password\n", buf);

		clients[id].is_logged = 1;

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

new_id:
		last_unique_id++;
		if (0 == last_unique_id)
			last_unique_id = 1;
		for (int i = 0; i < cfg.max_conn; i++) {
			if (!clients[i].is_logged)
				continue;

			if (clients[i].unique_id == last_unique_id)
				goto new_id; /* Yes, I like goto */
		}

		clients[id].unique_id = last_unique_id;

		update_users_file();
		break;

	case CC:
		c_log(clients[id].unique_id, "Caption block, size: %d\n", len);

		if (store_cc(id, buf, len) < 0)
			return -1;

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		break;

	default:
		c_log(clients[id].unique_id, "Unsupported command: %d\n", (int) c);

		write_byte(fds[id].fd, WRONG_COMMAND);
		return -1;
	}

	return 1;
}

void close_conn(int id)
{
	_log("Disconnected [%d] (%s: %s)\n",
			clients[id].unique_id, clients[id].host, clients[id].serv);

	if (clients[id].host != NULL)
		free(clients[id].host);
	if (clients[id].serv != NULL)
		free(clients[id].serv);

	if (clients[id].buf_fp != NULL)
	{
		assert(clients[id].buf_file_path != NULL);

		fclose(clients[id].buf_fp);
		if (unlink(clients[id].buf_file_path) < 0) 
			_log("unlink() error: %s\n", strerror(errno));

		free(clients[id].buf_file_path);
	}

	if (clients[id].arch_fp != NULL)
	{
		fclose(clients[id].arch_fp);
		assert(clients[id].arch_filepath != NULL);
		free(clients[id].arch_filepath);
	}

	close(fds[id].fd);
	fds[id].fd = -1;

	memset(&clients[id], 0, sizeof(struct cli_t));

	update_users_file();
}

int update_users_file()
{
	FILE *fp = fopen(USERS_FILE_PATH, "w+");
	if (fp == NULL)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	int i;
	for (i = 1; i < cfg.max_conn; i++) /* 0 for listener sock */
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
	for (i = 1; i < cfg.max_conn; i++) /* 0 for listener sock */
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
	assert(clients[id].arch_filepath == NULL);

	if ((clients[id].buf_file_path = (char *) malloc (PATH_MAX)) == NULL) 
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}

	snprintf(clients[id].buf_file_path, PATH_MAX, 
			"%s/%u.txt", cfg.buf_dir, clients[id].unique_id);

	clients[id].buf_fp = fopen(clients[id].buf_file_path, "w+");
	if (clients[id].buf_fp == NULL)
	{
		_log("fopen() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	if (setvbuf(clients[id].buf_fp, NULL, _IOLBF, 0) < 0) {
		_log("setvbuf() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	c_log(clients[id].unique_id, "Buffer file: %s\n", clients[id].buf_file_path);
}

int open_arch_file(int id)
{
	assert(clients[id].arch_fp == NULL);
	assert(clients[id].arch_filepath == NULL);

	if ((clients[id].arch_filepath = (char *) malloc (PATH_MAX)) == NULL) 
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%G/%m-%b/%d", t_tm);

	char dir[PATH_MAX] = {0};
	snprintf(dir, PATH_MAX, "%s/%s", cfg.arch_dir, time_buf);

    errno = 0;
    if (_mkdir(dir, S_IRWXU) < 0) {
		_log("_mkdir() error: %s\n", strerror(errno));
		return -1;
    }

	clients[id].prgrm_statr = t;
	memset(time_buf, 0, sizeof(time_buf));
	strftime(time_buf, 30, "%H:%M", t_tm);

	snprintf(clients[id].arch_filepath, PATH_MAX, 
			"%s/%s--%s:%s[%d].txt", 
			dir, 
			time_buf,
			clients[id].host, 
			clients[id].serv,
			clients[id].unique_id); 

	clients[id].arch_fp = fopen(clients[id].arch_filepath, "w+");
	if (NULL == clients[id].arch_fp)
	{
		_log("fopen() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	if (setvbuf(clients[id].arch_fp, NULL, _IOLBF, 0) < 0) {
		_log("setvbuf() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	c_log(clients[id].unique_id, "archive file: %s\n", clients[id].arch_filepath);

	return 0;
}

int store_cc(int id, char *buf, size_t len)
{
	assert(buf != 0);
	assert(len > 0);

	if ((NULL == clients[id].buf_fp) && (open_buf_file(id) < 0))
		return -1;

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_EX)) {
		_log("flock() error: %s\n", strerror(errno));
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	fprintf(clients[id].buf_fp, "%d %d ", clients[id].cur_line, len);
	fwrite(buf, sizeof(char), len, clients[id].buf_fp);
	fprintf(clients[id].buf_fp, "\r\n");

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_UN)) {
		_log("flock() error: %s\n", strerror(errno));
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
