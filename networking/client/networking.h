#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

#define INT_LEN 10

#define PASSW 10
#define CC 11
#define WAIT 5
#define SERV_ERROR 4
#define WRONG_COMMAND 3
#define WRONG_PASSW 2
#define OK 1

ssize_t write_block(int fd, char command, const char *buf, size_t buf_len);

int connect_to_addr(const char *host, const char *port);

void check_passwd(int fd);

#endif /* end of include guard: NETWORKING_H */
