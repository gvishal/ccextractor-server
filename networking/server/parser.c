#include "utils.h"
#include "params.h"
#include "parser.h"
#include "networking.h"
#include "db.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>
#include <sys/file.h>
#include <limits.h> 

id_t cli_id;

file_t buf;
unsigned buf_line_cnt;
unsigned cur_line;

file_t txt;
file_t xds;

struct pr_t
{
	id_t id;
	char *name;
	char *dir;
	time_t start;
} cur_pr;

int pipe_w; /* pipe write end */

pid_t fork_parser(id_t id, const char *cce_output, int pipe)
{
	assert(cce_output != NULL);
	assert(pipe >= 0);

	pid_t pid = fork();

	if (pid < 0)
	{
		_perror("fork");
		return -1;
	}
	else if (pid != 0)
	{
		c_log(id, "Parser forked, pid = %d\n", pid);
		return pid;
	}

	cli_id = id;
	pipe_w = pipe;

	_signal(SIGINT, sigint_parser);

	if (open_buf_file() < 0)
		goto out;

	if (set_pr(NULL) < 0)
		goto out;

	if (parser_loop(cce_output) < 0)
		goto out;

out:
	cleanup_parser();

	exit(EXIT_FAILURE);
}

int parser_loop(const char *cce_output)
{
	FILE *fp = fopen(cce_output, "w+");
	if (NULL == fp)
	{
		_perror("fopen");
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
			_perror("fgetpos");
			return -1;
		}

		if ((rc = getline(&line, &len, fp)) <= 0)
		{
			clearerr(fp);
			if (fsetpos(fp, &pos) < 0)
			{
				_perror("fgetpos");
				return -1;
			}

			if (nanosleep((struct timespec[]){{0, INF_READ_DELAY}}, NULL) < 0)
			{
				_perror("nanosleep");
				return -1;
			}

			continue;
		}

		if (parse_line(line, rc) < 0)
			return -1;
	}

	return 1;
}

int parse_line(char *line, size_t len)
{
	int mode = CAPTIONS;
	char *rc;
	if ((rc = is_xds(line)) != NULL)
	{
		mode = XDS;
		if ((rc = is_program_changed(rc)) && set_pr(rc) < 0)
			return -1;
	}
	else if (append_to_txt(line, len) < 0)
		return -1;

	if (append_to_xds(line, len) < 0)
		return -1;

	if (append_to_buf(line, len, mode) < 0)
		return -1;

	return 1;
}

char *is_program_changed(char *line)
{
	static char *pr = "Program name: ";
	size_t len = strlen(pr);
	if (strncmp(line, pr, len) != 0)
		return NULL;

	char *name = line + len;
	len = strlen(name);

	char *nice_name = nice_str(name, &len);
	if (NULL == nice_name || 0 == len)
		return NULL;

	if (NULL == cur_pr.name || len != strlen(cur_pr.name))
		return nice_name;

#if 0
	if (strncmp(cur_pr.name, nice_name, len) == 0)
	{
		free(nice_name);
		return FALSE;
	}
#endif

	return nice_name;
}

char *is_xds(char *line)
{
	char *pipe; 
	if ((pipe = strchr(line, '|')) == NULL)
		return NULL;
	pipe++;
	if ((pipe = strchr(pipe, '|')) == NULL)
		return NULL;
	pipe++;

	static const char *s = "XDS";
	if (strncmp(pipe, s, 3) == 0)
		return pipe + 4; /* without '|' */

	return NULL;
}

int set_pr(char *new_name)
{
	c_log(cli_id, "Program changed: %s\n", new_name);

	int was_null = TRUE;

	if (NULL == cur_pr.name)
		free(cur_pr.name);
	else
		was_null = FALSE;

	if ((was_null) == (new_name == NULL)) /* TODO comment or something */
	{
		cur_pr.name = new_name;

		if (cur_pr.id > 0 && db_set_pr_endtime(cur_pr.id) < 0)
			return -1;

		if (creat_pr_dir(&cur_pr.dir, &cur_pr.start) < 0)
			return -1;

		if (db_add_program(cli_id, &cur_pr.id, cur_pr.start, cur_pr.name) < 0)
			return -1;

		if (db_set_pr_arctive_cli(cli_id, cur_pr.id) < 0)
			return -1;

		if (send_pr_to_parent() < 0)
			return -1;

		if (new_name != NULL && send_pr_to_buf() < 0)
			return -1;

		if (open_txt_file() < 0)
			return -1;

		if (open_xds_file() < 0)
			return -1;
	}

	if (was_null && new_name != NULL)
	{
		cur_pr.name = new_name;

		if (db_set_pr_name(cur_pr.id, cur_pr.name) < 0)
			return -1;
	}

	return 1;
}

int send_pr_to_parent()
{
	char *path = file_path(cur_pr.id, cur_pr.dir, "bin");
	if (NULL == path)
		return -1;

	int rc;
	if ((rc = write_block(pipe_w, PR_BIN_PATH, path, strlen(path))) < 0)
	{
		m_perror("write_block", rc);
		free(path);
		return -1;
	}

	free(path);

	return 1;
}

int send_pr_to_buf()
{
	char id_str[INT_LEN] = {0};
	sprintf(id_str, "%u", cur_pr.id);

	int rc;
	if ((rc = append_to_buf(id_str, INT_LEN, PROGRAM_ID)) < 0)
		return -1;

	int c = PROGRAM_CHANGED;
	if (0 == cur_pr.id)
		c = PROGRAM_NEW;

	if ((rc = append_to_buf(cur_pr.name, strlen(cur_pr.name), c)) < 0)
		return -1;

	return 1;
}

int append_to_txt(const char *line, size_t len)
{
	if (txt.fp == NULL && open_txt_file() < 0)
		return -1;

	fwrite(line, sizeof(char), len, txt.fp);

	return 1;
}

int append_to_xds(const char *line, size_t len)
{
	if (xds.fp == NULL && open_xds_file() < 0)
		return -1;

	fwrite(line, sizeof(char), len, xds.fp);

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
		if ((rc = delete_n_lines(&buf.fp, buf.path,
					buf_line_cnt - cfg.buf_min_lines)) < 0)
		{
			m_perror("delete_n_lines", rc);
			free(tmp);
			return -1;
		}
		buf_line_cnt = cfg.buf_min_lines;
	}

	if (0 != flock(fileno(buf.fp), LOCK_EX)) 
	{
		_perror("flock");
		free(tmp);
		return -1;
	}

	fprintf(buf.fp, "%d %d ", cur_line, mode);

	if (tmp != NULL && len > 0)
	{
		fprintf(buf.fp, "%zd ", len);
		fwrite(tmp, sizeof(char), len, buf.fp);
	}

	fprintf(buf.fp, "\r\n");

	cur_line++;
	buf_line_cnt++;

	if (0 != flock(fileno(buf.fp), LOCK_UN))
	{
		_perror("flock");
		free(tmp);
		return -1;
	}

	free(tmp);

	return 1;
}

int open_txt_file()
{
	assert(cur_pr.dir != NULL);
	assert(cur_pr.id > 0);

	if (txt.fp != NULL)
		fclose(txt.fp);

	if (txt.path != NULL)
		free(txt.path);

	if ((txt.path = file_path(cur_pr.id, cur_pr.dir, "txt")) == NULL)
		return -1;

	if ((txt.fp = fopen(txt.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(txt.fp, NULL, _IOLBF, 0) < 0) 
	{
		_perror("setvbuf");
		return -1;
	}

	c_log(cli_id, "TXT file: %s\n", txt.path);

	return 1;
}

int open_xds_file()
{
	assert(cur_pr.dir != NULL);
	assert(cur_pr.id > 0);

	if (xds.fp != NULL)
		fclose(xds.fp);

	if (xds.path != NULL)
		free(xds.path);

	if ((xds.path = file_path(cur_pr.id, cur_pr.dir, "xds.txt")) == NULL)
		return -1;

	if ((xds.fp = fopen(xds.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(xds.fp, NULL, _IOLBF, 0) < 0) 
	{
		_perror("setvbuf");
		return -1;
	}

	c_log(cli_id, "XDS file: %s\n", xds.path);

	return 1;
}

int open_buf_file()
{
	assert(buf.path == NULL);
	assert(buf.fp == NULL);

	if ((buf.path = (char *) malloc (PATH_MAX)) == NULL)
	{
		_perror("malloc");
		return -1;
	}

	snprintf(buf.path, PATH_MAX, "%s/%u.txt", cfg.buf_dir, cli_id);

	if ((buf.fp = fopen(buf.path, "w+")) == NULL)
	{
		_perror("fopen");
		return -1;
	}

	if (setvbuf(buf.fp, NULL, _IOLBF, 0) < 0) 
	{
		_perror("setvbuf");
		return -1;
	}

	c_log(cli_id, "Buffer file opened: %s\n", buf.path);

	return 1;
}

char *file_path(id_t pr_id, const char *dir, const char *ext)
{
	assert(pr_id > 0);
	assert(ext != NULL);

	char *ret = (char *) malloc(PATH_MAX);
	if (NULL == ret)
	{
		_perror("malloc");
		return NULL;
	}

	sprintf(ret, "%s/%u.%s", dir, pr_id, ext);

	return ret;
}

int creat_pr_dir(char **path, time_t *start)
{
	if (NULL == *path && (*path = (char *) malloc(PATH_MAX)) == NULL) 
	{
		_perror("malloc");
		return -1;
	}

	*start = time(NULL);
	struct tm *t_tm = localtime(start);
	char time_buf[30];
	strftime(time_buf, 30, "%G/%m-%b/%d", t_tm);

	snprintf(*path, PATH_MAX, "%s/%s", cfg.arch_dir, time_buf);

	if (_mkdir(*path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) < 0)
	{
		_perror("_mkdir");
		return -1;
	}

	c_log(cli_id, "Program dir: %s\n", *path);

	return 1;
}

void cleanup_parser()
{
	if (cur_pr.id > 0)
	{
		db_set_pr_endtime(cur_pr.id);
		cur_pr.id = 0;
	}

	if (buf.fp != NULL)
	{
		fclose(buf.fp);
		buf.fp = NULL;
	}

	if (buf.path != NULL)
	{
		if (unlink(buf.path) < 0)
			_perror("unlink");
		buf.path = NULL;
	}
}

void sigint_parser()
{
	/* c_log(cli_id, "sigint_parser() handler\n"); */
	cleanup_parser();

	exit(EXIT_SUCCESS);
}

