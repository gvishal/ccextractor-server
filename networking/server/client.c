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
id_t cli_id = -1;
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
		logerr("fork");
		return -1;
	}
	else if (pid > 0)
	{
		logdebugmsg("Client forked, pid = %d (client: %s:%s)", pid, h, s);
		return pid;
	}

	close(listenfd);

	if (m_signal(SIGCHLD, sigchld_client) < 0)
		return -1;
	if (m_signal(SIGUSR1, kill_children) < 0)
		return -1;

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

	logclidebugmsg(cli_id, "Terminating client from fork_client()");
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
		logerr("write");
		return rc;
	}

	if (db_add_cli(host, serv, &cli_id) < 0)
		return -1;

	if (db_add_active_cli(cli_id) < 0)
		return -1;

	loginfomsg("%s:%s Logged in, id = %d", host, serv, cli_id);

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
				logcli(cli_id, "write_byte");
			return rc;
		}

		if ((rc = read_block(connfd, &c, buf, &len)) <= 0)
		{
			if (rc < 0)
				logcli_no(cli_id, "read_block", rc);
			return rc;
		}

		if (c != PASSWORD)
			return 0;

		logclimsg(cli_id, "Entered password: %s", buf);

		if (0 != strcmp(cfg.pwd, buf))
		{
			sleep(cfg.wrong_pwd_delay);

			logclimsg(cli_id, "Wrong password");

			if ((rc = write_byte(connfd, WRONG_PASSWORD)) <= 0)
			{
				if (rc < 0)
					logcli(cli_id, "write_byte");
				return rc;
			}

			continue;
		}

		logclimsg(cli_id, "Correct password");

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
				logcli_no(cli_id, "read_block", rc);
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
	logclimsg(cli_id, "Entered BIN mode");

	bin_mode = TRUE;

	int rc;
	if ((rc = write_byte(connfd, OK)) != 1)
	{
		if (rc < 0)
			logcli(cli_id, "write_byte");
		return rc;
	}

	if ((rc = read_bin_header()) <= 0)
		return rc;

	if (set_nonblocking(connfd) < 0)
	{
		logerr("set_nonblocking");
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
			logcli(cli_id, "readn");
		return rc;
	}

	if (memcmp(bin_header, "\xCC\xCC\xED", 3))
	{
		logclimsg(cli_id, "Wrong bin header: %02X%02X%02X",
				bin_header[0], bin_header[1], bin_header[2]);
		return 0;
	}

	logclimsg(cli_id, "Bin header recieved:");
	logclimsg(cli_id,
			"File created by %02X version: %02X%02X, format version: %02X%02X",
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

			logcli(cli_id, "poll");
			goto out;
		}

		if (fds[0].revents != 0)
		{
			if (fds[0].revents & POLLHUP)
			{
				ret = 0;
				goto out;
			}

			if (fds[0].revents & POLLERR) 
			{
				logclimsg(cli_id, "poll() error: Revents %d", fds[0].revents);
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
	if (0 == ret)
		logclimsg(cli_id, "Closed connection");

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
			logcli(cli_id, "readn");
		return rc;
	}

	/* logclidebugmsg(cli_id, "Bin data received: %zd bytes", rc); */

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
			logcli_no(cli_id, "read_block", rc);
		return -1;
	}

	if (c != PR_BIN_PATH)
	{
		logclimsg(cli_id, "Unexpected data in parser pipe");
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
		logerr("fork");
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
			"-autoprogram", /* XXX */
			"-latin1",
			"-o", cce_out.path,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			logerr("execv");
			exit(EXIT_FAILURE);
		}
	}

	logclidebugmsg(cli_id, "CCExtractor forked, pid = %d", pid);

	return pid;
}

int open_parser_pipe()
{
	int fds[2];
	if (pipe(fds) < 0)
	{
		logerr("pipe");
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
		logerr("fopen");
		return -1;
	}

	if (setvbuf(bin.fp, NULL, _IONBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	logclidebugmsg(cli_id, "BIN file opened: %s", bin.path);

	fwrite(bin_header, sizeof(char), BIN_HEADER_LEN, bin.fp);

	return 1;
}

int open_cce_input()
{
	assert(memcmp(bin_header, "\xCC\xCC\xED", 3) == 0);

	if ((cce_in.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		logerr("malloc");
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
		logerr("mkfifo");
		return -1;
	}

	if ((cce_in.fp = fopen(cce_in.path, "w+")) == NULL)
	{
		logerr("fopen");
		return -1;
	}

	if (setvbuf(cce_in.fp, NULL, _IONBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	fwrite(bin_header, sizeof(char), BIN_HEADER_LEN, cce_in.fp);

	logclidebugmsg(cli_id, "CCExtractor input fifo file: %s", cce_in.path);

	return 1;
}

int init_cce_output()
{
	if ((cce_out.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		logerr("malloc");
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%H%M%S", t_tm);
	snprintf(cce_out.path, PATH_MAX,
			"%s/%s-%u.cce.txt", cfg.cce_output_dir, time_buf, cli_id);

	logclidebugmsg(cli_id, "CCExtractor output file: %s", cce_out.path);

	return 1;
}

void cleanup()
{
	logclidebugmsg(cli_id, "cleanup() called");

	if (parser_pid > 0)
	{
		logclidebugmsg(cli_id, "Killing (SIGUSR2) parser (pid = %d)", parser_pid);
		if (kill(parser_pid, SIGUSR2) == 0)
			waitpid(parser_pid, NULL, 0);
		/* then sigchld_client() will be reached, which will set
		 * parser_pid to -1, */
	}

	if (cce_pid > 0)
	{
		logclidebugmsg(cli_id, "Killing (SIGINT) CCExtractor (pid = %d)", cce_pid);
		pid_t p = cce_pid;

		cce_pid = -1;
		if (kill(p, SIGINT) < 0)
			logerr("kill");
		else
			waitpid(p, NULL, 0);
	}

	if (cce_in.fp != NULL)
	{
		fclose(cce_in.fp);
		cce_in.fp = NULL;
	}

	if (cce_in.path != NULL)
	{
		logclidebugmsg(cli_id, "Deleting CCExtractor input %s", cce_in.path);

		if (unlink(cce_in.path) < 0)
			logerr("unlink");
		free(cce_in.path);
		cce_in.path = NULL;
	}

	if (cce_out.path != NULL)
	{
		logclidebugmsg(cli_id, "Deleting CCExtractor output %s", cce_out.path);

		if (unlink(cce_out.path) < 0)
			logerr("unlink");
		free(cce_out.path);
		cce_out.path = NULL;
	}

	if (cli_id > 0)
	{
		db_remove_active_cli(cli_id);
		db_close_conn();

		logclimsg(cli_id, "Disconnected");
		cli_id = 0;
	}
}

void sigchld_client()
{
	logclidebugmsg(cli_id, "SIGCHLD recieved");

	pid_t pid;
	int stat;
	int failed = FALSE;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
	{
		if (cce_pid == pid)
		{
			logclidebugmsg(cli_id, "CCExtractor (pid = %d) terminated", pid);
			cce_pid = -1;
		}
		else if (parser_pid == pid)
		{
			logclidebugmsg(cli_id, "Parser (pid = %d) terminated", pid);
			parser_pid = -1;
		}
		else
		{
			logclidebugmsg(cli_id, "pid %d terminated", pid);
		}

		if (!WIFEXITED(stat) || WEXITSTATUS(stat) != EXIT_SUCCESS)
			failed = TRUE;
	}

	cleanup();

	logclidebugmsg(cli_id, "Terminating client from SIGCHLD handler");

	if (failed)
	{
		write_byte(connfd, ERROR);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

void kill_children()
{
	logclidebugmsg(cli_id, "SIGUSR1 recieved");

	if (parser_pid > 0)
	{
		logclidebugmsg(cli_id, "Killing (SIGUSR1) parser (pid = %d)", parser_pid);
		id_t p = parser_pid; /* so it won't be killed again in sigchld_client */
		parser_pid = -1;

		if (kill(p, SIGUSR1) == 0)
			waitpid(p, NULL, 0);
	}

	if (cce_pid > 0)
	{
		logclidebugmsg(cli_id, "Killing (SIGINT) CCExtractor (pid = %d)", cce_pid);
		pid_t p = cce_pid;
		cce_pid = -1;
		if (kill(p, SIGINT) < 0)
			logerr("kill");
		else
			waitpid(p, NULL, 0);
	}

	logclidebugmsg(cli_id, "Terminating client from SIGUSR1 handler");
	exit(EXIT_SUCCESS);
}
