#ifndef NETWORKING_H
#define NETWORKING_H

#include <sys/types.h>

/* Protocol constants: */
#ifndef INT_LEN
#define INT_LEN         10
#endif
#define OK              1
#define PASSWORD        2
#define BIN_MODE        3
#define CC_DESC         4
#define ERROR           51
#define UNKNOWN_COMMAND 52
#define WRONG_PASSWORD  53
#define CONN_LIMIT      54
/* Buffer file and web-site constants: */
#define CONN_CLOSED     101
#define PROGRAM_ID      102
#define PROGRAM_NEW     103
#define PROGRAM_CHANGED 106
#define PROGRAM_DIR     108
#define PR_BIN_PATH     107
#define CAPTIONS        104
#define XDS             105
/* max: 108 */

/* 
 * Writes/Read data block from descriptor
 * block format:
 * command | lenght        | data         | \r\n    
 * 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes 
 */
ssize_t read_block(int fd, char *command, char *buf, size_t *buf_len);
ssize_t write_block(int fd, char command, const char *buf, size_t buf_len);

/*
 * Returns socket that can accept connections. Socket is binded to any
 * local address with specified port
 */
int bind_server(int port, int *fam);

#endif /* end of include guard: NETWORKING_H */
