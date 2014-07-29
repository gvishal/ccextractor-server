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

	file_t bin; /* bin file for clients */

	pid_t cce_pid;
	file_t cce_in; /* Named pipe */
	file_t cce_out;

	pid_t parcer_pid;
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

	return 1;
}

int greeting()
{
	if (cfg.use_pwd == TRUE && check_password() <= 0)
		return -1;

	cli->is_logged = TRUE;

	if (write_byte(cli->fd, OK) != 1)
		return -1;

	if (add_cli_info() < 0)
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

	if (open_bin_files() < 0)
		return -1;

	if ((cli->cce_pid = fork_cce()) < 0)
		return -1;

	if ((cli->parcer_pid = fork_parser(cli->id, cli->cce_out.path)) < 0)
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
	logged_out();

	return ret;
}

int read_bin_data()
{
	assert(cli->bin_mode);
	assert(cli->is_logged);
	assert(cli->bin.fp != NULL);
	assert(cli->bin.path != NULL);
	assert(cli->cce_in.fp != NULL);
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

	fwrite(buf, sizeof(char), rc, cli->bin.fp);

	fwrite(buf, sizeof(char), rc, cli->cce_in.fp);

	return 1;
}

pid_t fork_cce()
{
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
			cli->cce_in.path,
			"--stream",
			"-quiet",
			"-out=ttxt",
			"-xds",
			"-autoprogram",
			"-o", cli->cce_out.path,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			_perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli->id, "CCExtractor forked, pid=%d\n", pid);

	return pid;
}

int open_bin_files()
{
	if (file_path(&cli->bin.path, "bin", cli->id) < 0)
		return -1;

	if ((cli->bin.fp = fopen(cli->bin.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(cli->bin.fp, NULL, _IONBF, 0) < 0) 
	{
		_perror("setvbuf");
		return -1;
	}
	c_log(cli->id, "BIN file: %s\n", cli->bin.path);

	if ((cli->cce_in.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%H%M%S", t_tm);
	snprintf(cli->cce_in.path, PATH_MAX, 
			"%s/%s-%u.fifo", cfg.cce_output_dir, time_buf, cli->id); 

	if (mkfifo(cli->cce_in.path, S_IRWXU) < 0)
	{
		_perror("mkfifo");
		return -1;
	}

	if ((cli->cce_in.fp = fopen(cli->cce_in.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(cli->cce_in.fp, NULL, _IONBF, 0) < 0) 
	{
		_perror("setvbuf");
		return -1;
	}
	c_log(cli->id, "CCExtractor input fifo file: %s\n", cli->cce_in.path);


	if ((cli->cce_out.path = (char *) malloc(PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	snprintf(cli->cce_out.path, PATH_MAX, 
			"%s/%s-%u.cce.txt", cfg.cce_output_dir, time_buf, cli->id); 

	c_log(cli->id, "CCExtractor output file: %s\n", cli->cce_out.path);

	return 1;
}

int add_cli_info()
{
	FILE *fp = fopen(USERS_FILE_PATH, "a");
	if (fp == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (0 != flock(fileno(fp), LOCK_EX)) 
	{
		_perror("flock");
		fclose(fp);
		return -1;
	}

	fprintf(fp, "%u ", cli->id);

	if (0 != flock(fileno(fp), LOCK_UN)) 
	{
		_perror("flock");
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 1;
}

int logged_out()
{
	_log("[%d] Disconnected\n", cli->id); 

	if (cli->parcer_pid > 0 && kill(cli->parcer_pid, SIGINT) < 0)
		_perror("kill");

	if (cli->cce_pid > 0 && kill(cli->cce_pid, SIGINT) < 0)
		_perror("kill");

	if (cli->cce_in.fp != NULL)
		fclose(cli->cce_in.fp);

	if (cli->cce_in.path != NULL && unlink(cli->cce_in.path) < 0)
		_perror("unlink");

	if (cli->cce_out.path != NULL && unlink(cli->cce_out.path) < 0)
		_perror("unlink");

	if (remove_cli_info() < 0)
		return -1;

	return 1;
}

int remove_cli_info()
{
	FILE *fp = fopen(USERS_FILE_PATH, "r+");
	if (fp == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (0 != flock(fileno(fp), LOCK_EX)) 
	{
		_perror("flock");
		fclose(fp);
		return -1;
	}

	unsigned *ids = (unsigned *) malloc(cfg.max_conn * sizeof(unsigned));
	if (NULL == ids)
	{
		_perror("malloc");
		fclose(fp);
		return -1;
	}

	int i;
	for (i = 0; !feof(fp); i++)
	{
		fscanf(fp, "%u ", &ids[i]);
		if (feof(fp))
			break;
	}

	rewind(fp);

	for (int j = 0; j < i; j++) {
		if (ids[j] != cli->id)
			fprintf(fp, "%u ", ids[j]);
	}

	if (0 != flock(fileno(fp), LOCK_UN)) 
	{
		_perror("flock");
		fclose(fp);
		free(ids);
		return -1;
	}

	free(ids);
	fclose(fp);

	return 1;
}
