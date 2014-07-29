#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>

struct cli_t;

int add_new_cli(int fd, struct sockaddr *cliaddr, socklen_t clilen);
void close_conn(int id);

void open_log_file();

int update_users_file();

void sig_chld();

#endif /* end of include guard: SERVER_H */
