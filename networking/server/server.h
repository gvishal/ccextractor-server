#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>

#define BUFFER_SIZE 40860
#define USERS_FILE_PATH "connections.txt"

#include <netdb.h>
struct cli_t;

int add_new_cli(int id, struct sockaddr *cliaddr, socklen_t clilen);
int clinet_command(int id);

void close_conn(int id);

void unmute_clients();

void open_log_file();

int update_users_file();

int open_buf_file(int id);
int store_cc(int id, char *buf, size_t len);
int open_arch_file(int id);

#endif /* end of include guard: SERVER_H */
