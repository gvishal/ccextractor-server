#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

pid_t fork_parser(unsigned id, const char *cce_output, int pipe_w);

int parser_loop();

int parse_line(const char *line, size_t len);

const char *is_xds(const char *line);
int is_program_changed(const char *line);
int handle_program_change();

int send_prgm_to_parent();
int send_prgm_to_buf();

int open_buf_file();
int open_txt_file();
int open_xds_file();
int reopen_txt_file();
int reopen_xds_file();

int append_to_buf(const char *line, size_t len, char mode);
int append_to_txt(const char *line, size_t len);
int append_to_xds(const char *line, size_t len);

int file_path(char **path, const char *ext, unsigned cli_id, unsigned prgm_id);
// int append_to_arch_info();

#endif /* end of include guard: PARSER_H */

