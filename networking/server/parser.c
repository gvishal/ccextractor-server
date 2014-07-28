#include "utils.h"
#include "params.h"
#include "parser.h"
#include "networking.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>
#include <sys/file.h>
#include <limits.h> 

unsigned cli_id;

char *buf_filepath; 
FILE *buf_fp; 
unsigned buf_line_cnt;
unsigned cur_line;

char *txt_filepath; 
FILE *txt_fp; 
char *xds_filepath; 
FILE *xds_fp; 
/* char *srt_filepath;  */
/* FILE *srt_fp;  */

char *program_name;

pid_t fork_parser(unsigned id, const char *cce_output)
{
	pid_t pid = fork();

	if (pid < 0)
	{
		_log("fork() error: %s\n", strerror(errno));
		return -1;
	}
	else if (pid != 0)
	{
		c_log(cli_id, "Parser forked, pid=%d\n", pid);
		return pid;
	}

	cli_id = id;

	if (open_buf_file() < 0)
		exit(EXIT_FAILURE);

	if (parser_loop(cce_output) < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}

int parser_loop(const char *cce_output)
{
	FILE *fp = fopen(cce_output, "w+");
	if (NULL == fp)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
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

		if (parse_line(line, rc) < 0)
			return -1;
	}

	return 1;
}

int parse_line(const char *line, size_t len)
{
	int mode = CAPTIONS;
	if (is_xds(line))
		mode = XDS;
	else if (append_to_txt(line, len) < 0)
		return -1;

	if (append_to_xds(line, len) < 0)
		return -1;

	if (append_to_buf(line, len, mode) < 0)
		return -1;

	return 1;
}

int is_xds(const char *line)
{
	char *pipe; 
	if ((pipe = strchr(line, '|')) == NULL)
		return 0;
	if ((pipe = strchr(pipe + 1, '|')) == NULL)
		return 0;

	if (strncmp(pipe + 1, "XDS", 3) == 0)
		return 1;

	return 0;
}

int append_to_txt(const char *line, size_t len)
{
	if (txt_fp == NULL && open_txt_file() < 0)
		return -1;

	fwrite(line, sizeof(char), len, txt_fp);

	return 1;
}

int append_to_xds(const char *line, size_t len)
{
	if (xds_fp == NULL && open_xds_file() < 0)
		return -1;

	fwrite(line, sizeof(char), len, xds_fp);

	return 1;
}

int append_to_buf(const char *line, size_t len, char mode)
{
	char *tmp = nice_str(line, &len);
	if (NULL == tmp && line != NULL)
		return -1;

	if (0 == len)
	{
		free(tmp);
		return 1;
	}

	int rc; 
	if (buf_line_cnt >= cfg.buf_max_lines)
	{
		if ((rc = delete_n_lines(&buf_fp, buf_filepath,
					buf_line_cnt - cfg.buf_min_lines)) < 0)
		{
			e_log(rc);
			free(tmp);
			return -1;
		}
		buf_line_cnt = cfg.buf_min_lines;
	}

	if (0 != flock(fileno(buf_fp), LOCK_EX)) 
	{
		_log("flock() error: %s\n", strerror(errno));
		free(tmp);
		return -1;
	}

	fprintf(buf_fp, "%d %d ", cur_line, mode);

	if (tmp != NULL && len > 0)
	{
		fprintf(buf_fp, "%zd ", len);
		fwrite(tmp, sizeof(char), len, buf_fp);
	}

	fprintf(buf_fp, "\r\n");

	cur_line++;
	buf_line_cnt++;

	if (0 != flock(fileno(buf_fp), LOCK_UN))
	{
		_log("flock() error: %s\n", strerror(errno));
		free(tmp);
		return -1;
	}

	free(tmp);

	return 1;
}

int open_txt_file()
{
	assert(txt_fp == NULL);

new_name:
	if (file_path(&txt_filepath, "txt") < 0)
		return -1;

	if ((txt_fp = fopen(txt_filepath, "w+x")) == NULL)
	{
		if (EEXIST == errno)
			goto new_name;

		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	if (setvbuf(txt_fp, NULL, _IOLBF, 0) < 0) 
	{
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

int open_xds_file()
{
	assert(xds_fp == NULL);

new_name:
	if (file_path(&xds_filepath, "xds.txt") < 0)
		return -1;

	if ((xds_fp = fopen(xds_filepath, "w+")) == NULL)
	{
		if (EEXIST == errno)
			goto new_name;

		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	if (setvbuf(txt_fp, NULL, _IOLBF, 0) < 0) 
	{
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	return 1;
}

int open_buf_file()
{
	assert(buf_filepath == NULL);
	assert(buf_fp == NULL);

	if ((buf_filepath = (char *) malloc (PATH_MAX)) == NULL)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return -1;
	}

	snprintf(buf_filepath, PATH_MAX, "%s/%u.txt", cfg.buf_dir, cli_id);

	if ((buf_fp = fopen(buf_filepath, "w+")) == NULL)
	{
		_log("fopen() error: %s\n", strerror(errno));
		return -1;
	}

	if (setvbuf(buf_fp, NULL, _IOLBF, 0) < 0) 
	{
		_log("setvbuf() error: %s\n", strerror(errno));
		return -1;
	}

	c_log(cli_id, "Buffer file opened: %s\n", buf_filepath);

	return 1;
}


int file_path(char **path, const char *ext)
{
	assert(ext != NULL);

	if (NULL == *path && (*path = (char *) malloc(PATH_MAX)) == NULL) 
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
	strftime(time_buf, 30, "%H%M%S", t_tm);

	snprintf(*path, PATH_MAX, "%s/%s-%u.%s", dir, time_buf, cli_id, ext); 

	return 0;
}

/* int append_to_arch_info() */
/* { */
/* 	time_t t = time(NULL); */
/* 	struct tm *t_tm = localtime(&t); */
/*  */
/* 	char time_buf[30] = {0}; */
/* 	strftime(time_buf, 30, "%G/%m-%b/%d", t_tm); */
/*  */
/* 	char info_filepath[PATH_MAX]; */
/* 	snprintf(info_filepath, PATH_MAX, "%s/%s/%s",  */
/*     		cfg.arch_dir, */
/*     		time_buf, */
/*     		cfg.arch_info_filename); */
/*  */
/* 	FILE *info_fp = fopen(info_filepath, "a"); */
/* 	if (NULL == info_fp) */
/* 	{ */
/* 		_log("fopen() error: %s\n", strerror(errno)); */
/* 		return -1; */
/* 	} */
/*  */
/* 	fprintf(info_fp, "%d %u %s:%s %s %s %s\n", */
/* 			(int) t, */
/* 			cli->id, */
/* 			cli->host, */
/* 			cli->serv, */
/* 			cli_chld->bin_filepath, */
/* 			cli_chld->txt_filepath, */
/* 			cli_chld->srt_filepath); */
/*  */
/* 	fclose(info_fp); */
/*  */
/* 	return 0; */
/* } */
