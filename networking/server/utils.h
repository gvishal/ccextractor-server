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
#define FATAL   1
#define ERR     2
#define WARNING 3
#define INFO    4
#define DEBUG   5
#define NETWORK 6

#ifndef INT_LEN
#define INT_LEN 10
#endif

#define MODE755 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

#define LOG_MSG_SIZE 300

typedef struct 
{
	char *path;
	FILE *fp;
} file_t;

typedef unsigned id_t;

file_t logfile;

/* Reads n bytes from descriptor */
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);

int open_log_file();

void printlog(unsigned vlevel, int cli_id, const char *file, int line,
		const char *fmt, ...);

void lognetblock(unsigned cli_id, char c, char *buf, size_t len,
		const char *direction);

#define logfatalmsg(...)   printlog(FATAL,   0, __FILE__, __LINE__, __VA_ARGS__)
#define logerrmsg(...)     printlog(ERR,     0, __FILE__, __LINE__, __VA_ARGS__)
#define loginfomsg(...)    printlog(INFO,    0, __FILE__, __LINE__, __VA_ARGS__)
#define logclimsg(id, ...) printlog(INFO,   id, __FILE__, __LINE__, __VA_ARGS__)
#define logwarningmsg(...) printlog(WARNING, 0, __FILE__, __LINE__, __VA_ARGS__)
#define logdebugmsg(...)   printlog(DEBUG,   0, __FILE__, __LINE__, __VA_ARGS__)
#define lognetmsg(id, ...) printlog(NETWORK,id, __FILE__, __LINE__, __VA_ARGS__)
#define logclidebugmsg(id, ...) printlog(DEBUG,   id, __FILE__, __LINE__, __VA_ARGS__)

#define logfatal(str)            printlog(FATAL, 0, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))
#define logerr(str)              printlog(ERR,   0, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))
#define logcli(id, str)          printlog(ERR,  id, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))
#define logcli_no(id, str, rc)   printlog(ERR,  id, __FILE__, __LINE__, "%s() error: %s", str, m_strerror(rc))
#define logmysqlfatal(str, conn) printlog(FATAL, 0, __FILE__, __LINE__, "%s() error: %s", str, mysql_error(conn))
#define logmysqlerr(str, conn)   printlog(ERR,   0, __FILE__, __LINE__, "%s() error: %s", str, mysql_error(conn))

/* Writes message to stderr */
const char *m_strerror(int rc);

/* Sets signal handler */
int m_signal(int sig, void (*func)(int));

void m_lock(FILE *fp, int l);

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
