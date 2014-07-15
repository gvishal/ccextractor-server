#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

/* Protocol constants: */
#define INT_LEN         10
#define OK              1
#define PASSWORD        2
#define CAPTIONS        3
#define PROGRAM         4
#define ERROR           51
#define UNKNOWN_COMMAND 52
#define WRONG_PASSWORD  53
#define CONN_LIMIT      54

void connect_to_srv(const char *addr, const char *port);

void net_append_cc(const char *fmt, ...);
void net_append_cc_n(const char *data, size_t len);
void net_send_cc();

void net_set_new_program(const char *name);

#endif /* end of include guard: NETWORKING_H */
