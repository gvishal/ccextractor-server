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
#define DFT_PR_TIMEOUT 2 * 40 * 60
#define DFT_PR_REPORT_TIME 1 * 60
#define DFT_MYSQL_TZ "+00:00"
#define DFT_ENV_TZ "UTC"
#define DFT_LOG_VERBOSE_LVL 4
#define DFT_STORE_CC_IN_DB 1

/** CCE output is read in infinte loop with specifed
 * delays in nanoseconds (10^9 sec) */
#define INF_READ_DELAY 300000000

/** Size of a buffer for network messages or for parsing cce output */
#define BUFFER_SIZE 20480

/** BIN format defined header lenght */
#define BIN_HEADER_LEN 11

/** Config data structure
 */
struct cfg_t
{
    /** Port number the server is binded */
	int port;

    /** Maximum number of active connections */
	unsigned max_conn;

    /** if true, clients are required to enter password */
	unsigned use_pwd : 1;

    /** Password c-string */
	char *pwd; 

    /** Server will ignore client for specified number of seconds,
     * when he enters wrong password */
	int wrong_pwd_delay;

    /** c-string with path to dir with buffer files for web site */
	char *buf_dir;

    /** maximum number of lines in buffer file
     * When lines number reaches this value, file is
     * cropped to buf_min_lines lines
     * @see delete_n_lines */
	unsigned buf_max_lines;

    /** mimimum number of lines in buffer file
     * @see buf_max_lines
     * @see delete_n_lines */
	unsigned buf_min_lines;

    /** c-string with path to dir with recieved cc files */
	char *arch_dir;

    /** if true, ouputs log messages to stderr */
	unsigned log_stderr : 1;

    /** c-string with path to dir with logs files */
	char *log_dir;

    /** c-string with path to ccextractor bin file */
	char *cce_path;

    /** c-string path to dir to ccextractor output files */
	char *cce_output_dir;

    /** MySQL host */
	char *db_host;

    /** MySQL username */
	char *db_user;

    /** MySQL password. if NULL, then no password */
	char *db_passwd;

    /** MySQL database name */
	char *db_dbname;

    /** Current program is changed to unknown after specified number seconds passes,
     * if no program name reported in pr_report_time */
	unsigned pr_timeout;

    /** Current program changes to unknown,
     * if no program name reported in specified number of second */
	unsigned pr_report_time;

    /** MySQL time zone, 'SET time_zone=mysql_tz' will quered */
	char *mysql_tz;

	/** Enviroment time zone (TZ varibale)
	 * @see set_tz */
	char *env_tz;

	/** Debug level
	 * @see printlog
	 */
	unsigned log_vlvl;

	/** if true, then recieved captions will be stored in data base and file system,
	 * if false, then captions will only be send to web site*/
	unsigned store_cc : 1;

    /** if true, then this structure is inited, ie values are set do default */
	unsigned is_inited : 1;
} cfg; /**< Global config structure instance */

/** Sets default values to cfg structre fields
 * @see cfg_t
 * @return negative number on error or 1, when no error occured */
int init_cfg();

/** Opens config file, parses it into cfg structure
 * @return negative number on error or 1, when no error occured */
int parse_config_file();

#endif /* end of include guard: PARAMS_H */
