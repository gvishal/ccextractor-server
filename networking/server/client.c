#include "utils.h"
#include "params.h"
#include "parser.h"
#include "networking.h"
#include "client.h"
#include "db.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <limits.h>

int connfd;
id_t cli_id;
char *host;
char *serv;

unsigned is_logged;

unsigned bin_mode;

file_t bin; /* bin file for clients */

pid_t cce_pid;
file_t cce_in; /* Named pipe */
file_t cce_out;

pid_t parser_pid;
int parser_pipe_r; /* Read end */

char bin_header[BIN_HEADER_LEN];

pid_t fork_client(int fd, int listenfd, char *h, char *s)
{
	pid_t pid = fork();

	if (pid < 0)
	{
		_perror("fork");
		return -1;
	}
	else if (pid > 0)
	{
		c_log(0, "Client forked, pid = %d\n", pid); //TODO change id
		return pid;
	}

	close(listenfd);

	_signal(SIGCHLD, sigchld_client);
	/* _signal(SIGINT, cleanup); */

	connfd = fd;
	host = h;
	serv = s;

	int rc;
	if ((rc = db_conn()) < 0)
		goto out;

	if ((rc = greeting()) <= 0)
		goto out;

	if ((rc = ctrl_switch()) <= 0)
		goto out;

	if ((rc = bin_loop()) <= 0)
		goto out;

out:
	cleanup();

	if (rc < 0)
	{
		write_byte(connfd, ERROR);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

int greeting()
{
	int rc;
	if (cfg.use_pwd == TRUE && (rc = check_password()) <= 0)
		return rc;

	is_logged = TRUE;

	if ((rc = write_byte(connfd, OK)) <= 0)
	{
		if (rc < 0)
			_perror("write_byte");
		return rc;
	}

	if (db_add_cli(host, serv, &cli_id) < 0)
		return -1;

	if (db_add_active_cli(cli_id) < 0)
		return -1;

	_log("[%u] Logged in\n", cli_id);

	return 1;
}

int check_password()
{
	char c;
	int rc;
	size_t len = BUFFER_SIZE;
	char buf[BUFFER_SIZE];

	while(1)
	{
		if ((rc = write_byte(connfd, PASSWORD)) <= 0)
		{
			if (rc < 0)
				_perror("write_byte");
			return rc;
		}

		if ((rc = read_block(connfd, &c, buf, &len)) <= 0)
		{
			if (rc < 0)
				c_perror(cli_id, "read_block", rc);
			return rc;
		}

		if (c != PASSWORD)
			return 0;

		c_log(cli_id, "Password: %s\n", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			sleep(cfg.wrong_pwd_delay);

			c_log(cli_id, "Wrong password\n");

			if ((rc = write_byte(connfd, WRONG_PASSWORD)) <= 0)
			{
				if (rc < 0)
					_perror("write_byte");
				return rc;
			}

			continue;
		}

		c_log(cli_id, "Correct password\n");

		return 1;
	}
}

int ctrl_switch()
{
	char c;
	size_t len;
	char buf[BUFFER_SIZE];
	int rc = 1;
	char *desc;

	while (1)
	{
		len = BUFFER_SIZE;
		if ((rc = read_block(connfd, &c, buf, &len)) <= 0)
		{
			if (rc < 0)
				c_perror(cli_id, "read_block", rc);
			return rc;
		}

		switch (c)
		{
			case CC_DESC:
				desc = nice_str(buf, &len);

				rc = db_set_cc_desc(cli_id, desc);

				free(desc);

				if (rc < 0)
					return rc;
				break;
			case BIN_MODE:
				return handle_bin_mode();
			default:
				return 0;
		}
		continue;
	}

	return -1;
}

int handle_bin_mode()
{
	c_log(cli_id, "Bin mode\n");

	bin_mode = TRUE;

	int rc;
	if ((rc = write_byte(connfd, OK)) != 1)
	{
		if (rc < 0)
			_perror("write_byte");
		return rc;
	}

	if ((rc = read_bin_header()) <= 0)
		return rc;

	if (set_nonblocking(connfd) < 0)
	{
		_perror("set_nonblocking");
		return -1;
	}

	if ((cce_pid = fork_cce()) < 0)
		return -1;

	int pipe_w_end = open_parser_pipe();
	if (pipe_w_end < 0)
		return -1;

	if ((parser_pid = fork_parser(cli_id, cce_out.path, pipe_w_end)) < 0)
		return -1;

	return 1;
}

int read_bin_header()
{
	int rc;
	if ((rc = readn(connfd, bin_header, BIN_HEADER_LEN)) <= 0)
	{
		if (rc < 0)
			c_perror(cli_id, "readn", rc);
		return rc;
	}

	if (memcmp(bin_header, "\xCC\xCC\xED", 3))
	{
		c_log(cli_id, "Wrong bin header\n");
		return 0;
	}

	c_log(cli_id, "Bin header recieved:\n");
	c_log(cli_id,
			"File created by %02X version: %02X%02X, format version: %02X%02X\n",
			bin_header[3],
			bin_header[4], bin_header[5],
			bin_header[6], bin_header[7]);

	return 1;
}

int bin_loop()
{
	int ret = 0;

	if ((ret = read_parser_data()) < 0)
		goto out;

	if ((ret = open_bin_file()) < 0)
		goto out;

	struct pollfd fds[2];
	fds[0].fd = connfd;
	fds[0].events = POLLIN;
	fds[1].fd = parser_pipe_r;
	fds[1].events = POLLIN;
	nfds_t ndfs = 2;

	while(1)
	{
		if ((ret = poll(fds, ndfs, -1)) < 0)
		{
			if (EINTR == errno)
				continue;

			_perror("poll");
			goto out;
		}

		else if (fds[0].revents != 0)
		{
			if (fds[0].revents & POLLHUP)
			{
				ret = 0;
				goto out;
			}

			if (fds[0].revents & POLLERR) 
			{
				_log("poll() error: Revents %d\n", fds[0].revents);
				ret = -1;
				goto out;
			}

			if ((ret = read_bin_data()) <= 0)
				goto out;
		}

		if (fds[1].revents != 0)
		{
			if ((ret = read_parser_data()) < 0)
				goto out;

			if ((ret = open_bin_file()) < 0)
				goto out;
		}
	}

out:
	_log("[%d] Disconnected\n", cli_id);

	cleanup();

	return ret;
}

int read_bin_data()
{
	assert(bin_mode);
	assert(is_logged);
	assert(cce_in.fp != NULL);
	assert(cce_pid > 0);
	assert(parser_pid > 0);
	assert(bin.fp != NULL);

	int rc;
	size_t len = BUFFER_SIZE;
	char buf[BUFFER_SIZE] = {0};

	if ((rc = readn(connfd, buf, len)) <= 0)
	{
		if (rc < 0)
			c_perror(cli_id, "readn", rc);
		return rc;
	}

	/* c_log(cli_id, "Bin data received: %zd bytes\n", rc); */

	fwrite(buf, sizeof(char), rc, cce_in.fp);

	fwrite(buf, sizeof(char), rc, bin.fp);

	return 1;
}

int read_parser_data()
{
	char c;
	char buf[BUFFER_SIZE];
	size_t len = BUFFER_SIZE;
	int rc;
	if ((rc = read_block(parser_pipe_r, &c, buf, &len)) <= 0)
	{
		if (rc < 0)
			m_perror("read_block", rc);
		return -1;
	}

	if (c != PR_BIN_PATH)
	{
		_log("%s:%d Unexpected data in parser pipe\n", __FILE__, __LINE__);
		return -1;
	}

	if (bin.path != NULL)
		free(bin.path);
	bin.path = (char *) malloc(len + 1);
	memcpy(bin.path, buf, len);
	bin.path[len] = '\0';

	return 1;
}

pid_t fork_cce()
{
	if (open_cce_input() < 0)
		return -1;

	if (init_cce_output() < 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0)
	{
		_perror("fork");
		return -1;
	}
	else if (pid == 0)
	{
		char *const v[] = 
		{
			"ccextractor", 
			"-in=bin", 
			cce_in.path,
			"--stream",
			"-quiet",
			"-out=ttxt",
			"-xds",
			"-autoprogram", /* TODO */
			"-latin1",
			"-o", cce_out.path,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			_perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli_id, "CCExtractor forked, pid = %d\n", pid);

	return pid;
}

int open_parser_pipe()
{
	int fds[2];
	if (pipe(fds) < 0)
	{
		_perror("pipe");
		return -1;
	}

	parser_pipe_r = fds[0];
	return fds[1];
}

int open_bin_file()
{
	assert(bin.path != NULL);
	assert(memcmp(bin_header, "\xCC\xCC\xED", 3) == 0);

	if (bin.fp != NULL)
		fclose(bin.fp);

	if ((bin.fp = fopen(bin.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(bin.fp, NULL, _IONBF, 0) < 0)
	{
		_perror("setvbuf");
		return -1;
	}

	c_log(cli_id, "BIN file: %s\n", bin.path);

	fwrite(bin_header, sizeof(char), BIN_HEADER_LEN, bin.fp);

	return 1;
}

int open_cce_input()
{
	assert(memcmp(bin_header, "\xCC\xCC\xED", 3) == 0);

	if ((cce_in.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%H%M%S", t_tm);
	snprintf(cce_in.path, PATH_MAX,
			"%s/%s-%u.fifo", cfg.cce_output_dir, time_buf, cli_id);

	if (mkfifo(cce_in.path, S_IRWXU) < 0)
	{
		_perror("mkfifo");
		return -1;
	}

	if ((cce_in.fp = fopen(cce_in.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(cce_in.fp, NULL, _IONBF, 0) < 0)
	{
		_perror("setvbuf");
		return -1;
	}

	fwrite(bin_header, sizeof(char), BIN_HEADER_LEN, cce_in.fp);

	c_log(cli_id, "CCExtractor input fifo file: %s\n", cce_in.path);

	return 1;
}

int init_cce_output()
{
	if ((cce_out.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%H%M%S", t_tm);
	snprintf(cce_out.path, PATH_MAX,
			"%s/%s-%u.cce.txt", cfg.cce_output_dir, time_buf, cli_id);

	c_log(cli_id, "CCExtractor output file: %s\n", cce_out.path);

	return 1;
}

void cleanup()
{
	/* c_log(cli_id, "inside cleanup()\n"); */

	if (parser_pid > 0)
	{
		/* c_log(cli_id, "Killing parser (pid = %d)\n", parser_pid); */

		if (kill(parser_pid, SIGINT) < 0)
			_perror("kill");
		parser_pid = -1;

		waitpid(parser_pid, NULL, 0);
		/* then sigchld_client() will be reached */
		/* unless cleanup() is already called from sigchld_client() */
		/* e.g. ccextractor terminated */
		/* and following SIGCHLD of parser will be disarded */
	}

	if (cce_pid > 0)
	{
		if (kill(cce_pid, SIGINT) < 0)
			_perror("kill");
		cce_pid = -1;
	}

	if (cce_in.fp != NULL)
	{
		fclose(cce_in.fp);
		cce_in.fp = NULL;
	}

	if (cce_in.path != NULL)
	{
		if (unlink(cce_in.path) < 0)
			_perror("unlink");
		free(cce_in.path);
		cce_in.path = NULL;
	}

	if (cce_out.path != NULL)
	{
		if (unlink(cce_out.path) < 0)
			_perror("unlink");
		free(cce_out.path);
		cce_out.path = NULL;
	}

	if (cli_id > 0)
	{
		db_remove_active_cli(cli_id);
		cli_id = 0;
	}
}

void sigchld_client()
{
	/* c_log(cli_id, "inside sigchld_client() handler\n"); */
	pid_t pid;
	int stat;
	int failed = FALSE;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
	{
		if (cce_pid == pid)
		{
			c_log(cli_id, "CCExtractor (pid = %d) terminated\n", pid);
			cce_pid = -1;
		}
		else if (parser_pid == pid)
		{
			c_log(cli_id, "Parser (pid = %d) terminated\n", pid);
			parser_pid = -1;
		}
		else
		{
			c_log(cli_id, "pid %d terminated\n", pid);
		}

		if (!WIFEXITED(stat) || WEXITSTATUS(stat) != EXIT_SUCCESS)
			failed = TRUE;
	}

	cleanup();

	db_close_conn();

	_log("[%d] Disconnected\n", cli_id);

	if (failed)
	{
		write_byte(connfd, ERROR);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
