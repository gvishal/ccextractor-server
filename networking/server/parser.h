#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

struct pr_t
{
	id_t id;
	char *name;
	char *dir;
	time_t start;
};

pid_t fork_parser(id_t id, const char *cce_output, int pipe_w);

int parser_loop();

int parse_line(char *line, size_t len);

char *is_xds(char *line);
char *is_program_changed(char *line);
int set_pr(char *new_name);

int send_pr_to_parent();
int send_pr_to_buf();

int open_buf_file();
int open_txt_file();
int open_xds_file();
int reopen_txt_file();
int reopen_xds_file();

int append_to_buf(const char *line, size_t len, char mode);
int append_to_txt(const char *line, size_t len);
int append_to_xds(const char *line, size_t len);

int creat_pr_dir(char **path, time_t *start);
char *file_path(id_t prgm_id, const char *dir, const char *ext);

#endif /* end of include guard: PARSER_H */

