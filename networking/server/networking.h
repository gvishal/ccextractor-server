#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

/* Protocol constants: */
#define INT_LEN         10
#define OK              1
#define PASSWORD        2
#define BIN_HEADER      3
#define ERROR           51
#define UNKNOWN_COMMAND 52
#define WRONG_PASSWORD  53
#define CONN_LIMIT      54
/* Buffer file and web-site constants: */
#define CONN_CLOSED     101
#define PROGRAM_ID      102
#define RESET_PROGRAM   103

/* 
 * Read data block from descriptor
 * block format:
 * command | lenght        | data         | \r\n    
 * 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes 
 */
ssize_t read_block(int fd, char *command, char *buf, size_t *buf_len);

/*
 * Returns socket that can accept connections. Socket is binded to any
 * local address with specified port
 */
int bind_server(int port);

#endif /* end of include guard: NETWORKING_H */
