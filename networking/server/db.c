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
int dblock_fd;

int init_db()
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

	if ((dblock_fd = open(DB_LOCK_FILE, O_RDWR)) < 0)
	{
		_perror("open");
		mysql_close(con);
		return -1;
	}

	return 1;
}

int db_add_cli(const char *host, const char *serv, id_t *new_id)
{
	assert(host != NULL);
	assert(serv != NULL);

	char query[QUERY_LEN] = {0};
	char *end = query;

	strcpy(end, "INSERT INTO clients (address, port, date) VALUES(\'");
	end += strlen(query);

	end += mysql_real_escape_string(con, end, host, strlen(host));

	strcpy(end, "\', \'");
	end += strlen(end);

	end += mysql_real_escape_string(con, end, serv, strlen(serv));

	strcpy(end, "\', NOW()) ;");
	end += strlen(end);

	if (lock_db() < 0)
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

	return 1;
}

int db_add_program(id_t cli_id, id_t *pgrm_id, time_t start, char *name)
{
	assert(cli_id > 0);
	assert(pgrm_id != NULL);

	if (lock_db() < 0)
		return -1;

	char query[QUERY_LEN] = {0};
	char *end = query;

	if (NULL == name)
		strcpy(end, "INSERT INTO programs (client_id, start_date) VALUES(\'");
	else
		strcpy(end, "INSERT INTO programs (client_id, start_date, name) VALUES(\'");
	end += strlen(query);

	end += sprintf(end, "%u", cli_id);

	strcpy(end, "\', FROM_UNIXTIME(");
	end += strlen(end);

	end += sprintf(end, "%lu", (unsigned long) start);

	*end++ = ')';

	if (name != NULL)
	{
		strcpy(end, ", \'");
		end += strlen(end);
		end += mysql_real_escape_string(con, end, name, strlen(name));
		*end++ = '\'';
	}

	strcpy(end, ") ;");
	end += strlen(end);

	if (mysql_real_query(con, query, end - query))
	{
		_log("MySQL query: %s\n", query);
		mysql_perror("mysql_real_query", con);
		unlock_db();
		return -1;
	}

	if (db_get_last_pgrm_id(pgrm_id) < 0)
	{
		unlock_db();
		return -1;
	}

	return 1;
}

int db_get_last_pgrm_id(id_t *pgrm_id)
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

	*pgrm_id = atoi(row[0]);

	return 1;
}

int lock_db()
{
	if (0 != flock(dblock_fd, LOCK_EX))
	{
		_perror("flock");
		close(dblock_fd);
		return -1;
	}

	return 1;
}

int unlock_db()
{
	if (0 != flock(dblock_fd, LOCK_UN))
	{
		_perror("flock");
		close(dblock_fd);
		return -1;
	}

	return 1;
}

