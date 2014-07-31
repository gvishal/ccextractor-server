#include "utils.h"
#include "params.h"
#include "db.h"

#include <my_global.h>
#include <mysql.h>

int init_db()
{
	MYSQL *con = mysql_init(NULL);
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
