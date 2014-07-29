#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>

pid_t fork_client(unsigned id, int connfd, int listenfd);

int init_cli();

int greeting();
int check_password();
int handle_bin_mode();
int bin_loop();
int read_bin_data();
int open_bin_files();

// int logged_in();
int add_cli_info();
int logged_out();
int remove_cli_info();

pid_t fork_cce();
#endif /* end of include guard: CLIENT_H */

