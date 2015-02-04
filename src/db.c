#include "utils.h"
#include "params.h"
#include "db.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/file.h>

#include <my_global.h>
#include <mysql.h>
#include <errmsg.h>

int is_db_created;
MYSQL *con;

int init_db() {
	if (mysql_library_init(0, NULL, NULL))
	{
		logfatalmsg("Could not initialize MySQL library");
		return -1;
	}

	if (db_conn() < 0)
		return -1;

	if (creat_tables() < 0)
	{
		logfatalmsg("Couldn't create tables");
		return -1;
	}

	if (query("TRUNCATE active_clients ;") < 0)
	{
		logfatalmsg("Couldn't truncate active_clients table");
		return -1;
	}

	db_close_conn();

	return 1;
}

int creat_tables()
{
	char q[QUERY_LEN];
	sprintf(q, "CREATE DATABASE IF NOT EXISTS %s ;", cfg.db_dbname);
	if (query(q) < 0)
		return -1;

	sprintf(q, "USE %s ;", cfg.db_dbname);
	if (query(q) < 0)
		return -1;

	if (query(
		"CREATE TABLE IF NOT EXISTS `clients` ("
		"   `id` int(11) NOT NULL AUTO_INCREMENT,"
		"   `ip` varchar(256) COLLATE utf8_bin NOT NULL,"
		"   `port` varchar(6) COLLATE utf8_bin NOT NULL,"
		"   `date` timestamp NOT NULL,"
		"   `description` varchar(300) COLLATE utf8_bin DEFAULT NULL,"
		"  PRIMARY KEY (`id`)"
		") ENGINE=InnoDB  DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;"
		) < 0)
	{
		return -1;
	}

	if (query(
		" CREATE TABLE IF NOT EXISTS `programs` ("
		"   `id` int(11) NOT NULL AUTO_INCREMENT,"
		"   `cli_id` int(11) NOT NULL,"
		"   `start_date` timestamp NULL DEFAULT NULL,"
		"   `end_date` timestamp NULL DEFAULT NULL,"
		"   `name` varchar(300) COLLATE utf8_bin DEFAULT NULL,"
		"   `cc_data` mediumtext COLLATE utf8_bin,"
		"   PRIMARY KEY (`id`),"
		"   FOREIGN KEY (`cli_id`) REFERENCES clients(id)"
		" ) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;"
		) < 0)
	{
		return -1;
	}

	if (query(
		" CREATE TABLE IF NOT EXISTS `active_clients` ("
		"   `cli_id` int(11) NOT NULL,"
		"   `pr_id` int(11) DEFAULT NULL,"
		"   `pr_name` varchar(300) COLLATE utf8_bin DEFAULT NULL,"
		"   `pr_start_date` timestamp NULL DEFAULT NULL,"
		"   `cli_description` varchar(300) COLLATE utf8_bin DEFAULT NULL,"
		"   FOREIGN KEY (`cli_id`) REFERENCES clients(id),"
		"   FOREIGN KEY (`pr_id`) REFERENCES programs(id)"
		" ) ENGINE=InnoDB DEFAULT CHARSET=latin1;"
		) < 0)
	{
		return -1;
	}

	is_db_created = TRUE;

	return 1;
}

int db_conn()
{
	con = mysql_init(NULL);
	if (NULL == con)
	{
		logmysqlfatal("mysql_init", con);
		return -1;
	}

	char *db = cfg.db_dbname;
	if (!is_db_created)
		db = NULL;

	if (mysql_real_connect(con, cfg.db_host, cfg.db_user, cfg.db_passwd, db,
				0, NULL, 0) == NULL)
	{
		logmysqlfatal("mysql_real_connect", con);
		mysql_close(con);
		return -1;
	}

	char q[QUERY_LEN];
	sprintf(q, "SET time_zone='%s'; ", cfg.mysql_tz);

	if (query(q) < 0)
		return -1;

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

	char q[QUERY_LEN] = {0};
	char *end = q;

	end += strmov(end, "INSERT INTO clients (ip, port, date) VALUES(\'");

	end += mysql_real_escape_string(con, end, host, strlen(host));

	end += strmov(end, "\', \'");

	end += mysql_real_escape_string(con, end, serv, strlen(serv));

	end += strmov(end, "\', UTC_TIMESTAMP()) ;");

	if (lock_cli_tbl() < 0)
		return -1;

	int rc = 1;

	if ((rc = query(q)) < 0)
		goto out;

	if ((rc = db_get_last_id(new_id)) < 0)
		goto out;

out:
	if (unlock_db() < 0)
		return -1;

	return rc;
}

int db_get_last_id(id_t *new_id)
{
	if (query("SELECT MAX(id) FROM clients ;") < 0)
		return -1;

	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
	{
		logmysqlerr("mysql_store_result", con);
		return -1;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (NULL == row[0])
	{
		logfatalmsg("empty response");
		mysql_free_result(result);
		return -1;
	}

	*new_id = atoi(row[0]);

	mysql_free_result(result);
	return 1;
}

int db_set_cc_desc(id_t cli_id, const char *desc)
{
	assert(cli_id > 0);
	assert(desc != NULL);

	char q[QUERY_LEN] = {0};
	sprintf(q, "UPDATE clients "
			   "SET description = \'%s\' "
			   "WHERE id = %u ;", desc, cli_id);

	if (query(q) < 0)
		return -1;

	sprintf(q, "UPDATE active_clients "
			   "SET cli_description = \'%s\' "
			   "WHERE cli_id = %u ;", desc, cli_id);

	if (query(q) < 0)
		return -1;

	return 1;
}

int db_add_active_cli(id_t cli_id)
{
	assert(cli_id > 0);

	char q[QUERY_LEN] = {0};
	sprintf(q, "INSERT INTO active_clients(cli_id) VALUES(\'%u\') ;", cli_id);

	return query(q);
}

int db_set_pr_arctive_cli(id_t cli_id, id_t pr_id, time_t start, char *name)
{
	assert(cli_id > 0);

	char q[QUERY_LEN] = {0};
	char *end = q;

	end += sprintf(end,
			"UPDATE active_clients "
			"SET pr_start_date = FROM_UNIXTIME(%lu)",
			(unsigned long) start);

	if (pr_id > 0)
		end += sprintf(end, ", pr_id = %u ", pr_id);

	if (name != NULL)
		end += sprintf(end, ", pr_name = \'%s\' ", name);

	end += sprintf(end, "WHERE cli_id = %u LIMIT 1 ;", cli_id);

	return query(q);
}

int db_remove_active_cli(id_t cli_id)
{
	assert(cli_id > 0);

	char q[QUERY_LEN] = {0};
	sprintf(q, "DELETE FROM active_clients WHERE cli_id = \'%u\' LIMIT 1 ;", cli_id);

	return query(q);
}

int db_add_program(id_t cli_id, id_t *pr_id, time_t start, char *name)
{
	assert(cli_id > 0);
	assert(pr_id != NULL);

	char q[QUERY_LEN] = {0};
	char *end = q;

	if (NULL == name)
		end += strmov(end, "INSERT INTO programs (cli_id, start_date) VALUES(\'");
	else
		end += strmov(end, "INSERT INTO programs (cli_id, start_date, name) VALUES(\'");

	end += sprintf(end, "%u", cli_id);

	end += sprintf(end, "\', FROM_UNIXTIME(%lu) ", (unsigned long) start);

	if (name != NULL)
	{
		end += strmov(end, ", \'");
		end += mysql_real_escape_string(con, end, name, strlen(name));
		*end++ = '\'';
	}

	end += strmov(end, ") ;");

	if (lock_pr_tbl() < 0)
		return -1;

	int rc;
	if ((rc = query(q)) < 0)
		goto out;

	if ((rc = db_get_last_pr_id(pr_id)) < 0)
		goto out;

out:
	if (unlock_db() < 0)
		return -1;

	return rc;
}

int db_get_last_pr_id(id_t *pr_id)
{
	if (query("SELECT MAX(id) FROM programs ;") < 0)
		return -1;

	MYSQL_RES *result = mysql_store_result(con);
	if (NULL == result)
	{
		logmysqlerr("mysql_store_result", con);
		return -1;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (NULL == row[0])
	{
		logerrmsg("empty response");
		mysql_free_result(result);
		return -1;
	}

	*pr_id = atoi(row[0]);

	mysql_free_result(result);
	return 1;
}

int db_set_pr_name(id_t cli_id, id_t pr_id, char *name)
{
	assert(pr_id > 0);
	assert(name != NULL);

	char q[QUERY_LEN] = {0};
	char *end = q;

	end += strmov(end, "UPDATE programs SET name = \'");
	end += mysql_real_escape_string(con, end, name, strlen(name));
	end += sprintf(end, "\' WHERE id = \'%u\' LIMIT 1 ;", pr_id);

	if (query(q) < 0)
		return -1;

	end = q;

	end += strmov(end, "UPDATE active_clients SET pr_name = \'");
	end += mysql_real_escape_string(con, end, name, strlen(name));
	end += sprintf(end, "\' WHERE cli_id = \'%u\' LIMIT 1 ;", cli_id);

	if (query(q) < 0)
		return -1;

	return 1;
}

int db_set_pr_endtime(id_t pr_id)
{
	assert(pr_id > 0);

	char q[QUERY_LEN] = {0};

	sprintf(q, "UPDATE programs SET end_date = UTC_TIMESTAMP() WHERE id = \'%u\' LIMIT 1 ;",
			pr_id);

	return query(q);
}

int db_append_cc(id_t pr_id, char *cc, size_t len)
{
	assert(pr_id > 0);
	assert(len > 0);
	assert(cc != NULL);

	char q[QUERY_LEN] = {0};
	char *e = q;

	e += strmov(e, "UPDATE programs SET cc_data = IFNULL(CONCAT(cc_data, \'");

	e += mysql_real_escape_string(con, e, cc, len);

	e += strmov(e, "\'), \'");

	e += mysql_real_escape_string(con, e, cc, len);

	e += sprintf(e, "\') WHERE id = \'%u\' LIMIT 1 ;", pr_id);

	return query(q);
}

int lock_cli_tbl()
{
	return query("LOCK TABLES clients WRITE ;");
}

int lock_pr_tbl()
{
	return query("LOCK TABLES programs WRITE;");
}

int unlock_db()
{
	return query("UNLOCK TABLES ;");
}

int query(const char *q)
{
    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGUSR1);
    sigaddset(&newmask, SIGUSR2);

	if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
		logerr("sigprocmask");

	int rc = 1;

again:
	if (mysql_real_query(con, q, strlen(q)))
	{
		unsigned int err = mysql_errno(con);
		if (err == CR_SERVER_GONE_ERROR || err == CR_SERVER_LOST)
		{
			logdebugmsg("MySQL server has gone away, reconnecting");
			if (db_conn() > 0)
				goto again;
		}

		rc = -1;

		logerrmsg("MySQL query failed: %s", q);
		logmysqlerr("mysql_real_query", con);
	}

	if (sigprocmask(SIG_SETMASK, &oldmask, 0) < 0)
		logerr("sigprocmask");

	return rc;
}
