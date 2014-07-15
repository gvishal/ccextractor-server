#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

#define INT_LEN 10

#define PASSW 10
#define MAX_CONN 16
#define NEW_PRG 12
#define CC 11
#define WAIT 5
#define SERV_ERROR 4
#define WRONG_COMMAND 3
#define WRONG_PASSW 2
#define OK 1

void connect_to_srv(const char *addr, const char *port);

void net_append_cc(const char *fmt, ...);
void net_append_cc_n(const char *data, size_t len);
void net_send_cc();

void net_set_new_program(const char *name);

#endif /* end of include guard: NETWORKING_H */
