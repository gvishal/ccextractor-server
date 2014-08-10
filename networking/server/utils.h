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
/* parse_config_file(): */
#define CFG_ERR -4 /* Can't parse config file */
#define CFG_NUM -5 /* Number expected */
#define CFG_STR -6 /* String expected */
#define CFG_BOOL -7 /* true/false expected */
#define CFG_UNKNOWN -8 /* Unknown key-value pair */
/* bind_server(): */
#define B_SRV_GAI -9 /* getaddrinfo() error val in errno */
#define B_SRV_ERR -10 /* Could bind */

#ifndef INT_LEN
#define INT_LEN 10
#endif

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

/* Writes message to stderr */
const char *m_strerror(int rc);
void c_log(int cli_id, const char *fmt, ...);
// FIXME: these names may be reserved
#define _log(...) c_log(0, __VA_ARGS__);

#define _perror(str) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, strerror(errno));
#define m_perror(str, rc) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, m_strerror(rc));
// TODO change cli_id
#define c_perror(cli, str, rc) c_log(cli, "%s:%d %s() error: %s\n", __FILE__, __LINE__, str, m_strerror(rc));
#define mysql_perror(str, rc) _log("%s:%d %s() error: %s\n", __FILE__, __LINE__, str, mysql_error(rc));

/* Sets signal handler */
void _signal(int sig, void (*func)(int));

/* Recursive mkdir */
int _mkdir(const char *dir, mode_t mode);

/* Generates string of random chars */
void rand_str(char *s, size_t len);

/* Deletes first n lines (end with \n) from the file */
int delete_n_lines(FILE **fp, char *filepath, size_t n);

/* Returns malloc'ed copy of `s` without not printable chars*/
char *nice_str(const char *s, size_t *len);

/* Sets socket in non blocking mode */
int set_nonblocking(int fd);

/* Copies src sting to dest, including \0, return number of copied bytes */
size_t strmov(char *dest, char *src);

#endif /* end of include guard: UTILS_H */
