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
unsigned cur_line = 1;

file_t txt;
file_t xds;
file_t srt;

int sigusr2_received;

struct pr_t
{
	id_t id;
	char *name;
	char *dir;
	time_t start;
	time_t report_time;
} cur_pr;

int pipe_w; /* pipe write end */

pid_t fork_parser(id_t id, const char *cce_output, int pipe)
{
	assert(cce_output != NULL);
	assert(pipe >= 0);

	pid_t pid = fork();

	if (pid < 0)
	{
		logerr("fork");
		return -1;
	}
	else if (pid != 0)
	{
		logclidebugmsg(id, "Parser forked, pid = %d", pid);
		return pid;
	}

	cli_id = id;
	pipe_w = pipe;

	if (m_signal(SIGUSR1, sigusr1_parser) < 0)
		return -1;
	if (m_signal(SIGUSR2, sigusr2_parser) < 0)
		return -1;

	if (open_buf_file() < 0)
		goto out;

	if (set_pr(NULL) < 0)
		goto out;

	if (parser_loop(cce_output) < 0)
		goto out;

out:
	cleanup_parser();

	logclidebugmsg(cli_id, "Terminating parser on error");
	exit(EXIT_FAILURE);
}

int parser_loop(const char *cce_output)
{
	FILE *fp = fopen(cce_output, "w+");
	if (NULL == fp)
	{
		logerr("fopen");
		return -1;
	}

	char *line = NULL;
	size_t len = 0;
	int rc = 1;
	int nread = 0;

	fpos_t pos;
	while (1)
	{
		if ((rc = fgetpos(fp, &pos)) < 0)
		{
			logerr("fgetpos");
			goto out;
		}

		if ((nread = getline(&line, &len, fp)) <= 0)
		{
			if (sigusr2_received)
			{
				cleanup_parser();
				logclidebugmsg(cli_id, "Terminating parser from parser_loop()");
				exit(EXIT_SUCCESS);
			}

			clearerr(fp);
			if ((rc = fsetpos(fp, &pos)) < 0)
			{
				logerr("fgetpos");
				goto out;
			}

			rc = nanosleep((struct timespec[]){{0, INF_READ_DELAY}}, NULL);
			if (rc < 0)
			{
				if (EINTR == errno)
					continue;

				logerr("nanosleep");
				goto out;
			}

			continue;
		}

		if ((rc = parse_line(line, nread)) < 0)
			goto out;

		if ((rc = check_pr_timout()) < 0)
			goto out;
	}

out:
	fclose(fp);
	return rc;
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
	else
	{
		if (db_store_cc(line, len) < 0)
			return -1;

		if (append_to_txt(line) < 0)
			return -1;

		if (append_to_srt(line) < 0)
			return -1;
	}

	if (append_to_xds(line) < 0)
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
	if (NULL == nice_name)
		return NULL;
	if (0 == len)
	{
		free(nice_name);
		return NULL;
	}

	if (NULL == cur_pr.name || len != strlen(cur_pr.name))
		return nice_name;

	if (strncmp(cur_pr.name, nice_name, len) == 0)
	{
		cur_pr.report_time = time(NULL);
		free(nice_name);
		return NULL;
	}

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
	logclimsg(cli_id, "Program changed: %s", new_name);
	/* TODO print reason (timeout or xds) */

	int was_null = TRUE;

	if (cur_pr.name != NULL)
	{
		free(cur_pr.name);
		was_null = FALSE;
	}

	time_t now = time(NULL);
	if (was_null && new_name != NULL
			&& now - cur_pr.report_time < cfg.pr_report_time)
	{
		cur_pr.name = new_name;

		if (db_set_pr_name(cur_pr.id, cur_pr.name) < 0)
			return -1;

		if (send_pr_to_buf(FALSE) < 0)
			return -1;
	}
	else
	{
		cur_pr.name = new_name;
		cur_pr.start = now;
		cur_pr.report_time = now;

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

		if (open_txt_file() < 0)
			return -1;

		if (open_xds_file() < 0)
			return -1;

		if (open_srt_file() < 0)
			return -1;

		if (send_pr_to_buf(TRUE) < 0)
			return -1;
	}

	return 1;
}

int check_pr_timout()
{
	time_t now = time(NULL);

	if (cur_pr.name != NULL)
	{
		if (now - cur_pr.report_time >= cfg.pr_report_time)
			return set_pr(NULL);
	}
	else
	{
		if (now - cur_pr.start >= cfg.pr_timeout)
			return set_pr(NULL);
	}

	return 1;
}

int db_store_cc(char *line, size_t len)
{
	char *pipe; 
	if ((pipe = strchr(line, '|')) == NULL)
		return 1;
	pipe++;
	if ((pipe = strchr(pipe, '|')) == NULL)
		return 1;
	pipe++;

	static const char *s = "XDS";
	if (strncmp(pipe, s, 3) == 0)
		return 1;
	pipe += 4; /* without '|' */

	while (' ' == *pipe) pipe++;

	len -= (pipe - line) + 1; /* +1 for \n */
	char *cc = nice_str(pipe, &len);
	if (NULL == cc) 
		return 1;
	if (0 == len) {
		free(cc);
		return 1;
	}

	cc[len] = '\n';

	int rc = db_append_cc(cur_pr.id, cc, len + 1);
	free(cc);

	return rc;
}

int send_pr_to_parent()
{
	char *path = file_path(cur_pr.id, cur_pr.dir, "bin");
	if (NULL == path)
		return -1;

	int rc;
	if ((rc = write_block(pipe_w, PR_BIN_PATH, path, strlen(path))) < 0)
	{
		logcli_no(cli_id, "write_block", rc);
		free(path);
		return -1;
	}

	free(path);

	return 1;
}

int send_pr_to_buf(int is_changed)
{
	char id_str[INT_LEN] = {0};
	sprintf(id_str, "%u", cur_pr.id);

	int rc;
	if ((rc = append_to_buf(id_str, INT_LEN, PROGRAM_ID)) < 0)
		return -1;

	char time_str[INT_LEN] = {0};
	sprintf(time_str, "%u", (unsigned)cur_pr.start);

	/* TODO: use returned value from sprintf instead INT_LEN */
	if ((rc = append_to_buf(time_str, INT_LEN, PROGRAM_DIR)) < 0)
		return -1;

	static int first_run = TRUE;
	int c = PROGRAM_NEW;
	if (is_changed && !first_run)
		c = PROGRAM_CHANGED;
	first_run = FALSE;

	char *s = "Unknown";
	if (cur_pr.name != NULL)
		s = cur_pr.name;

	if ((rc = append_to_buf(s, strlen(s), c)) < 0)
		return -1;

	return 1;
}

int append_to_txt(const char *line)
{
	if (txt.fp == NULL && open_txt_file() < 0)
		return -1;

	fprintf(txt.fp, "%s", line);

	return 1;
}

int append_to_xds(const char *line)
{
	if (xds.fp == NULL && open_xds_file() < 0)
		return -1;

	fprintf(xds.fp, "%s", line);

	return 1;
}

int append_to_srt(const char *line)
{
	if (srt.fp == NULL && open_srt_file() < 0)
		return -1;

#define T_LEN 30 /* lol */
	static char start[T_LEN] = {0};
	static char end[T_LEN] = {0};
	static char cc_buf[BUFFER_SIZE] = {0};
	static char *buf_end = cc_buf;
	static int cc_blk = 1;

	const char *pipe;
	if ((pipe = strchr(line, '|')) == NULL)
		return 1;

	const char *cur_st = line;
	size_t cur_st_len = pipe - line;
	const char *cur_en = pipe + 1;

	if ((pipe = strchr(cur_en, '|')) == NULL)
		return 1;
	size_t cur_en_len = pipe - cur_en;

	if ((pipe = strchr(pipe + 1, '|')) == NULL)
		return 1;

	const char *cur_cc = pipe + 1;

	if (strncmp(cur_st, start, cur_st_len) != 0)
	{
		if (start[0] != 0)
		{
			fprintf(srt.fp, "%d\n%s --> %s\n%s\n", cc_blk, start, end, cc_buf);
			cc_blk++;
		}
		buf_end = cc_buf;
		*buf_end = '\0';

		memcpy(start, cur_st, cur_st_len);
		memcpy(end, cur_en, cur_en_len);
	}

	buf_end += strmov(buf_end, cur_cc);

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

	if (buf_line_cnt >= cfg.buf_max_lines)
	{
		if (delete_n_lines(&buf.fp, buf.path, buf_line_cnt - cfg.buf_min_lines) < 0)
		{
			free(tmp);
			return -1;
		}
		buf_line_cnt = cfg.buf_min_lines;
	}

	if (0 != flock(fileno(buf.fp), LOCK_EX)) 
	{
		logerr("flock");
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
		logerr("flock");
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
		logerr("fopen");
		return -1;
	}

	if (setvbuf(txt.fp, NULL, _IOLBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	logclidebugmsg(cli_id, "Opened TXT output file: %s", txt.path);

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
		logerr("fopen");
		return -1;
	}

	if (setvbuf(xds.fp, NULL, _IOLBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	logclidebugmsg(cli_id, "Opened XDS output file: %s", xds.path);

	return 1;
}

int open_srt_file()
{
	assert(cur_pr.dir != NULL);
	assert(cur_pr.id > 0);

	if (srt.fp != NULL)
		fclose(srt.fp);

	if (srt.path != NULL)
		free(srt.path);

	if ((srt.path = file_path(cur_pr.id, cur_pr.dir, "srt")) == NULL)
		return -1;

	if ((srt.fp = fopen(srt.path, "w+")) == NULL)
	{
		logerr("fopen");
		return -1;
	}

	if (setvbuf(srt.fp, NULL, _IOLBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	logclidebugmsg(cli_id, "Opened SRT output file: %s", srt.path);

	return 1;
}

int open_buf_file()
{
	assert(buf.path == NULL);
	assert(buf.fp == NULL);

	if ((buf.path = (char *) malloc (PATH_MAX)) == NULL)
	{
		logerr("malloc");
		return -1;
	}

	snprintf(buf.path, PATH_MAX, "%s/%u.txt", cfg.buf_dir, cli_id);

	if ((buf.fp = fopen(buf.path, "w+")) == NULL)
	{
		logerr("fopen");
		return -1;
	}

	if (setvbuf(buf.fp, NULL, _IOLBF, 0) < 0)
	{
		logerr("setvbuf");
		return -1;
	}

	logclidebugmsg(cli_id, "Opened buffer file: %s", buf.path);

	return 1;
}

char *file_path(id_t pr_id, const char *dir, const char *ext)
{
	assert(pr_id > 0);
	assert(ext != NULL);

	char *ret = (char *) malloc(PATH_MAX);
	if (NULL == ret)
	{
		logerr("malloc");
		return NULL;
	}

	sprintf(ret, "%s/%u.%s", dir, pr_id, ext);

	return ret;
}

int creat_pr_dir(char **path, time_t *start)
{
	if (NULL == *path && (*path = (char *) malloc(PATH_MAX)) == NULL) 
	{
		logerr("malloc");
		return -1;
	}

	struct tm *t_tm = localtime(start);
	char time_buf[30];
	strftime(time_buf, 30, "%G/%m/%d", t_tm);

	snprintf(*path, PATH_MAX, "%s/%s", cfg.arch_dir, time_buf);

	if (mkpath(*path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) < 0)
		return -1;

	logclidebugmsg(cli_id, "Program output dir: %s", *path);

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
			logerr("unlink");
		free(buf.path);
		buf.path = NULL;
	}
}

void sigusr2_parser()
{
	logclidebugmsg(cli_id, "SIGUSR2 recieved");
	sigusr2_received = TRUE;

	/* XXX: seed up parsing and db queries */
	return;
}

void sigusr1_parser()
{
	logclidebugmsg(cli_id, "SIGUSR1 recieved");

	cleanup_parser();

	logclidebugmsg(cli_id, "Terminating parser from SIGUSR1 handler");
	exit(EXIT_SUCCESS);
}

