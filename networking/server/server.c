#include "server.h"
#include "utils.h"
#include "networking.h"
#include "params.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <limits.h> 
#include <assert.h>
#include <errno.h>

#include <sys/file.h>
#include <sys/wait.h>

struct cli_t
{
	int fd;
	pid_t pid;

	char *host;
	char *serv;

	unsigned id;
} *cli;

size_t cli_cnt;
unsigned last_used_id;

struct cli_chld_t
{
	unsigned is_logged : 1;

	unsigned bin_mode : 1;
	char *bin_filepath;
	pid_t txt_parcer_pid;
	FILE *bin_fp;

	pid_t cce_txt_pid;
	char *txt_filepath;

	pid_t cce_srt_pid;
	char *srt_filepath;

	/* XXX move to different file */
	char *buf_file_path; 
	FILE *buf_fp; 
	/* Empty outside txt parser: */
	unsigned buf_line_cnt;
	int cur_line;
} *cli_chld; 

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

	_signal(SIGCHLD, sig_chld);

	cli = (struct cli_t *) malloc((sizeof(struct cli_t)) * (cfg.max_conn));
	if (NULL == cli)
	{
		_log("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	memset(cli, 0, (sizeof(struct cli_t)) * (cfg.max_conn));

	while (1)
	{
		struct sockaddr cliaddr;
		socklen_t clilen = sizeof(struct sockaddr);

		int connfd;
		if ((connfd = accept(listen_sd, &cliaddr, &clilen)) < 0) 
		{
			if (EINTR == errno)
			{
				continue;
			}
			else
			{
				_log("accept() error: %s\n", strerror(errno));
				goto end_server;
			}
		}

		if (cli_cnt >= cfg.max_conn)
		{
			write_byte(connfd, CONN_LIMIT);
			close(connfd);

			continue;
		}

		int id;
		if ((id = add_new_cli(connfd, &cliaddr, clilen)) < 0)
		{
			write_byte(connfd, ERROR);
			close_conn(cli_cnt - 1);

			continue;
		}

		if (cli_cnt == cfg.max_conn)
			_log("Connection limit reached, ignoring new connections\n");

		if ((cli[id].pid = fork()) < 0)
		{
			_log("fork() error: %s\n", strerror(errno));
			write_byte(connfd, ERROR);
			close_conn(id);
			continue;
		}
		else if (cli[id].pid > 0)
		{
			/* Parenet: */
			close(connfd);
			continue;
		}

		/* Child: */
		close(listen_sd);

		if (init_cli_chld(connfd, id) < 0)
		{
			write_byte(connfd, ERROR);
			exit(EXIT_FAILURE);
		}

		if (cfg.use_pwd)
			write_byte(connfd, PASSWORD);
		else if (cli_logged_in() < 0)
		{
			_log("[%d] (%s:%s) Disconnected\n",
					cli->id, cli->host, cli->serv);
			exit(EXIT_FAILURE);
		}

		if (cli_loop() < 0)
			exit(EXIT_FAILURE); 

		_log("[%d] (%s:%s) Disconnected\n",
				cli->id, cli->host, cli->serv);
		exit(EXIT_SUCCESS); 
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
		c_log(cli[id].id, "getnameinfo() error: %s\n", gai_strerror(rc));
		return -1;
	}

	int host_len = strlen(host) + 1; /* +1 for'\0' */
	cli[id].host = (char *) malloc(host_len); 
	if (NULL == cli[id].host)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}
	memcpy(cli[id].host, host, host_len);
	int serv_len = strlen(serv) + 1;
	cli[id].serv = (char *) malloc(serv_len);
	if (NULL == cli[id].serv)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}
	memcpy(cli[id].serv, serv, serv_len);

new_id:
	last_used_id++;
	if (0 == last_used_id)
		last_used_id = 1;
	for (unsigned i = 0; i < cfg.max_conn; i++)
		if (cli[i].id == last_used_id)
			goto new_id; 

	cli[id].id = last_used_id; /* TODO: save it to file */

	_log("Connected %s:%s\n", cli[id].host, cli[id].serv);

	if (update_users_file() < 0)
		return -1;

	return id;
}

int clinet_command()
{
	char c;
	int rc;
	size_t len = BUFFER_SIZE;
	char buf[BUFFER_SIZE] = {0};

	if (cli_chld->bin_mode)
	{
		assert(cli_chld->is_logged);

		assert(cli_chld->bin_fp != NULL);
		assert(cli_chld->bin_filepath != NULL);

		if ((rc = readn(cli->fd, buf, len)) <= 0)
		{
			if (rc < 0)
				c_log(cli->id, "readn() error: %s\n", strerror(errno));
			return rc;
		}

		c_log(cli->id, "Bin data received: %zd bytes\n", rc);

		fwrite(buf, sizeof(char), rc, cli_chld->bin_fp);
		fflush(cli_chld->bin_fp);

		return 1;
	}

	if ((rc = read_block(cli->fd, &c, buf, &len)) <= 0)
	{
		if (rc < 0)
			ec_log(cli->id, rc);
		return rc;
	}

	if (c != PASSWORD && !cli_chld->is_logged)
	{
		c_log(cli->id, "No password presented\n");
		return -1;
	}

	switch (c)
	{
	case PASSWORD:
		if (!cfg.use_pwd)
			return -1; /* We didn't ask for it */

		c_log(cli->id, "Password: %s\n", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			c_log(cli->id, "Wrong password\n");

			sleep(cfg.wrong_pwd_delay);

			if (write_byte(cli->fd, WRONG_PASSWORD) != 1)
				return -1;

			return 1;
		}

		c_log(cli->id, "Correct password\n");

		if (cli_logged_in() < 0)
			return -1;

		break;

	case BIN_MODE:
		c_log(cli->id, "Bin header\n");

		assert(cli_chld->txt_filepath == NULL);
		assert(cli_chld->bin_filepath == NULL);
		assert(cli_chld->bin_fp == NULL);

		cli_chld->bin_mode = TRUE;

		if (file_path(&cli_chld->bin_filepath, "bin") < 0)
			return -1;

		if ((cli_chld->bin_fp = fopen(cli_chld->bin_filepath, "w+")) == NULL)
		{
			_log("fopen() error: %s\n", strerror(errno));
			return -1;
		}

		if (file_path(&cli_chld->txt_filepath, "txt") < 0)
			return -1;

		if (file_path(&cli_chld->srt_filepath, "srt") < 0)
			return -1;

		if (fork_cce() < 0)
			return -1;

		if (append_to_arch_info() < 0)
			return -1;

		if (write_byte(cli->fd, OK) != 1)
			return -1;

		if (set_nonblocking(cli->fd) < 0)
		{
			_log("set_nonblocking() error: %s\n", strerror(errno));
			return -1;
		}

		if (open_buf_file() < 0)
		{
			return -1;
		}

		if (fork_txt_parser() < 0)
			return -1;

		break;

	default:
		c_log(cli->id, "Unsupported command: %d\n", (int) c);
		write_byte(cli->fd, UNKNOWN_COMMAND);

		return -1;
	}

	return 1;
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

	_log("[%d] (%s:%s) Disconnected\n",
			cli[id].id, cli[id].host, cli[id].serv);

	free_cli(id);
}

int update_users_file()
{
	FILE *fp = fopen(USERS_FILE_PATH, "w+");
	if (fp == NULL)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	/* XXX: client not logged in */
	for (unsigned i = 0; i < cli_cnt; i++) 
	{
		if (0 == cli[i].fd)
			continue;
		
		fprintf(fp, "%u %s:%s\n", 
				cli[i].id,
				cli[i].host, 
				cli[i].serv);
	}

	fclose(fp);

	return 0;
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

int open_buf_file()
{
	assert(cli_chld->buf_fp == NULL);
	assert(cli_chld->buf_file_path == NULL);

	if ((cli_chld->buf_file_path = (char *) malloc (PATH_MAX)) == NULL) 
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}

	snprintf(cli_chld->buf_file_path, PATH_MAX, 
			"%s/%u.txt", cfg.buf_dir, cli->id);

	cli_chld->buf_fp = fopen(cli_chld->buf_file_path, "w+");
	if (cli_chld->buf_fp == NULL)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	if (setvbuf(cli_chld->buf_fp, NULL, _IOLBF, 0) < 0) 
	{
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	c_log(cli->id, "Buffer file: %s\n", cli_chld->buf_file_path);

	return 0;
}

int send_to_buf(char command, char *buf, size_t len)
{
	assert(cli_chld->buf_fp != NULL);

	char *tmp = nice_str(buf, &len);
	if (NULL == tmp && buf != NULL)
			return -1;

	if (0 == len)
	{
		free(tmp);
		return 0;
	}

	int rc; 
	if (cli_chld->buf_line_cnt >= cfg.buf_max_lines)
	{
		if ((rc = delete_n_lines(&cli_chld->buf_fp, 
					cli_chld->buf_file_path,
					cli_chld->buf_line_cnt - cfg.buf_min_lines)) < 0)
		{
			e_log(rc);
			free(tmp);
			return -1;
		}
		cli_chld->buf_line_cnt = cfg.buf_min_lines;
	}

	if (0 != flock(fileno(cli_chld->buf_fp), LOCK_EX)) 
	{
		_log("flock() error: %s\n", strerror(errno));
		free(tmp);
		return -1;
	}

	fprintf(cli_chld->buf_fp, "%d %d ", 
			cli_chld->cur_line, command);

	if (tmp != NULL && len > 0)
	{
		fprintf(cli_chld->buf_fp, "%zd ", len);
		fwrite(tmp, sizeof(char), len, cli_chld->buf_fp);
	}

	fprintf(cli_chld->buf_fp, "\r\n");

	cli_chld->cur_line++;
	cli_chld->buf_line_cnt++;

	if (0 != flock(fileno(cli_chld->buf_fp), LOCK_UN))
	{
		_log("flock() error: %s\n", strerror(errno));
		free(tmp);
		return -1;
	}

	free(tmp);

	return 1;
}

int init_cli_chld(int fd, int id)
{
	cli_chld = (struct cli_chld_t *) malloc(sizeof(struct cli_chld_t));
	if (NULL == cli_chld)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}
	memset(cli_chld, 0, sizeof(struct cli_chld_t));

	if (id != 0)
		cli = &cli[id];

	cli->fd = fd;
	cli_chld->is_logged = FALSE;

	return 0;
}

int cli_logged_in()
{
	assert(cli_chld != NULL);

	_log("[%u] (%s:%s) Logged in\n",
			cli->id,
			cli->host, 
			cli->serv);

	cli_chld->is_logged = TRUE;

	if (write_byte(cli->fd, OK) != 1)
		return -1;

	return 0;
}

int cli_loop()
{
	struct pollfd pfd;
	pfd.fd = cli->fd;
	pfd.events = POLLIN;

	int ret = 1;

	while(1)
	{
		if (poll(&pfd, 1, -1) < 0)
		{
			if (EINTR == errno)
				continue;
			_log("poll() error: %s\n", strerror(errno));
			ret = -1;
			goto out;
		}

		if (pfd.revents & POLLHUP)
		{
			_log("[%d] (%s:%s) Disconnected\n",
					cli->id, cli->host, cli->serv);
			ret = 1;
			goto out;
		}

		if (pfd.revents & POLLERR) 
		{
			_log("poll() error: Revents %d\n", pfd.revents);
			ret = -1;
			goto out;
		}

		if ((ret = clinet_command()) <= 0)
			goto out;
	}

out:
	if (cli_chld->txt_parcer_pid > 0 && kill(cli_chld->txt_parcer_pid, SIGINT) < 0)
		_log("kill error(): %s\n", strerror(errno));

	if (cli_chld->cce_txt_pid > 0 && kill(cli_chld->cce_txt_pid, SIGINT) < 0)
		_log("kill error(): %s\n", strerror(errno));

	if (cli_chld->cce_srt_pid > 0 && kill(cli_chld->cce_srt_pid, SIGINT) < 0)
		_log("kill error(): %s\n", strerror(errno));

	if (cli_chld->buf_file_path != NULL)
	{
		if (unlink(cli_chld->buf_file_path) < 0)
			_log("unlink() error: %s\n", strerror(errno));
	}

	return ret;
}

int append_to_arch_info()
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

	fprintf(info_fp, "%d %u %s:%s %s %s %s\n",
			(int) t,
			cli->id,
			cli->host,
			cli->serv,
			cli_chld->bin_filepath,
			cli_chld->txt_filepath,
			cli_chld->srt_filepath);

	fclose(info_fp);

	return 0;
}

int file_path(char **path, const char *ext)
{
	assert(NULL == *path);
	assert(ext != NULL);

	if ((*path = (char *) malloc(PATH_MAX)) == NULL) 
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

	memset(time_buf, 0, sizeof(time_buf));
	strftime(time_buf, 30, "%H:%M:%S", t_tm);

	snprintf(*path, PATH_MAX, "%s/%s--%d.%s", dir, time_buf, cli->id, ext); 

	return 0;
}

int fork_cce()
{
	assert(cli_chld->bin_filepath != NULL);
	assert(cli_chld->txt_filepath != NULL);
	assert(cli_chld->srt_filepath != NULL);

	if ((cli_chld->cce_txt_pid = fork()) < 0)
	{
		_log("fork() error: %s\n", strerror(errno));
		return -1;
	}
	else if (cli_chld->cce_txt_pid == 0)
	{
		char *const v[] = 
		{
			"ccextractor", 
			"-in=bin", 
			cli_chld->bin_filepath,
			"--stream",
			"-quiet",
			"-out=ttxt",
			"-xds",
			"-autoprogram",
			"-o", cli_chld->txt_filepath,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli->id,
			"ttxt ccextractor forked, pid=%d\n", cli_chld->cce_txt_pid);

	if ((cli_chld->cce_srt_pid = fork()) < 0)
	{
		_log("fork() error: %s\n", strerror(errno));
		return -1;
	}
	else if (cli_chld->cce_srt_pid == 0)
	{
		char *const v[] = 
		{
			"ccextractor", 
			"-in=bin", 
			cli_chld->bin_filepath,
			"--stream",
			"-quiet",
			"-out=srt",
			"-autoprogram",
			"-o", cli_chld->srt_filepath,
			NULL
		};

		if (execv(cfg.cce_path, v) < 0) 
		{
			perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli->id,
			"srt ccextractor forked, pid=%d\n", cli_chld->cce_srt_pid);

	return 0;
}

int fork_txt_parser()
{
	assert(cli_chld->txt_filepath != NULL);
	assert(cli_chld->buf_fp != NULL);
	assert(cli_chld->buf_file_path != NULL);

	if ((cli_chld->txt_parcer_pid = fork()) < 0)
	{
		_log("fork() error: %s\n", strerror(errno));
		return -1;
	}
	else if (cli_chld->txt_parcer_pid != 0)
	{
		c_log(cli->id,
				"ttxt parcer forked, pid=%d\n", cli_chld->txt_parcer_pid);

		fclose(cli_chld->buf_fp);
		cli_chld->buf_fp = NULL;

		return 0;
	}

	/* Child: */

	FILE *fp = fopen(cli_chld->txt_filepath, "w+");
	if (NULL == fp)
	{
		_log("fopen() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	int rc;

	fpos_t pos;
	while (1)
	{
		if (fgetpos(fp, &pos) < 0)
		{
			_log("fgetpos() error: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if ((rc = getline(&line, &len, fp)) <= 0)
		{
			clearerr(fp);
			if (fsetpos(fp, &pos) < 0)
			{
				_log("fsetpos() error: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (nanosleep((struct timespec[]){{0, INF_READ_DELAY}}, NULL) < 0)
			{
				_log("nanosleep() error: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			continue;
		}

		char *pipe; 
		int mode = CAPTIONS;
		if ((pipe = strchr(line, '|')) == NULL)
			continue;
		if ((pipe = strchr(pipe + 1, '|')) == NULL)
			continue;
		pipe++;
		if (strstr(pipe, "XDS") == pipe)
			mode = XDS;

		if (send_to_buf(mode, line, rc) < 0)
			exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

void sig_chld()
{
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
