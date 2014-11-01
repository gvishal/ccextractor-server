#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1

/* Returned errors: */
#define ERRNO -1 /* Use errno value */
/* read_block(): */
#define BLK_SIZE -2 /* Wrong block size */
#define END_MARKER -3 /* No end marker present */
/* bind_server(): */
#define B_SRV_GAI -9 /* getaddrinfo() error val in errno */
#define B_SRV_ERR -10 /* Could bind */

/* debug verbose levels: */
#define FATAL 1
#define ERR 2
#define WARNING 3
#define INFO 4

#ifndef INT_LEN
#define INT_LEN 10
#endif

#define MODE755 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

typedef struct 
{
	char *path;
	FILE *fp;
} file_t;

typedef unsigned id_t;

/* Reads n bytes from descriptor */
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);


void debug(int vlevel, int cli_id, const char *fmt, ...);

#define logfatalmsg(str) debug(FATAL, 0, "%s:%d %s\n", __FILE__, __LINE__, str)
#define logerrmsg(str) debug(FATAL, 0, "%s:%d %s\n", __FILE__, __LINE__, str)
#define loginfomsg(str) debug(FATAL, 0, "%s:%d %s\n", __FILE__, __LINE__, str)

#define logfatal(str)    debug(FATAL, 0, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, strerror(errno))
#define logerr(str)      debug(ERR,   0, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, strerror(errno))
#define logmysqlfatal(str, conn) debug(FATAL, 0, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, mysql_error(conn))
#define logmysqlerr(str, conn)   debug(ERR,   0, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, mysql_error(conn))

/* Writes message to stderr */
const char *m_strerror(int rc);
void c_log(int cli_id, const char *fmt, ...);
// FIXME: these names may be reserved
#define _log(...) c_log(0, __VA_ARGS__)

#define _perror(str) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, strerror(errno))
#define m_perror(str, rc) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, m_strerror(rc))
// TODO change cli_id
#define c_perror(cli, str, rc) c_log(cli, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, m_strerror(rc))
#define mysql_perror(str, rc) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, mysql_error(rc))

/* Sets signal handler */
void _signal(int sig, void (*func)(int));

/* Recursive mkdir */
int mkpath(const char *path, mode_t mode);

/* Generates string of random chars */
void rand_str(char *s, size_t len);

/* Deletes first n lines (end with \n) from the file */
int delete_n_lines(FILE **fp, char *filepath, size_t n);

/* Returns malloc'ed copy of `s` without not printable chars*/
char *nice_str(const char *s, size_t *len);

/* Sets socket in non blocking mode */
int set_nonblocking(int fd);

/* Copies src sting to dest, including \0, return number of copied bytes */
size_t strmov(char *dest, const char *src);

int set_tz();

#endif /* end of include guard: UTILS_H */
