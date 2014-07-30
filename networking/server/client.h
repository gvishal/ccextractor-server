#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>

pid_t fork_client(id_t id, int connfd, int listenfd);

int greeting();
int check_password();
int handle_bin_mode();
int bin_loop();
int read_bin_data();
int read_parser_data();

int open_parser_pipe();
int open_bin_file();
int reopen_bin_file();
int open_cce_input();
int init_cce_output();

// XXX rename
int handle_program_change_cli();

// int logged_in();
int add_cli_info();
void logged_out();
void remove_cli_info();

pid_t fork_cce();
#endif /* end of include guard: CLIENT_H */

