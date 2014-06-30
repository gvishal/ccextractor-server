#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <signal.h>
#include <sys/wait.h>

#include <sys/file.h>

#define MAX_CONN 10

#define WRONG_PASSW_DELAY 1

#define BUF_FILE_DIR "./tmp"
#define BUF_FILE_PATH_LEN 50
#define BUF_FILE_LINES 20
#define TMP_FILE_PATH "./tmp/buf_tmp"

#define USERS_FILE_PATH "connections.txt"

struct cli_t
{
	unsigned is_logged : 1;
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];

	char buf_file_path[BUF_FILE_PATH_LEN];
	FILE *buf_fp;
	int cur_line;

}; 

int clinet_command(int id);

int update_users_file();

void close_conn(int id);

void print_cli_addr(int id);

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

/*
 * SIGCHLD handler. Reads zombie exit status and removes 
 * it from conn_pids array
 */
void sig_chld();

/*
 * SIGUSR1 handler. Sends SIGUSR2 to server's children. 
 */
void data_is_ready();

/*
 * SIGUSR2 handler. Reads data from buffer_fd and sends them 
 * to connected client
 */
void send_data();

/*
 * Client socket descriptor. 
 * It's global because handler sends data to clients.
 */
int connfd;

/*
 * PIDs of server's childen working with clients
 */
int conn_cnt;
pid_t conn_pids[MAX_CONN];

/*
 * Descriptor of buffer file. Server's children process lock (LOCK_SH) it
 * before reading data to avoid readers-writers problem
 */
int buffer_fd;


#endif /* end of include guard: SERVER_H */
