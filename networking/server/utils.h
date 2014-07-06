#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

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

/* Generates string of random chars */
void rand_str(char *s, size_t len);

#endif /* end of include guard: UTILS_H */
