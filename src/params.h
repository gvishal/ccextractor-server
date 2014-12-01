#ifndef PARAMS_H
#define PARAMS_H

#include <sys/stat.h>

/* Default config constants: */
#define DFT_CONFIG_FILE "./server.ini"
#define DFT_PORT 2048
#define DFT_MAX_CONN 10
#define DFT_BUF_FILE_DIR "./webbuffer"
#define DFT_BUF_FILE_LINES 20
#define DFT_LOG_DIR "./logs"
#define DFT_USE_PWD 1
#define DFT_WRONG_PASSW_DELAY 2
#define DFT_PASSW_LEN 10
#define DFT_LOG_STDERR 0
#define DFT_ARCHIVE_DIR "./ccfiles"
#define DFT_BUF_MAX_LINES 40
#define DFT_BUF_MIN_LINES 30
#define DFT_CCEXTRACTOR_PATH "./ccextractor"
#define DFT_CCE_OUTPUT_DIR "./ccextractorio"
#define DFT_DB_HOST "localhost"
#define DFT_DB_USER "root"
#define DFT_DB_PASSWORD NULL
#define DFT_DB_DBNAME "ccrepository"
#define DFT_PR_TIMEOUT 2 * 40 * 60 /* sec */
#define DFT_PR_REPORT_TIME 1 * 60 /* sec */
#define DFT_MYSQL_TZ "+00:00"
#define DFT_ENV_TZ "UTC"
#define DFT_LOG_VERBOSE_LVL 4
#define DFT_STORE_CC_IN_DB 1

#define INF_READ_DELAY 300000000 /* Nano sec less than 10^9 */
#define BUFFER_SIZE 20480
#define BIN_HEADER_LEN 11

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
	unsigned log_stderr : 1;
	char *log_dir;
	char *cce_path;
	char *cce_output_dir;
	char *db_host;
	char *db_user;
	char *db_passwd;
	char *db_dbname;
	unsigned pr_timeout;
	unsigned pr_report_time;
	char *mysql_tz;
	char *env_tz;
	unsigned log_vlvl;
	unsigned store_cc : 1;

	unsigned is_inited : 1;
} cfg;

int init_cfg();

/* errors values: */
#define CFG_ERR -4 /* Can't parse config file */
#define CFG_NUM -5 /* Number expected */
#define CFG_STR -6 /* String expected */
#define CFG_BOOL -7 /* true/false expected */
#define CFG_UNKNOWN -8 /* Unknown key-value pair */
int parse_config_file();

#endif /* end of include guard: PARAMS_H */
