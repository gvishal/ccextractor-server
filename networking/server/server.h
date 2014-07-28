#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>

struct cli_t;

int add_new_cli(int fd, struct sockaddr *cliaddr, socklen_t clilen);
int cli_logged_in();
int clinet_command();
void close_conn(int id);
int init_cli_chld(int fd, int id);
int cli_loop();

void open_log_file();

int update_users_file();

int fork_cce();

void sig_chld();

#endif /* end of include guard: SERVER_H */
