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

struct cli_t
{
	unsigned is_logged : 1;
	time_t muted_since;
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	char buf_file_path[BUF_FILE_PATH_LEN];
	FILE *buf_fp;
	int cur_line;
}; 

int clinet_command(int id);

int update_users_file();

void close_conn(int id);

void unmute_clients();

void open_log_file();

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

#endif /* end of include guard: SERVER_H */
