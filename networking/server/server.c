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

#define BUFFER_SIZE 20480

struct cli_t
{
	char *host;
	char *serv;

	unsigned is_logged : 1;

	unsigned unique_id;

	time_t muted_since;

	time_t program_start;
	char *program_name;
	unsigned program_id;

	char *buf_file_path;
	FILE *buf_fp;
	unsigned buf_line_cnt;
	int cur_line;

	char *arch_filepath;
	FILE *arch_fp;
} *clients; 

unsigned last_unique_id;

struct pollfd *fds;

/* int main(int argc, char *argv[]) */
int main()
{
	int rc;

	if ((rc = init_cfg()) < 0)
	{
		e_log(rc);
		exit(EXIT_FAILURE);
	}

	/* line buffering in stdout */
	if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) 
	{
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
	if (cfg.use_pwd)
		printf("Password: %s\n", cfg.pwd);

	if (cfg.create_logs)
		open_log_file();

	_signal(SIGALRM, unmute_clients);

	clients = (struct cli_t *) malloc((sizeof(struct cli_t)) * (cfg.max_conn + 1));
	/* +1 for listen socket*/
	if (NULL == clients)
	{
		_log("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	memset(clients, 0, (sizeof(struct cli_t)) * (cfg.max_conn + 1));

	fds = (struct pollfd *) malloc((sizeof(struct pollfd)) * (cfg.max_conn + 1));
	if (NULL == clients)
	{
		_log("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	fds[0].fd = listen_sd;
	fds[0].events = POLLIN;
	clients[0].is_logged = 1;
	nfds_t nfds = 1;

	int compress_array;

	while (1)
	{
		if (poll(fds, nfds, -1) < 0) 
		{ 
			if (EINTR == errno) 
				continue;

			_log("poll() error: %s\n", strerror(errno));
			goto end_server;
		}

		size_t current_size = nfds;
		for (size_t i = 0; i < current_size; i++) {
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
					if (nfds - 1 >= cfg.max_conn)
					{
						write_byte(new_sd, MAX_CONN);
						close(new_sd);

						continue;
					}

					fds[nfds].fd = new_sd;
					fds[nfds].events = POLLIN;

					if (add_new_cli(nfds, &cliaddr, clilen) < 0)
					{
						compress_array = TRUE;
						close_conn(i);
						continue;
					}

					if (cfg.use_pwd)
					{
						write_byte(new_sd, PASSW);
					}
					else if (cli_logged_in(nfds) < 0)
					{
						compress_array = TRUE;
						close_conn(nfds);
						continue;
					}

					nfds++;

					if (nfds - 1 == cfg.max_conn)
						_log("Connection limit reached, ignoring new connections\n");

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
			for (size_t i = 0; i < nfds; i++) {
				if (fds[i].fd >= 0)
					continue;

				for (size_t j = i; j < nfds - 1; j++)
				{
					fds[j].fd = fds[j + 1].fd;
					memcpy(&clients[j], &clients[j + 1], sizeof(struct cli_t));
					memset(&clients[j + 1], 0, sizeof(struct cli_t));
				}

				fds[nfds - 1].fd = -1;
				memset(&clients[nfds - 1], 0, sizeof(struct cli_t));

				i--;
				nfds--;
			}
		}
	}

end_server:
	for (size_t i = 0; i < nfds; i++)
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

	_log("Connected %s: %s\n", clients[id].host, clients[id].serv);

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
		if (rc < 0)
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
		if (!cfg.use_pwd)
			return -1; /* We didn't ask for it */

		c_log(clients[id].unique_id, "Password: %s\n", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			c_log(clients[id].unique_id, "Wrong password\n");

			clients[id].muted_since = time(NULL);

			alarm(cfg.wrong_pwd_delay);

			if (write_byte(fds[id].fd, WRONG_PASSW) != 1)
				return -1;

			return 0;
		}

		c_log(clients[id].unique_id, "Correct password\n");

		if (cli_logged_in(id) < 0)
			return -1;

		break;

	case CC:
		c_log(clients[id].unique_id, "Caption block, size: %d\n", len);

		if (store_cc(id, buf, len) < 0)
		{
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		break;

	case NEW_PRGM:
		for (char *p = buf; (size_t)(p - buf) <= len; p++) 
			if (*p == '\n' || *p == '\r')
				*p = ' '; /* TODO: delete them completely? */

		c_log(clients[id].unique_id, "New program: %s\n", buf);

		if (NULL == clients[id].buf_fp)
			open_buf_file(id);

		int was_null = TRUE;
		if (clients[id].program_name != NULL)
		{
			free(clients[id].program_name);
			clients[id].program_name = NULL;
			was_null = FALSE;
		}

		/* XXX: don't malloc, use memory from prev. */
		clients[id].program_name = (char *) malloc(len + 1); /* +1 for '\0' */
		if (NULL == clients[id].program_name) 
		{
			_log("malloc() error: %s\n", strerror(errno));
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}
		memcpy(clients[id].program_name, buf, len);
		clients[id].program_name[len] = '\0';
		clients[id].program_id++;

		char program_id_str[INT_LEN];
		snprintf(program_id_str, INT_LEN, "%u", clients[id].program_id);
		if (send_to_buf(id, PRGM_ID, program_id_str, INT_LEN) < 0)
		{
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		char com;
		if (was_null)
			com = NEW_PRGM;
		else
			com = RESET_PRGM;
		if (send_to_buf(id, com, buf, len) < 0)
		{
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		if (clients[id].arch_fp != NULL)
		{
			fclose(clients[id].arch_fp);
			clients[id].arch_fp = NULL;
		}

		if (clients[id].arch_filepath != NULL)
		{
			free(clients[id].arch_filepath);
			clients[id].arch_filepath = NULL;
		}

		if (open_arch_file(id) < 0)
		{
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		if (update_users_file() < 0)
		{
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

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
	c_log(clients[id].unique_id, "(%s:%s) Disconnected\n", 
			clients[id].host, clients[id].serv);

	if (clients[id].host != NULL)
		free(clients[id].host);
	if (clients[id].serv != NULL)
		free(clients[id].serv);

	if (clients[id].buf_fp != NULL)
	{
		send_to_buf(id, DISCONN, NULL, 0);

		fclose(clients[id].buf_fp);
		if (unlink(clients[id].buf_file_path) < 0) 
			_log("unlink() error: %s\n", strerror(errno));

		assert(clients[id].buf_file_path != NULL);
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

	for (unsigned i = 1; i < cfg.max_conn; i++) /* 0 for listener sock */
	{
		if (!clients[i].is_logged)
			continue;
		
		fprintf(fp, "%u %s:%s %d \"%s\"\n", 
				clients[i].unique_id,
				clients[i].host, 
				clients[i].serv,
				(int)clients[i].program_start,
				clients[i].program_name); /* should print (null) */
	}

	fclose(fp);

	return 0;
}

void unmute_clients()
{
	for (unsigned i = 1; i < cfg.max_conn; i++) /* 0 for listener sock */
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
	assert(clients[id].buf_file_path == NULL);

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
		return -1;
	}

	if (setvbuf(clients[id].buf_fp, NULL, _IOLBF, 0) < 0) 
	{
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	c_log(clients[id].unique_id, "Buffer file: %s\n", clients[id].buf_file_path);

	return 0;
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

	if (_mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) < 0)
	{
		_log("_mkdir() error: %s\n", strerror(errno));
		return -1;
	}

	clients[id].program_start = t;
	memset(time_buf, 0, sizeof(time_buf));
	strftime(time_buf, 30, "%H:%M:%S", t_tm);

	snprintf(clients[id].arch_filepath, PATH_MAX, "%s/%s--%d.txt", 
			dir, time_buf, clients[id].unique_id); 

	clients[id].arch_fp = fopen(clients[id].arch_filepath, "w+");
	if (NULL == clients[id].arch_fp)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	if (setvbuf(clients[id].arch_fp, NULL, _IOLBF, 0) < 0) {
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	if (append_to_arch_info(id) < 0)
		return -1;

	return 0;
}

int store_cc(int id, char *buf, size_t len)
{
	assert(buf != 0);
	assert(len > 0);

	if ((NULL == clients[id].buf_fp) && (open_buf_file(id) < 0))
		return -1;

	if (send_to_buf(id, CC, buf, len) < 0)
		return -1;

	if ((NULL == clients[id].arch_fp) && (open_arch_file(id) < 0))
		return -1;

	fwrite(buf, sizeof(char), len, clients[id].arch_fp);
	fprintf(clients[id].arch_fp, "\r\n");

	return 0;
}

int send_to_buf(int id, char command, char *buf, size_t len)
{
	assert(clients[id].buf_fp != NULL);

	int rc; 
	if (clients[id].buf_line_cnt >= cfg.buf_max_lines)
	{
		if ((rc = delete_n_lines(&clients[id].buf_fp, 
					clients[id].buf_file_path,
					clients[id].buf_line_cnt - cfg.buf_min_lines)) < 0)
		{
			e_log(rc);
			return -1;
		}
		clients[id].buf_line_cnt = cfg.buf_min_lines;
	}

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_EX)) 
	{
		_log("flock() error: %s\n", strerror(errno));
		return -1;
	}

	fprintf(clients[id].buf_fp, "%d %d ", 
			clients[id].cur_line, command);

	if (buf != NULL && len > 0)
	{
		fprintf(clients[id].buf_fp, "%zd ", len);
		fwrite(buf, sizeof(char), len, clients[id].buf_fp);
	}


	fprintf(clients[id].buf_fp, "\r\n");

	clients[id].cur_line++;
	clients[id].buf_line_cnt++;

	if (0 != flock(fileno(clients[id].buf_fp), LOCK_UN))
	{
		_log("flock() error: %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

int cli_logged_in(int id)
{
	clients[id].is_logged = 1;

new_id:
	last_unique_id++;
	if (0 == last_unique_id)
		last_unique_id = 1;
	for (unsigned i = 0; i < cfg.max_conn; i++)
	{
		if (!clients[i].is_logged)
			continue;

		if (clients[i].unique_id == last_unique_id)
			goto new_id; /* Yes, I like goto */
	}

	clients[id].unique_id = last_unique_id;

	_log("[%u] (%s:%s) Logged in\n",
			clients[id].unique_id, clients[id].host, clients[id].serv);

	update_users_file();

	if (write_byte(fds[id].fd, OK) != 1)
		return -1;

	return 0;
}

int append_to_arch_info(int id)
{
	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);

	char time_buf[30] = {0};
	strftime(time_buf, 30, "%G/%m-%b/%d", t_tm);

	char info_filepath[PATH_MAX];
	snprintf(info_filepath, PATH_MAX, "%s/%s/%s", 
    		cfg.arch_dir,
    		time_buf,
    		cfg.arch_info_filename);

	FILE *info_fp = fopen(info_filepath, "a");
	if (NULL == info_fp)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	fprintf(info_fp, "%d %u %s %s:%s %u \"%s\"\n",
			(int) t,
			clients[id].unique_id,
			clients[id].arch_filepath,
			clients[id].host,
			clients[id].serv,
			clients[id].program_id,
			clients[id].program_name); /* prints (null) when it's NULL */

	fclose(info_fp);

	return 0;
}
