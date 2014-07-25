#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

/* Protocol constants: */
#define INT_LEN         10
#define OK              1
#define PASSWORD        2
#define BIN_MODE        3
#define ERROR           51
#define UNKNOWN_COMMAND 52
#define WRONG_PASSWORD  53
#define CONN_LIMIT      54

void connect_to_srv(const char *addr, const char *port);

void net_send_header(const char *data, size_t len);
void net_send_cc(const char *data, size_t len);

#endif /* end of include guard: NETWORKING_H */
