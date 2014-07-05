#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <signal.h>
#include <sys/wait.h>

#include <sys/file.h>
#include <stdarg.h>

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

struct cli_t
{
	unsigned is_logged : 1;
	time_t muted_since;
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	char cur_program[PROGRAM_NAME_LEN];
	time_t prgrm_statr;

	char buf_file_path[BUF_FILE_PATH_LEN];
	FILE *buf_fp;
	int cur_line;

	char arch_filepath[ARCHIVE_FILEPATH_LEN];
	FILE *arch_fp;

}; 

int clinet_command(int id);

void close_conn(int id);

void unmute_clients();

void open_log_file();

int update_users_file();

int open_buf_file(int id);
int store_cc(int id, char *buf, size_t len);
int open_arch_file(int id);

void _log(int cli_id, const char *fmt, ...);
void e_log(int cli_id, int ret_val);

/*
 * Returns socket that can accept connections. Socket is binded to any
 * local address with specified port
 */
int bind_server(const char *port);

/*
 * Fills specified string with random letters. Lenght is randomn but
 * less than MAX_PAWSSWORD_LEN.
 */
void gen_passw(char *p);

/*
 * Convenience function for setting signal handler
 */
void my_signal(int sig, void (*func)(int));

int _mkdir(const char *dir, mode_t mode);

#endif /* end of include guard: SERVER_H */
