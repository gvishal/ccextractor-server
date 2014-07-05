#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

/* Returned errors: */
#define ERRNO -1 /* Use errno value */
#define BLK_SIZE -2 /* Wrong block size */
#define END_MARKER -3 /* No end marker present */

/* Reads n bytes from descriptor */
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);

/* Writes message to stderr */
void _log(int cli_id, const char *fmt, ...);

/* Writes to stderr description of returned value */
void e_log(int cli_id, int ret_val);

/* Sets signal handler */
void _signal(int sig, void (*func)(int));

/* Recursive mkdir */
int _mkdir(const char *dir, mode_t mode);

#endif /* end of include guard: UTILS_H */
