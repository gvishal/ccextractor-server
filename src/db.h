#ifndef DB_H
#define DB_H

#include <sys/types.h>

#define QUERY_LEN 2048


/** Initializes MySql library, establishes a connection, 
 * creates tables if dont exist, truncates active_clients table,
 * closes connection.
 *
 * Should be called from parent process. Child process should establish
 * thier own connections.
 *
 * @return -1 on error and prints error massage to log. 1 on success
 * @see db_conn
 * @see creat_tables
 * @see db_close_conn
 */
int init_db();

/** Establishes a connection to DB specified in server options (struct cfg_t).
 * Sets MySQL time zone.
 *
 * @return -1 on error and prints error massage to log. 1 on success
 * @see cfg_t
 */
int db_conn();

/** Closes DB connection */
void db_close_conn();

/** Creates DB and required tables. See wikipages for db arcitecture.
 *
 * @return -1 on error and prints error massage to log. 1 on success
 */
int creat_tables();

/** Locks writing on clients table. 
 *
 * Should be used before changing clients table to syncronize concurrent 
 * access.
 *
 * @return -1 on error and prints error massage to log. 1 on success
 */
int lock_cli_tbl();

/** Locks writing on programs table. 
 *
 * Should be used before changing programs table to syncronize concurrent 
 * access.
 *
 * @return -1 on error and prints error massage to log. 1 on success
 */
int lock_pr_tbl();

/** Unlocks all tables in DB
 *
 * @return -1 on error and prints error massage to log. 1 on success
 */
int unlock_db();

/** Executes specified statement.
 *
 * Blocks SIGUSR1 and SIGUSR2 signals during quering as they are used
 * for modules communication. If connection "has gone away" 
 * establishes new one and executes the statement again.
 *
 * @param q c-string with SQL statement
 * @return -1 on error and prints error massage to log. 1 on success
 */
int query(const char *q);

int db_add_cli(const char *host, const char *serv, id_t *new_id);
int db_get_last_id(id_t *new_id);
int db_set_cc_desc(id_t cli_id, const char *desc);

int db_add_active_cli(id_t id);
int db_set_pr_arctive_cli(id_t id, id_t pr_id, time_t start, char *name);
int db_remove_active_cli(id_t id);

int db_add_program(id_t cli_id, id_t *pr_id, time_t start, char *name);
int db_get_last_pr_id(id_t *pr_id);
int db_set_pr_name(id_t cli_id, id_t pr_id, char *name);
int db_set_pr_endtime(id_t pr_id);
int db_append_cc(id_t pr_id, char *cc, size_t len);

#endif /* end of include guard: DB_H */
