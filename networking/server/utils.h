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
/* delete_n_lines(): */
#define DEL_L_EOF -11 /* Unexpected eof */

#define TMP_FILE_PATH "./tmp_file" /* temprorary file for delete_n_lines */

/* Reads n bytes from descriptor */
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);

/* Writes message to stderr */
void _log(const char *fmt, ...);
void c_log(int cli_id, const char *fmt, ...);

/* Writes to stderr description of returned value */
#define e_log(rc) ec_log(0, (rc))
void ec_log(int cli_id, int rc);

/* Sets signal handler */
void _signal(int sig, void (*func)(int));

/* Recursive mkdir */
int _mkdir(const char *dir, mode_t mode);

/* Generates string of random chars */
void rand_str(char *s, size_t len);

/* Deletes first n lines (end with \n) from the file */
int delete_n_lines(FILE **fp, const char *filepath, size_t n);

#endif /* end of include guard: UTILS_H */
