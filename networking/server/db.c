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

