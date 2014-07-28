#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>

pid_t fork_parser(unsigned cli_id, const char *cce_output);

int parser_loop();

int parse_line(const char *line, size_t len);

int is_xds(const char *line);
int is_cc(const char *line);

int open_buf_file();
int open_txt_file();
int open_xds_file();
// int open_srt_file(const char *program);

int append_to_txt(const char *line, size_t len);
int append_to_xds(const char *line, size_t len);
// int append_to_srt(const char *line, size_t len);
int append_to_buf(const char *line, size_t len, char mode);

int file_path(char **path, const char *ext);

// int append_to_arch_info();

#endif /* end of include guard: PARSER_H */

