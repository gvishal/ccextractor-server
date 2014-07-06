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
