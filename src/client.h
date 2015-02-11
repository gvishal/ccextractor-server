#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>

/** Forks client process. in created process: sets signal handlers, 
 * connects to db, handles clietns' commands.
 *
 * @param fd client socked descriptor
 * @param listenfd server coket that accept connections (to close in clild)
 * @param h host name c-string
 * @param s port number c-string
 * @return in parent process: client pid on success, -1 otherwise 
 */
// TODO: pass cli_t struct???
pid_t fork_client(int fd, int listenfd, char *h, char *s);

/** Writes "OK" to client, adds client to db 
 *
 * @return -1 on error, 1 on success
 */
int greeting();

/** Asks password from client untill he enters corrcet one. sleeps for 
 * cfg.wrong_pwd_delay seconds if the password is wrong.
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int check_password();

/** Handles client's commands (sets client desription or entres BIN mode)
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int ctrl_switch();

/** Writes "OK" to clients, reads BIN header, sets socket in non-blocking mode,
 * forks CCExtractor and parser.
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int handle_bin_mode();

/** Reads BIN header from client, checks if it's valid
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int read_bin_header();

/** Poll through client socket and parser output descriptor. 
 *
 * Client socket should receive BIN data, wich are hanlded by read_bin_data().
 * Parser sends notifications that program changed and paths should be changed.
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int bin_loop();

/** Reads BIN data from client socket and outputs it to CCExtractor input file
 * and BIN output file.
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
int read_bin_data();

/** Reads and sets path to new BIN output file from parser pipe.
 *
 * @return -1 on error, 0 if client closed connection, 1 on success.
 */
// TODO: rename
int read_parser_data();

/** Inits parser pipe 
 *
 * @return Pipe's write end on success, -1 otherwise
 */
int open_parser_pipe();

/** Creates and opens BIN output file (with the path form bin.path) and 
 * writes BIN header to this file.
 *
 * @return -1 on error, 1 otherwise
 */
int open_bin_file();

/** Creates and opens fifo file for CCExtractor BIN input. Disables 
 * buffering. Writes BIN header to it.
 * 
 * @return -1 on error, 1 otherwise
 */
int open_cce_input();

/** Creates CCExtractor output file path, wich would be passed to the parser.
 * Doesn't open it.
 *
 * @return -1 on error, 1 otherwise
 */
int init_cce_output();

/** Frees resources. Kills parser, ccextractor; closes opened descritors.
 * Removes client from active_clients table in db. Closes db connection
 */
void cleanup();

/** SIGCHLD handler. Determines whether parser or ccextractor terminated;
 * Writes ERROR status if child terminated with an error.
 * Terminates client (this) process.
 */
void sigchld_client();

/** Sends SIGUSR to parser (ie cause it to exit) and SIGINT to CCExtractor
 */
void kill_children();

/** Creates CCExtractor input and ouput files and Forks CCExtractor process.
 *
 * @return CCExtractor pid on success, -1 otherwise
 */
pid_t fork_cce();

#endif /* end of include guard: CLIENT_H */

