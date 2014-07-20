#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>

struct cli_t;

int add_new_cli(int id, struct sockaddr *cliaddr, socklen_t clilen);
int cli_logged_in(int id);
int clinet_command(int id);
void close_conn(int id);

void unmute_clients();

void open_log_file();

int update_users_file();

int open_buf_file(int id);
int send_to_buf(int id, char command, char *buf, size_t len);

int open_arch_file(int id);
int append_to_arch_info(int id);

int file_path(char **path, const char *ext, int id);

int fork_cce(int id);

int fork_txt_parser(int id);

void sig_chld();

#endif /* end of include guard: SERVER_H */
