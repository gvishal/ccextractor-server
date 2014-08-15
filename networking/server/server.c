#include "server.h"
#include "utils.h"
#include "networking.h"
#include "params.h"
#include "parser.h"
#include "client.h"
#include "db.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <limits.h> 
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/file.h>
#include <sys/wait.h>

struct cli_t
{
	int fd;
	pid_t pid;

	char *host;
	char *serv;
} *cli;

size_t cli_cnt;
id_t last_used_id;

int main()
{
	int rc;

	if ((rc = init_cfg()) < 0)
	{
		m_perror("init_cfg", rc);
		exit(EXIT_FAILURE);
	}

	if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) 
	{
		_perror("setvbuf");
		exit(EXIT_FAILURE);
	}

	if ((rc = parse_config_file()) < 0)
	{
		m_perror("parse_config_file", rc); /* TODO Nope */
		exit(EXIT_FAILURE);
	}

	if (set_tz() < 0)
		return -1;

	if (creat_dirs() < 0)
		return -1;

	if (init_db() < 0)
		exit(EXIT_FAILURE);

	int fam;
	int listen_sd = bind_server(cfg.port, &fam);
	if (listen_sd < 0) 
	{
		m_perror("bind_server", rc);
		exit(EXIT_FAILURE);
	}

	printf("Server is binded to %d\n", cfg.port);
	if (cfg.use_pwd)
		printf("Password: %s\n", cfg.pwd);

	if (cfg.create_logs)
		open_log_file();

	_signal(SIGCHLD, sigchld_server);
	_signal(SIGINT, cleanup_server);

	cli = (struct cli_t *) malloc((sizeof(struct cli_t)) * (cfg.max_conn));
	if (NULL == cli)
	{
		_perror("malloc");
		exit(EXIT_FAILURE);
	}
	memset(cli, 0, (sizeof(struct cli_t)) * (cfg.max_conn));

	while (1)
	{
		socklen_t clilen;
		if (AF_INET == fam)
			clilen = sizeof(struct sockaddr_in);
		else
			clilen = sizeof(struct sockaddr_in6);
		struct sockaddr *cliaddr = (struct sockaddr *) malloc(clilen);
		if (NULL == cliaddr)
		{
			_perror("malloc");
			goto end_server;
		}

		int connfd;
		if ((connfd = accept(listen_sd, cliaddr, &clilen)) < 0)
		{
			free(cliaddr);
			if (EINTR == errno)
			{
				continue;
			}
			else
			{
				_perror("accept");
				goto end_server;
			}
		}

		if (cli_cnt >= cfg.max_conn)
		{
			write_byte(connfd, CONN_LIMIT);
			close(connfd);
			free(cliaddr);

			continue;
		}

		int id;
		if ((id = add_new_cli(connfd, cliaddr, clilen)) < 0)
		{
			write_byte(connfd, ERROR);
			close_conn(cli_cnt - 1);
			free(cliaddr);

			continue;
		}

		free(cliaddr);

		if (cli_cnt == cfg.max_conn)
			_log("Connection limit reached, ignoring new connections\n");

		if ((cli[id].pid = fork_client(connfd, listen_sd, cli[id].host, cli[id].serv)) < 0)
		{
			write_byte(connfd, ERROR);
			close_conn(id);
			continue;
		}

		close(connfd);
	}

end_server:
	for (size_t i = 0; i < cli_cnt; i++)
	{
		if (cli[i].fd >= 0)
			close_conn(i);
	}

	return 0;
}

int add_new_cli(int fd, struct sockaddr *cliaddr, socklen_t clilen)
{
	int id = cli_cnt;
	cli_cnt++;

	assert(fd != 0);

	cli[id].fd = fd;

	assert(cli[id].host == NULL);
	assert(cli[id].serv == NULL);

	char host[NI_MAXHOST] = {0};
	char serv[NI_MAXSERV] = {0};

	int rc;
	if ((rc = getnameinfo(cliaddr, clilen, 
					host, sizeof(host), serv, sizeof(serv), 0)) != 0)
	{
		_log("getnameinfo() error: %s\n", gai_strerror(rc));
		return -1;
	}

	int host_len = strlen(host) + 1; /* +1 for'\0' */
	if ((cli[id].host = (char *) malloc(host_len)) == NULL)
	{
		_perror("malloc");
		return -1;
	}
	memcpy(cli[id].host, host, host_len);
	int serv_len = strlen(serv) + 1;
	if ((cli[id].serv = (char *) malloc(serv_len)) == NULL)
	{
		_perror("malloc");
		return -1;
	}
	memcpy(cli[id].serv, serv, serv_len);

	_log("Connected %s:%s\n", cli[id].host, cli[id].serv);

	return id;
}

void free_cli(int id)
{
	if (cli[id].host != NULL)
		free(cli[id].host);
	if (cli[id].serv != NULL)
		free(cli[id].serv);

	/* compress cli array */
	for (size_t j = id; j < cli_cnt - 1; j++)
	{
		memcpy(&cli[j], &cli[j + 1], sizeof(struct cli_t));
		memset(&cli[j + 1], 0, sizeof(struct cli_t));
		cli[j + 1].fd = -1;
	}

	memset(&cli[cli_cnt - 1], 0, sizeof(struct cli_t));
	cli[cli_cnt - 1].fd = -1;

	cli_cnt--;
}

void close_conn(int id)
{
	close(cli[id].fd);

	_log("%s:%s Disconnected\n", cli[id].host, cli[id].serv);

	free_cli(id);
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
		_perror("freopen");
		exit(EXIT_FAILURE);
	}

	printf("strerr is redirected to log file: %s\n", log_filepath);
}

void sigchld_server()
{
	/* c_log(-1, "sigchld_server() handler\n"); */
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
	{ 
		c_log(-1, "pid %d terminated\n", pid);
		for (unsigned i = 0; i < cli_cnt; i++) {
			if (cli[i].pid != pid)
				continue;

			free_cli(i);
		}
	}
}

void cleanup_server()
{
	for (size_t i = 0; i < cli_cnt; i++)
	{
		if (cli[i].fd < 0 || cli[i].pid <= 0)
			continue;

		id_t p = cli[i].pid;
		cli[i].pid = 0;

		if (kill(p, 0) < 0)
			continue;

		/* _log("sending SIGUSR1 to %d\n", p); */
		if (kill(p, SIGUSR1) < 0)
			_perror("kill");
		else
			waitpid(p, NULL, 0);
	}

	exit(EXIT_SUCCESS);
}
