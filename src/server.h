#ifndef SERVER_H
#define SERVER_H

#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>

/** Holds info about connected client (socket, host, port, process pid) */
struct cli_t;

/** Adds client to global client array, fills cli_t structure's host and
 * serv fields (using getnameinfo).
 *
 * @param fd client socket descriptor
 * @param cliaddr socket address structure 
 * @param clilen size of \p cliaddr structure
 * @return -1 on error, new client id in global array otherwise
 */
int add_new_cli(int fd, struct sockaddr *cliaddr, socklen_t clilen);

/** Frees cli_t structure of specified client. Compresses global clietns array
 * by moving following clinets' entries closer to beginning.
 *
 * @parma id client id to remove
 */
void free_cli(int id);


/** Closes socket, kills client process (sending SIGUSR1) and frees 
 * entry in global clients array.
 *
 * @param id client id
 * @see free_cli
 */
void close_conn(int id);

/** SIGCHLD handler. Determines client's id and removes it from global array.
 */
void sigchld_server();

/** SIGINT handler. Kills all connected clietns (SIGUSR1), waits for them to 
 * terminate, exits server with EXIT_SUCCESS.
 */
void cleanup_server();

/** Creates necessary dirs with 755 mode
 * @retrun -1 on error, 1 on success
 */
int creat_dirs();

#endif /* end of include guard: SERVER_H */
