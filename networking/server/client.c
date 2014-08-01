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

char *program_name; /* not null-terminated */
id_t program_id;
size_t program_len;

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
		c_log(0, "Client forked, pid=%d\n", pid); //TODO change id
		return pid;
	}

	close(listenfd);

	connfd = fd;
	host = h;
	serv = s;

	if (greeting() < 0)
		goto fail;

	if (handle_bin_mode() < 0)
		goto fail;

	if (bin_loop() < 0)
		goto fail;

	exit(EXIT_SUCCESS);
fail:
	/* write_byte(connfd, ERROR); */
	exit(EXIT_FAILURE);
}

int greeting()
{
	if (cfg.use_pwd == TRUE && check_password() <= 0)
		return -1;

	is_logged = TRUE;

	if (write_byte(connfd, OK) != 1)
		return -1;

	if (db_add_cli(host, serv, &cli_id) < 0)
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
			return rc;

		if ((rc = read_block(connfd, &c, buf, &len)) <= 0)
		{
			c_perror(cli_id, "read_block", rc);
			return rc;
		}

		if (c != PASSWORD)
			return -1;

		c_log(cli_id, "Password: %s\n", buf);


		if (0 != strcmp(cfg.pwd, buf))
		{
			sleep(cfg.wrong_pwd_delay);

			c_log(cli_id, "Wrong password\n");

			if ((rc = write_byte(connfd, WRONG_PASSWORD)) <= 0)
				return rc;

			continue;
		}

		c_log(cli_id, "Correct password\n");

		return 1;
	}
}

int handle_bin_mode()
{
	char c;
	int rc;
	if ((rc = read_block(connfd, &c, NULL, NULL)) <= 0)
	{
		c_perror(cli_id, "read_block", rc);
		return -1;
	}

	if (c != BIN_MODE)
		return -1;

	c_log(cli_id, "Bin header\n");

	bin_mode = TRUE;

	if (open_bin_file() < 0)
		return -1;

	if ((cce_pid = fork_cce()) < 0)
		return -1;

	int pipe_w_end = open_parser_pipe();
	if (pipe_w_end < 0)
		return -1;

	if ((parser_pid = fork_parser(cli_id, cce_out.path, pipe_w_end)) < 0)
		return -1;

	if (set_nonblocking(connfd) < 0)
	{
		_perror("set_nonblocking");
		return -1;
	}

	if (write_byte(connfd, OK) != 1)
		return -1;

	return 1;
}

int bin_loop()
{
	struct pollfd fds[2];
	fds[0].fd = connfd;
	fds[0].events = POLLIN;
	fds[1].fd = parser_pipe_r;
	fds[1].events = POLLIN;
	nfds_t ndfs = 2;

	int ret = 1;

	while(1)
	{
		if (poll(fds, ndfs, -1) < 0)
		{
			if (EINTR == errno)
				continue;

			_perror("poll");
			ret = -1;
			goto out;
		}

		if (fds[0].revents != 0)
		{
			if (fds[0].revents & POLLHUP)
			{
				_log("[%d] Disconnected (poll)\n", cli_id); /* TODO remove from here */
				ret = 1;
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

		if (fds[1].revents != 0 && read_parser_data())
			goto out;

	}

out:
	logged_out();

	return ret;
}

int read_bin_data()
{
	assert(bin_mode);
	assert(is_logged);
	assert(bin.fp != NULL);
	assert(bin.path != NULL);
	assert(cce_in.fp != NULL);
	assert(cce_pid > 0);
	assert(parser_pid > 0);

	int rc;
	size_t len = BUFFER_SIZE;
	char buf[BUFFER_SIZE] = {0};

	if ((rc = readn(connfd, buf, len)) <= 0)
	{
		if (rc < 0)
			c_perror(cli_id, "readn", rc);
		return rc;
	}

	c_log(cli_id, "Bin data received: %zd bytes\n", rc);

	fwrite(buf, sizeof(char), rc, bin.fp);

	fwrite(buf, sizeof(char), rc, cce_in.fp);

	return 1;
}

int read_parser_data()
{
	char c;
	char buf[BUFFER_SIZE];
	size_t len = BUFFER_SIZE;
	int rc;
	if ((rc = read_block(parser_pipe_r, &c, buf, &len)) < 0)
	{
		m_perror("read_block", rc);
		return -1;
	}

	if (c == PROGRAM_ID)
	{
		assert(BUFFER_SIZE > INT_LEN);

		buf[len] = '\0';
		program_id = atoi(buf);
	}
	else if (c == PROGRAM_NEW || c == PROGRAM_CHANGED)
	{
		if (program_name != NULL)
			free(program_name);

		if ((program_name = (char *) malloc(len * sizeof(char))) == NULL)
		{
			_perror("malloc");
			return -1;
		}
		memcpy(program_name, buf, len);
		program_len = len;

		if (handle_program_change_cli() < 0)
			return -1;
	}

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
			"-autoprogram",
			"-o", cce_out.path,
			NULL
		};

		if (execv(cfg.cce_path , v) < 0) 
		{
			_perror("execv");
			exit(EXIT_FAILURE);
		}
	}

	c_log(cli_id, "CCExtractor forked, pid=%d\n", pid);

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
	if (file_path(&bin.path, "bin", cli_id, program_id) < 0)
		return -1;

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

	return 1;
}

int reopen_bin_file()
{
	assert(bin.fp != NULL);
	assert(bin.path != NULL);

	fclose(bin.fp);
	bin.fp = NULL;

	free(bin.path);
	bin.path = NULL;

	return open_bin_file();
}

int open_cce_input()
{
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

int handle_program_change_cli()
{
	if (reopen_bin_file() < 0)
		return -1;

	return 1;
}

void logged_out()
{
	_log("[%d] Disconnected\n", cli_id);

	if (parser_pid > 0 && kill(parser_pid, SIGINT) < 0)
		_perror("kill");

	if (cce_pid > 0 && kill(cce_pid, SIGINT) < 0)
		_perror("kill");

	if (cce_in.fp != NULL)
		fclose(cce_in.fp);

	if (cce_in.path != NULL && unlink(cce_in.path) < 0)
		_perror("unlink");

	if (cce_out.path != NULL && unlink(cce_out.path) < 0)
		_perror("unlink");

	remove_cli_info();
}

void remove_cli_info()
{
	FILE *fp = fopen(USERS_FILE_PATH, "r+");
	if (fp == NULL)
	{
		_perror("fopen");
		return;
	}

	if (0 != flock(fileno(fp), LOCK_EX))
	{
		_perror("flock");
		fclose(fp);
		return;
	}

	id_t *ids = (id_t *) malloc(cfg.max_conn * sizeof(id_t));
	if (NULL == ids)
	{
		_perror("malloc");
		fclose(fp);
		return;
	}

	int i;
	for (i = 0; !feof(fp); i++)
		fscanf(fp, "%u ", &ids[i]);

	rewind(fp);

	if (ftruncate(fileno(fp), 0) < 0)
	{
		_perror("ftruncate");
		return;
	}

	for (int j = 0; j < i; j++) {
		if (ids[j] != cli_id)
			fprintf(fp, "%u ", ids[j]);
	}

	if (0 != flock(fileno(fp), LOCK_UN))
	{
		_perror("flock");
		fclose(fp);
		free(ids);
		return;
	}

	free(ids);
	fclose(fp);
}
