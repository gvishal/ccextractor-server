#ifndef DB_H
#define DB_H

#include <sys/types.h>

#define QUERY_LEN 2048

int init_db();
int db_conn();
void db_close_conn();
int creat_tables();

int lock_cli_tbl();
int lock_pr_tbl();
int unlock_db();

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
