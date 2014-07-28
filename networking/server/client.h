#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>

pid_t fork_client();

int init_cli();

int logged_in();

int greeting();
int check_password();
int handle_bin_mode();
int bin_loop();
int read_bin_data();

int fork_cce();
#endif /* end of include guard: CLIENT_H */

