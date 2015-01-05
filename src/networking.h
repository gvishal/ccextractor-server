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

/** Reads data block from descriptor. 
 *
 * Blocks have following format:
 * -# Command -- 1 byte
 * -# Lenght -- INT_LEN (ie 10) bytes
 * -# Data -- Lenght bytes
 * -# \\r\\n -- 2 bytes
 *
 * Command is defined protocol constant, representing block's data type
 *
 * Lenght is a string with decimal number representing data size in bytes.
 * Lenght can be 0, then no data would be read and \p buf_len would be set to 0.
 * If lenght is greater than 0, buf specified buffer is NULL or 0-size, then 
 * BLK_SIZE error is returned. If buffer size is smaller than block size,
 * \p buf_size would be read, other bytes would be ignored and warning message 
 * would be printed to logs.
 * 
 * If read_block didn't find "\\r\\n" bytes at the end, then END_MARKER error is
 * returned.
 *
 * If readn call inside this function can't read specied number of bytes, then
 * read_block returns 0. This can happen if client closed connection or specifed
 * descriptor is in non-blocking mode. If readn ends with error, read_block 
 * returns ERRNO constans, meaning that error value is hold in errno variable.
 *
 * To get error string corresponding to returned values, m_strerror should be called.
 *
 * @param fd file or socket descriptor to read from
 * @param command 1 byte, representing block's data type
 * @param buf data buffer of buf_len size. It can be NULL if block Lenght is
 * expected to be 0. 
 * @param buf_len before call it should contain size of data buffer (\p buf).
 * After suсcessful completion it shall contain a number of bytes read into
 * data buffer (\p buf)
 * @return On succcess, number of bytes read in whole block is returned.\n
 * Returns 0, if it can't read specified number of bytes from decriptor 
 * (eg. client closed connection).\n
 * Otherwise, returns negative value, represinting error.
 *
 * @see readn
 * @see m_strerror
 */
ssize_t read_block(int fd, char *command, char *buf, size_t *buf_len);


/** Writes data block to descriptor.
 *
 * Blocks have following format:
 * -# Command -- 1 byte
 * -# Lenght -- INT_LEN (ie 10) bytes
 * -# Data -- Lenght bytes
 * -# \\r\\n -- 2 bytes
 *
 * @param fd file or socket descriptor to write to
 * @param command 1 byte, representing block's data type
 * @param buf data buffer of buf_len size which would be written to \p fd 
 * @param buf_len size of data buffer. If it's 0, no data from \p buf 
 * would be written
 * @return On succcess, number of bytes written in whole block is returned.\n
 * Returns 0, if it can't write specified number of bytes to decriptor 
 * (eg. client closed connection).\n
 * Otherwise, returns negative value, and sets errno to indicate the error.
 *
 * @see writen
 */
ssize_t write_block(int fd, char command, const char *buf, size_t buf_len);

/** Creates socket that can accept connections on any local address with 
 * specified port
 *
 * bind_server tries to bind to any local address with specief port. If
 * something fails, then it tries next address in the list returned 
 * by getaddrinfo(). It shall create TCP socket for chosen address. If
 * this address is IPv6, then IPV6_V6ONLY is set to 0 (ie it can accept 
 * both IPv4 and IPv6). Then it bind()s this address to created socket and
 * listen()s for incoming connections.
 * 
 * @param port port number to bind to
 * @param fam on success it shall contain address family (ai_family) 
 * @return socket descriptor on success.\n
 * -1 on error and prints error message to log. 
 */
int bind_server(int port, int *fam);

#endif /* end of include guard: NETWORKING_H */
