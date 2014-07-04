#ifndef NETWORKING_H
#define NETWORKING_H

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <assert.h>

#define INT_LEN 10

#define PASSW 10
#define CC 11
#define WAIT 5
#define SERV_ERROR 4
#define WRONG_COMMAND 3
#define WRONG_PASSW 2
#define OK 1

#define BUFFER_SIZE 40860
#define TMP_FILE_PATH_MAX_LEN 

/* Function Errors: */
#define ERRNO -1
#define BLK_SIZE -2
#define END_MARKER -3

/* Reads n bytes from descriptor*/
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);

ssize_t read_block(int fd, char *command, char *buf, size_t *buf_len);
ssize_t write_block(int fd, char command, const char *buf, size_t buf_len);

void print_address(const struct sockaddr *sa, socklen_t salen);

#endif /* end of include guard: NETWORKING_H */
