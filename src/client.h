#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>

pid_t fork_client(int fd, int listenfd, char *h, char *s);

int greeting();
int check_password();
int ctrl_switch();
int handle_bin_mode();
int bin_loop();
int read_bin_data();
int read_bin_header();

int read_parser_data();
int read_pr_id();
int read_pr_name();
int read_pr_dir();

int open_parser_pipe();
int open_bin_file();
int open_cce_input();
int init_cce_output();

void cleanup();
void logged_out();

void sigchld_client();
void kill_children();

pid_t fork_cce();

#endif /* end of include guard: CLIENT_H */

