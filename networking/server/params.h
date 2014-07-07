#ifndef PARAMS_H
#define PARAMS_H

#define DFT_CONFIG_FILE "./server.cfg"
#define DFT_PORT 2048
#define DFT_MAX_CONN 10
#define DFT_BUF_FILE_DIR "./tmp"
#define DFT_BUF_FILE_LINES 20
#define DFT_LOG_DIR "./logs"
#define DFT_WRONG_PASSW_DELAY 2
#define DFT_PASSW_LEN 10
#define DFT_CREATE_LOGS 0
#define DFT_ARCHIVE_DIR "./cc"
#define DFT_CLIENT_LOGS 1

#define USERS_FILE_PATH "connections.txt"
#define ARCHIVE_FILEPATH_LEN 50

struct cfg_t
{
	int port;
	int max_conn;
	char *pwd;
	int wrong_pwd_delay;
	char *buf_dir;
	char *arch_dir;
	unsigned create_logs : 1;
	unsigned log_clients : 1;
	char *log_dir;


	unsigned is_inited : 1;
} cfg;

int init_cfg();

int parse_config_file();

#endif /* end of include guard: PARAMS_H */

