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

/* log verbose levels: */
/** verbose level -- errors that cause server to terminate */
#define FATAL   1
/** verbose level -- errors that cause server to close client connection */
#define ERR     2
/** verbose level -- not implemeted functions, unexpected behaviour */
#define WARNING 3
/** verbose level -- information about client actions */
#define INFO    4
/** verbose level -- debug messages */
#define DEBUG   5
/** verbose level -- server-client, interprocess, server-website communications */
#define CHAT    6

#define MODE755 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

/** buffer size for log messages */
#define LOG_MSG_SIZE 300

/** file structure with FILE pointer and path */
typedef struct 
{
	char *path;
	FILE *fp;
} file_t;

typedef unsigned id_t;

file_t logfile;

/** Reads n bytes from descriptor. 
 *
 * Handles kernel buffer overflow. Restarts when signal interrupts read.
 * If specified socket is non-blocking, reads avaliable number of 
 * bytes and returns.
 * @param fd file or socket descriptor
 * @param vptr buffer pointer with size >= \p n. 
 * If NULL, then \p n bytes will be read (one by one) 
 * @param n number of bytes to read
 * @return number of bytes read from descriptor. -1 on error and sets errno
 */
ssize_t readn(int fd, void *vptr, size_t n);

/** Writes n bytes to descriptor 
 *
 * Handles kernel buffer overflow. Restarts when signal interrupts read.
 * @param fd file or socket descriptor
 * @param vptr buffer pointer with size >= \p n. 
 * If NULL, then \p n must be null, nothing will be written
 * @param n number of bytes to write
 * @return number of bytes written to descriptor. -1 on error and sets errno
 */
ssize_t writen(int fd, const void *vptr, size_t n);

/** Convinence function for writing single byte
 * @param fd file or socket descriptor
 * @param ch byte to write
 * @return 1 when no errors occured, -1 otherwise
 */
ssize_t write_byte(int fd, char ch);

/** Convinence function for reading single byte
 * @param fd file or socket descriptor
 * @param ch buffer pointer where the byte will be stored
 * @return 1 when no errors occured, -1 otherwise
 */
size_t read_byte(int fd, char *ch);

/** Creates and opens log file. Changes global logfile varibale.
 * @return 1 when no errors occured, -1 otherwise
 */
int open_log_file();

/** Prints log message to log file or stderr (if log file is not opened)
 * @param vlevel log verbose level
 * @param cli_id client id that caused this message or 0 for server
 * @param file c-string with filenname \__FILE__
 * @param line line number if \p file \__LINE__
 * @param fmt message format string (like in fprintf :) )
 */
void printlog(unsigned vlevel, int cli_id, const char *file, int line,
		const char *fmt, ...);

void logblock(unsigned cli_id, char c, char *buf, size_t len,
		const char *direction);

#define logfatalmsg(...)   printlog(FATAL,   0, __FILE__, __LINE__, __VA_ARGS__)
#define logerrmsg(...)     printlog(ERR,     0, __FILE__, __LINE__, __VA_ARGS__)
#define loginfomsg(...)    printlog(INFO,    0, __FILE__, __LINE__, __VA_ARGS__)
#define logclimsg(id, ...) printlog(INFO,   id, __FILE__, __LINE__, __VA_ARGS__)
#define logwarningmsg(...) printlog(WARNING, 0, __FILE__, __LINE__, __VA_ARGS__)
#define logdebugmsg(...)   printlog(DEBUG,   0, __FILE__, __LINE__, __VA_ARGS__)
#define logchatmsg(id, ...) printlog(CHAT,id, __FILE__, __LINE__, __VA_ARGS__)
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
