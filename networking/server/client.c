#include "utils.h"
#include "params.h"
#include "parser.h"
#include "networking.h"
#include "client.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <sys/stat.h>
#include <unistd.h>
#include <sys/file.h>
#include <limits.h> 


struct cli_t
{
	int fd;
	unsigned id;

	unsigned is_logged : 1;

	unsigned bin_mode : 1;
	char *bin_filepath;
	int fifo_fd;
	char *bin_fifo_filepath;
	FILE *bin_fp;
	pid_t parcer_pid;

	pid_t cce_pid;
	char *cce_output_file;
} *cli; 

pid_t fork_client(unsigned id, int connfd, int listenfd)
{
	pid_t pid = fork();

	if (pid < 0)
	{
		_perror("fork");
		return -1;
	}
	else if (pid > 0)
	{
		c_log(id, "Client forked, pid=%d\n", pid);
		return pid;
	}

	close(listenfd);

	if (init_cli() < 0)
		goto fail;

	cli->fd = connfd;
	cli->id = id;

	if (greeting() < 0)
		goto fail;

	if (handle_bin_mode() < 0)
		goto fail;

	if (bin_loop() < 0)
		goto fail;

fail:
	/* write_byte(connfd, ERROR); */
	exit(EXIT_FAILURE);
}

int init_cli()
{
	if ((cli = (struct cli_t *) malloc(sizeof(struct cli_t))) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	memset(cli, 0, sizeof(struct cli_t));
	cli->fd = -1;
	cli->fifo_fd = -1;

	return 1;
}

int greeting()
{
	if (cfg.use_pwd == TRUE && check_password() <= 0)
		return -1;

	cli->is_logged = TRUE;

	if (write_byte(cli->fd, OK) != 1)
		return -1;

	_log("[%u] Logged in\n", cli->id); /* TODO remove from here */

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
		if ((rc = write_byte(cli->fd, PASSWORD)) <= 0)
			return rc;

		if ((rc = read_block(cli->fd, &c, buf, &len)) <= 0)
		{
			c_perror(cli->id, "read_block", rc);
			return rc;
		}

		if (c != PASSWORD)
			return -1;

		c_log(cli->id, "Password: %s\n", buf);


		if (0 != strcmp(cfg.pwd, buf))
		{
			sleep(cfg.wrong_pwd_delay);

			c_log(cli->id, "Wrong password\n");

			if ((rc = write_byte(cli->fd, WRONG_PASSWORD)) <= 0)
				return rc;

			continue;
		}

		c_log(cli->id, "Correct password\n");

		return 1;
	}
}

int handle_bin_mode()
{
	char c;
	int rc;
	if ((rc = read_block(cli->fd, &c, NULL, NULL)) <= 0)
	{
		c_perror(cli->id, "read_block", rc);
		return -1;
	}

	if (c != BIN_MODE)
		return -1;

	c_log(cli->id, "Bin header\n");

	cli->bin_mode = TRUE;

	if (fork_cce() < 0)
		return -1;

	if ((cli->parcer_pid = fork_parser(cli->id, cli->cce_output_file)) < 0)
		return -1;

	if (set_nonblocking(cli->fd) < 0)
	{
		_perror("set_nonblocking");
		return -1;
	}

	if (write_byte(cli->fd, OK) != 1)
		return -1;

	return 1;
}

int bin_loop()
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

			_perror("poll");
			ret = -1;
			goto out;
		}

		if (pfd.revents & POLLHUP)
		{
			_log("[%d] Disconnected (poll)\n", cli->id); /* TODO remove from here */
			ret = 1;
			goto out;
		}

		if (pfd.revents & POLLERR) 
		{
			_log("poll() error: Revents %d\n", pfd.revents);
			ret = -1;
			goto out;
		}

		if ((ret = read_bin_data()) <= 0)
			goto out;
	}

out:
	if (cli->parcer_pid > 0 && kill(cli->parcer_pid, SIGINT) < 0)
		_perror("kill");

	if (cli->cce_pid > 0 && kill(cli->cce_pid, SIGINT) < 0)
		_perror("kill");

	return ret;
}

int read_bin_data()
{
	assert(cli->bin_mode);
	assert(cli->is_logged);
	assert(cli->bin_fp != NULL);
	assert(cli->bin_filepath != NULL);
	assert(cli->fifo_fd >= 0);
	assert(cli->cce_pid > 0);
	assert(cli->parcer_pid > 0);

	int rc;
	size_t len = BUFFER_SIZE;
	char buf[BUFFER_SIZE] = {0};

	if ((rc = readn(cli->fd, buf, len)) <= 0)
	{
		if (rc < 0)
			c_perror(cli->id, "readn", rc);
		return rc;
	}

	c_log(cli->id, "Bin data received: %zd bytes\n", rc);

	fwrite(buf, sizeof(char), rc, cli->bin_fp);
	fflush(cli->bin_fp);

	write(cli->fifo_fd, buf, rc);

	return 1;
}

int fork_cce()
{
	if (file_path(&cli->bin_filepath, "bin") < 0)
		return -1;

	if ((cli->bin_fp = fopen(cli->bin_filepath, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if ((cli->bin_fifo_filepath = (char *) malloc(PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%H%M%S", t_tm);
	snprintf(cli->bin_fifo_filepath, PATH_MAX, 
			"%s/%s-%u.fifo", "./cce", time_buf, cli->id); 

	if ((cli->fifo_fd = mkfifo(cli->bin_fifo_filepath, S_IRWXU)) < 0)
	{
		_perror("mkfifo");
		return -1;
	}

	if ((cli->cce_pid = fork()) < 0)
	{
		_perror("fork");
		return -1;
	}
	else if (cli->cce_pid == 0)
	{
		char *const v[] = 
		{
			"ccextractor", 
			"-in=bin", 
			cli->bin_fifo_filepath,
			"--stream",
			"-quiet",
			"-out=ttxt",
			"-xds",
			"-autoprogram",
			"-o", cli->cce_output_file,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			_perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli->id, "CCExtractor forked, pid=%d\n", cli->cce_pid);

	return 0;
}
