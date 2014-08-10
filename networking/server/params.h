#ifndef PARAMS_H
#define PARAMS_H

#include <sys/stat.h>

/* Default config constants: */
#define DFT_CONFIG_FILE "./server.ini"
#define DFT_PORT 2048
#define DFT_MAX_CONN 10
#define DFT_BUF_FILE_DIR "./tmp"
#define DFT_BUF_FILE_LINES 20
#define DFT_LOG_DIR "./logs"
#define DFT_USE_PWD 1
#define DFT_WRONG_PASSW_DELAY 2
#define DFT_PASSW_LEN 10
#define DFT_CREATE_LOGS 0
#define DFT_ARCHIVE_DIR "./cc"
#define DFT_CLIENT_LOGS 1
#define DFT_BUF_MAX_LINES 40
#define DFT_BUF_MIN_LINES 30
#define DFT_CCEXTRACTOR_PATH "./ccextractor"
#define DFT_CCE_OUTPUT_DIR "./cce"
#define DFT_DB_HOST "localhost"
#define DFT_DB_USER "root"
#define DFT_DB_PASSWORD NULL
#define DFT_DB_DBNAME "cce"
#define DFT_PR_TIMEOUT 2 * 40 * 60 /* sec */

#define INF_READ_DELAY 300000000 /* Nano sec less than 10^9 */
#define BUFFER_SIZE 20480
#define BIN_HEADER_LEN 11
#define DB_LOCK "db_lock"

#define MODE755 S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

struct cfg_t
{
	int port;
	unsigned max_conn;
	unsigned use_pwd : 1;
	char *pwd;
	int wrong_pwd_delay;
	char *buf_dir;
	unsigned buf_max_lines;
	unsigned buf_min_lines;
	char *arch_dir;
	unsigned create_logs : 1;
	unsigned log_clients : 1;
	char *log_dir;
	char *cce_path;
	char *cce_output_dir;
	char *db_host;
	char *db_user;
	char *db_passwd;
	char *db_dbname;
	unsigned pr_timeout;

	unsigned is_inited : 1;
} cfg;

int init_cfg();

int parse_config_file();

int creat_dirs();

#endif /* end of include guard: PARAMS_H */
