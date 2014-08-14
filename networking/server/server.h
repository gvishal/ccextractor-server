#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>

struct cli_t;

int add_new_cli(int fd, struct sockaddr *cliaddr, socklen_t clilen);
void close_conn(int id);

void open_log_file();

void sigchld_server();
void cleanup_server();

#endif /* end of include guard: SERVER_H */
