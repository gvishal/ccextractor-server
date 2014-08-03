#include "utils.h"
#include "params.h"
#include "db.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/file.h>

#include <my_global.h>
#include <mysql.h>

MYSQL *con;

int init_db()
{
	if (mysql_library_init(0, NULL, NULL))
	{
		_log("could not initialize MySQL library\n");
		return -1;
	}

	return 1;
}

int db_conn()
{
	con = mysql_init(NULL);
	if (NULL == con)
	{
		mysql_perror("mysql_init", con);
		return -1;
	}

	if (mysql_real_connect(con, cfg.db_host, cfg.db_user, cfg.db_passwd,
				cfg.db_dbname, 0, NULL, 0) == NULL)
	{
		mysql_perror("mysql_real_connect", con);
		mysql_close(con);
		return -1;
	}

	return 1;
}

void db_close_conn()
{
	if (NULL == con)
		return;

	mysql_close(con);
	con = NULL;
}

int db_add_cli(const char *host, const char *serv, id_t *new_id)
{
	assert(host != NULL);
	assert(serv != NULL);

	char query[QUERY_LEN] = {0};
	char *end = query;

	end += strmov(end, "INSERT INTO clients (address, port, date) VALUES(\'");

	end += mysql_real_escape_string(con, end, host, strlen(host));

	end += strmov(end, "\', \'");

	end += mysql_real_escape_string(con, end, serv, strlen(serv));

	end += strmov(end, "\', NOW()) ;");

	if (lock_cli_tbl() < 0)
		return -1;

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		unlock_db();
		return -1;
	}

	if (db_get_last_id(new_id) < 0)
	{
		unlock_db();
		return -1;
	}

	if (unlock_db() < 0)
		return -1;

	return 1;
}

int db_get_last_id(id_t *new_id)
{
	if (mysql_query(con, "SELECT MAX(id) FROM clients ;"))
	{
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
	{
		mysql_perror("mysql_store_result", con);
		return -1;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (NULL == row[0])
	{
		_log("%s:%d error: row[0] == NULL", __FILE__, __LINE__);
		return -1;
	}

	*new_id = atoi(row[0]);

	return 1;
}

int db_add_active_cli(id_t id)
{
	assert(id > 0);

	char id_str[INT_LEN + 1] = {0};
	snprintf(id_str, INT_LEN, "%u", id);

	char query[QUERY_LEN] = {0};
	int rc = snprintf(query, QUERY_LEN,
			"INSERT INTO active_clients(id) VALUES(\'%s\') ;",
			id_str);

	if (mysql_real_query(con, query, rc))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int db_set_pr_arctive_cli(id_t id, id_t pr_id)
{
	assert(id > 0);
	assert(pr_id > 0);

	char query[QUERY_LEN] = {0};
	char *end = query;

	end += sprintf(end,
			"UPDATE active_clients SET program_id = %u WHERE id = %u LIMIT 1 ;",
			pr_id, id);

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int db_remove_active_cli(id_t id)
{
	assert(id > 0);

	char id_str[INT_LEN] = {0};
	snprintf(id_str, INT_LEN, "%u", id);

	char query[QUERY_LEN] = {0};
	int rc = snprintf(query, QUERY_LEN,
			"DELETE FROM active_clients WHERE id = \'%s\' LIMIT 1 ;",
			id_str);

	if (mysql_real_query(con, query, rc))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		unlock_db();
		return -1;
	}

	unlock_db();

	return 1;
}

int db_add_program(id_t cli_id, id_t *pr_id, time_t start, char *name)
{
	assert(cli_id > 0);
	assert(pr_id != NULL);

	if (lock_pr_tbl() < 0)
		return -1;

	char query[QUERY_LEN] = {0};
	char *end = query;

	if (NULL == name)
		end += strmov(end, "INSERT INTO programs (client_id, start_date) VALUES(\'");
	else
		end += strmov(end, "INSERT INTO programs (client_id, start_date, name) VALUES(\'");

	end += sprintf(end, "%u", cli_id);

	end += sprintf(end, "\', FROM_UNIXTIME(%lu)", (unsigned long) start);

	if (name != NULL)
	{
		end += strmov(end, ", \'");
		end += mysql_real_escape_string(con, end, name, strlen(name));
		*end++ = '\'';
	}

	end += strmov(end, ") ;");

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		unlock_db();
		return -1;
	}

	if (db_get_last_pr_id(pr_id) < 0)
	{
		unlock_db();
		return -1;
	}

	unlock_db();

	return 1;
}

int db_get_last_pr_id(id_t *pr_id)
{
	if (mysql_query(con, "SELECT MAX(id) FROM programs ;"))
	{
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
	{
		mysql_perror("mysql_store_result", con);
		return -1;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (NULL == row[0])
	{
		_log("%s:%d error: row[0] == NULL", __FILE__, __LINE__);
		return -1;
	}

	*pr_id = atoi(row[0]);

	return 1;
}

int db_set_pr_name(id_t pr_id, char *name)
{
	assert(pr_id > 0);
	assert(name != NULL);

	char query[QUERY_LEN] = {0};
	char *end = query;

	end += strmov(end, "UPDATE programs SET name = \'");

	end += mysql_real_escape_string(con, end, name, strlen(name));

	end += sprintf(end, "\' WHERE id = \'%u\' LIMIT 1 ;", pr_id);

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int db_set_pr_endtime(id_t pr_id)
{
	assert(pr_id > 0);

	char query[QUERY_LEN] = {0};
	char *end = query;

	end += sprintf(end,
			"UPDATE programs SET end_date = NOW() WHERE id = \'%u\' LIMIT 1 ;",
			pr_id);

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int lock_cli_tbl()
{
	if (mysql_query(con, "LOCK TABLES clients WRITE ;"))
	{
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int lock_pr_tbl()
{
	if (mysql_query(con, "LOCK TABLES programs WRITE;"))
	{
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}

int unlock_db()
{
	if (mysql_query(con, "UNLOCK TABLES"))
	{
		mysql_perror("mysql_real_query", con);
		return -1;
	}

	return 1;
}
