#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>

// TODO: config file
#define BUFFER_SIZE 40860
#define MAX_CONN 10
#define BUF_FILE_DIR "./tmp"
#define BUF_FILE_PATH_LEN 50
#define BUF_FILE_LINES 20
#define TMP_FILE_PATH "./tmp/buf_tmp"
#define USERS_FILE_PATH "connections.txt"
#define LOG_DIR "./logs"
#define WRONG_PASSW_DELAY 2
#define CREATE_LOGS 0
#define ARCHIVE_DIR "./cc"
#define ARCHIVE_FILEPATH_LEN 50
#define PROGRAM_NAME_LEN 50

struct cli_t;

int clinet_command(int id);

void close_conn(int id);

void unmute_clients();

void open_log_file();

int update_users_file();

int open_buf_file(int id);
int store_cc(int id, char *buf, size_t len);
int open_arch_file(int id);

/*
 * Fills specified string with random letters. Lenght is randomn but
 * less than MAX_PAWSSWORD_LEN.
 */
void gen_passw(char *p);

#endif /* end of include guard: SERVER_H */
