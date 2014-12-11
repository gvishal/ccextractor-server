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
 * @param line line number in \p file \__LINE__
 * @param fmt message format string (like in fprintf :) )
 */
void printlog(unsigned vlevel, int cli_id, const char *file, int line,
		const char *fmt, ...);

/** Prints protocol block to log file or stderr (if log file is not opened)
 * @param cli_id client id that caused this message or 0 for server
 * @param c protocol defined command
 * @param buf pointer with block's data to be printed 
 * @param len size of \p buf
 * @param direction c-string identifying sender and reciever of this block \n
 * possible values:\n
 * "S->N" from server to network (ie connected client)\n
 * "S<-N" from network to server\n
 * "S->P" from server to cc parser\n
 * "S<-P" from cc parser to server\n
 * "S->B" from server to buffer file (ie web site)\n
 * @see write_block
 */
void logblock(unsigned cli_id, char c, char *buf, size_t len,
		const char *direction);

/** Logs fatal error message (1st verbose level) with file and line no.
 * @param ... format string and its arguments (id any)/
 */
#define logfatalmsg(...)   printlog(FATAL,   0, __FILE__, __LINE__, __VA_ARGS__)

/** Logs fatal error message (2nd verbose level) with file and line no.
 * @param ... format string and its arguments (id any)/
 */
#define logerrmsg(...)     printlog(ERR,     0, __FILE__, __LINE__, __VA_ARGS__)

/** Logs info message (3rd verbose level) with file and line no.
 * @param ... format string and its arguments (id any)/
 */
#define loginfomsg(...)    printlog(INFO,    0, __FILE__, __LINE__, __VA_ARGS__)

/** Logs info message (3rd verbose level) with file and line no. 
 * for specified client
 * @param id client id to printed in log message
 * @param ... format string and its arguments (id any)/
 */
#define logclimsg(id, ...) printlog(INFO,   id, __FILE__, __LINE__, __VA_ARGS__)

/** Logs warning message (4th verbose level) with file and line no.
 * @param ... format string and its arguments (id any)/
 */
#define logwarningmsg(...) printlog(WARNING, 0, __FILE__, __LINE__, __VA_ARGS__)

/** Logs debug message (5th verbose level) with file and line no.
 * @param ... format string and its arguments (id any)/
 */
#define logdebugmsg(...)   printlog(DEBUG,   0, __FILE__, __LINE__, __VA_ARGS__)

// TODO remove
#define logchatmsg(id, ...) printlog(CHAT, id, __FILE__, __LINE__, __VA_ARGS__)

/** Logs debug message (5th verbose level) with file and line no.
 * for specified client.
 * @param id client id to printed in log message
 * @param ... format string and its arguments (id any)/
 */
#define logclidebugmsg(id, ...) printlog(DEBUG,   id, __FILE__, __LINE__, __VA_ARGS__)

/** Logs fatal error message (1st verbose level) with file and line no.
 * Inserts errno decription in the message and specified function name.
 * @param str function name that caused this error
 */
#define logfatal(str)            printlog(FATAL, 0, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))

/** Logs error message (2nd verbose level) with file and line no.
 * Inserts errno decription in the message and specified function name.
 * @param str function name that caused this error
 */
#define logerr(str)              printlog(ERR,   0, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))

/** Logs error message (2nd verbose level) with file and line no.
 * for specified client.
 * Inserts errno decription in the message and specified function name.
 * @param id client id to printed in log message
 * @param str function name that caused this error
 */
#define logcli(id, str)          printlog(ERR,  id, __FILE__, __LINE__, "%s() error: %s", str, strerror(errno))

/** Logs error message (2nd verbose level) with file and line no.
 * for specified client.
 * Inserts error decription from m_strerror in the message and specified function name.
 * @param id client id to printed in log message
 * @param str function name that caused this error
 * @param rc error value to be passed to m_strerror
 * @see m_strerror
 */
#define logcli_no(id, str, rc)   printlog(ERR,  id, __FILE__, __LINE__, "%s() error: %s", str, m_strerror(rc))

/** Logs MySQL fatal error message (1st verbose level) with file and line no.
 * Inserts MySQL error decription in the message and specified function name.
 * @param str function name that caused this error
 * @param conn MySQL connection where error occurred
 */
#define logmysqlfatal(str, conn) printlog(FATAL, 0, __FILE__, __LINE__, "%s() error: %s", str, mysql_error(conn))

/** Logs MySQL error message (2nd verbose level) with file and line no.
 * Inserts MySQL error decription in the message and specified function name.
 * @param str function name that caused this error
 * @param conn MySQL connection where error occurred
 */
#define logmysqlerr(str, conn)   printlog(ERR,   0, __FILE__, __LINE__, "%s() error: %s", str, mysql_error(conn))

/** Error desprition for defiened return values
 * @param rc negative value indicating error returned by some function 
 * @return c-string with error description
 */
const char *m_strerror(int rc);

/** Convinence function for setting signal handler
 * @param sig signal number
 * @param func pointer to handler function
 */
int m_signal(int sig, void (*func)(int));

/** Locks file specified by it's FILE pointer using flock.
 * Restarts when signal interrupts blocking.
 * @param fp FILE pointer
 * @param l second flock argument (LOCK_EX or LOCK_UN)
 */
void m_lock(FILE *fp, int l);

/** Recursive mkdir. Splits path into directories and creates each directory if
 * it doesn't exist.
 * @param path c-string with path to create
 * @param mode mode for created directories
 */
int mkpath(const char *path, mode_t mode);

/** Generates string of random chars from 'a' to 'z'
 * @param s pointer to string
 * @param len lenght of string to generate
 */
void rand_str(char *s, size_t len);

/** Deletes first n lines (that end with \n) from the file. 
 * Next lines are moved to the beginning of file
 * @param fp pointer to FILE pointer. At the end new file will be opened
 * @param filepath c-string with file path related to \p fp
 * @param n number of lines to delete
 * @return negative number if error occured
 */
int delete_n_lines(FILE **fp, char *filepath, size_t n);

/** Returns malloc'ed copy of string without non printable chars
 * @param s string to be cleaned up
 * @param len pointer to lenght of \p s. 
 * After this call it'd contain lenght of copyied string
 * @return pointer to the copy \p s, or NULL if error occured
 */
char *nice_str(const char *s, size_t *len);

/** Sets socket in non blocking mode
 * @param fd socket descriptor
 * @return negative number if error occured
 */
int set_nonblocking(int fd);

/** Copies \p src sting to \p dest, including \\0.
 * @param dest pointer to a string to copy src. 
 * Lenght must not be less than strlen(src)
 * @param src null terminating string to copy. 
 * @return number of copied bytes
 */
size_t strmov(char *dest, const char *src);

/** Sets TZ enviroment varibale to a value from cfg.env_tz
 * @see cfg_t::env_tz
 */
int set_tz();

#endif /* end of include guard: UTILS_H */
