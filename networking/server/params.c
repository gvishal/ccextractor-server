#include "params.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int init_cfg()
{
	cfg.port = DFT_PORT;
	cfg.max_conn = DFT_MAX_CONN;
	cfg.wrong_pwd_delay = DFT_WRONG_PASSW_DELAY;
	cfg.buf_dir = DFT_BUF_FILE_DIR;
	cfg.arch_dir = DFT_ARCHIVE_DIR;
	cfg.create_logs = DFT_CREATE_LOGS;
	cfg.log_dir = DFT_LOG_DIR;
	cfg.log_clients = DFT_CLIENT_LOGS;
	cfg.use_pwd = DFT_USE_PWD;
	cfg.buf_max_lines = DFT_BUF_MAX_LINES;
	cfg.buf_min_lines = DFT_BUF_MIN_LINES;
	cfg.cce_path = DFT_CCEXTRACTOR_PATH;
	cfg.cce_output_dir = DFT_CCE_OUTPUT_DIR;
	cfg.db_host = DFT_DB_HOST;
	cfg.db_user = DFT_DB_USER;
	cfg.db_passwd = DFT_DB_PASSWORD;
	cfg.db_dbname = DFT_DB_DBNAME;
	cfg.pr_timeout = DFT_PR_TIMEOUT;

	if ((cfg.pwd = (char *) malloc(DFT_PASSW_LEN + 1)) == NULL) /* +1 for '\0' */
		return ERRNO;

	memset(cfg.pwd, 0, DFT_PASSW_LEN + 1);
	rand_str(cfg.pwd, DFT_PASSW_LEN);

	cfg.is_inited = TRUE;

	return 0;
}

int parse_config_file()
{
	assert(cfg.is_inited);

	FILE *fp = fopen(DFT_CONFIG_FILE, "r");
	if (fp == NULL)
	{
		_log("fopen() error (%s): %s\n", DFT_CONFIG_FILE, strerror(errno));
		return -1;
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read = 0;

	int key_len;
	int value_len;
	char *value;
	enum {boolean, number, string} val_type;
	int val_bool;
	int val_num;
	char *val_str;
	int rc = 1;

	while ((read = getline(&line, &len, fp)) != -1) 
	{
		key_len = 0;
		value_len = 0;

		while (strchr(" \t", *line)) line++;

		for (int i = 0; i < read; i++) {
			if ('=' == line[i])
			{
				/* skip spaces before '=' */
				int j = i - 1;
				while (j >= 0 && strchr(" \t", line[j])) j--;

				key_len = j + 1; 
				line[key_len] = '\0';

				/* skip spaces after '=' */
				j = i + 1;
				while (j < read && strchr(" \t", line[j])) j++;

				value = &line[j]; 
			}
			else if ('\n' == line[i])
			{
				/* skip spaces before '\n' */
				int j = i - 1;
				while (j >= 0 && strchr(" \t", line[j])) j--;

				line[j + 1] = '\0';
				value_len = j + 1 - (value - line); 
				break;
			} 
			else if (';' == line[i])
			{
				break;
			}
		}

		if ((key_len <= 0) || (value_len <= 0))
		{
			/* then this line is empty or it's error */
			char *p = line;
			while (*p == ' ' || *p == '\t') p++;

			if (!strchr("\n;", *p)) /* it includes case *p == '\0' */
			{
				rc = CFG_ERR;
				goto out;
			}

			continue;
		}

		if (strncmp(value, "true", value_len) == 0)
		{
			val_type = boolean;
			val_bool = TRUE;
		}
		else if (strncmp(value, "false", value_len) == 0)
		{
			val_type = boolean;
			val_bool = FALSE;
		}
		else if (*value == '"' && value[value_len - 1] == '"')
		{
			val_type = string;

			/* without '"', but with '\0' */
			if ((val_str = (char *) malloc(value_len - 1)) == NULL)
			{
				rc = ERRNO;
				goto out;
			}
			memcpy(val_str, value + 1, value_len - 1);
			val_str[value_len - 2] = '\0';
		}
		else
		{
			val_type = number;

			char *endptr;
			val_num = strtol(value, &endptr, 10);
			if (*endptr != '\0')
			{
				rc = CFG_ERR;
				goto out;
			}
		}

#if 0
		printf("%s => %s ", line, value);
		if (boolean == val_type)
			printf("(bool = %d)\n", val_bool);
		else if (string == val_type)
			printf("(string)\n");
		else if (number == val_type)
			printf("(int = %d)\n", val_num);
#endif

		if (strncmp(line, "port", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.port = val_num;
		}
		else if (strncmp(line, "max_connections", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.max_conn = val_num;
		}
		else if (strncmp(line, "password", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}

			if (cfg.pwd != NULL)
				free(cfg.pwd);

			cfg.pwd = val_str;
		}
		else if (strncmp(line, "wrong_password_delay", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.wrong_pwd_delay = val_num;
		}
		else if (strncmp(line, "buffer_files_dir", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;

			}
			cfg.buf_dir = val_str;
		}
		else if (strncmp(line, "archive_files_dir", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.arch_dir = val_str;
		}
		else if (strncmp(line, "create_logs", key_len) == 0)
		{
			if (boolean != val_type)
			{
				rc = CFG_BOOL;
				goto out;
			}
			cfg.create_logs = val_bool;
		}
		else if (strncmp(line, "logs_dir", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.log_dir = val_str;
		}
		else if (strncmp(line, "use_password", key_len) == 0)
		{
			if (boolean != val_type)
			{
				rc = CFG_BOOL;
				goto out;
			}
			cfg.use_pwd = val_bool;
		}
		else if (strncmp(line, "log_cliets_msg", key_len) == 0)
		{
			if (boolean != val_type)
			{
				rc = CFG_BOOL;
				goto out;
			}
			cfg.log_clients = val_bool;
		}
		else if (strncmp(line, "buffer_file_max_lines", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.buf_max_lines = val_num;
		}
		else if (strncmp(line, "buffer_file_min_lines", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.buf_min_lines = val_num;
		}
		else if (strncmp(line, "ccextractor_path", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.cce_path = val_str;
		}
		else if (strncmp(line, "ccextractor_output_dir", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.cce_output_dir = val_str;
		}
		else if (strncmp(line, "mysql_host", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.db_host = val_str;
		}
		else if (strncmp(line, "mysql_user", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.db_user = val_str;
		}
		else if (strncmp(line, "mysql_password", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.db_passwd = val_str;
		}
		else if (strncmp(line, "mysql_db_name", key_len) == 0)
		{
			if (string != val_type)
			{
				rc = CFG_STR;
				goto out;
			}
			cfg.db_dbname = val_str;
		}
		else if (strncmp(line, "program_change_timeout", key_len) == 0)
		{
			if (number != val_type)
			{
				rc = CFG_NUM;
				goto out;
			}
			cfg.pr_timeout = val_num;
		}
		else
		{
			rc = CFG_UNKNOWN;
			goto out;
		}
	}

out:
	if (line != NULL)
		free(line);
	fclose(fp);
	return rc;
}

int creat_dirs()
{
	struct stat st = {0};

	if (stat(cfg.buf_dir, &st) == -1 && _mkdir(cfg.buf_dir, MODE755) < 0)
	{
		_perror("_mkdir");
		return -1;
	}

	if (stat(cfg.log_dir, &st) == -1 && _mkdir(cfg.log_dir, MODE755) < 0)
	{
		_perror("_mkdir");
		return -1;
	}

	if (stat(cfg.arch_dir, &st) == -1 && _mkdir(cfg.arch_dir, MODE755) < 0)
	{
		_perror("_mkdir");
		return -1;
	}

	if (stat(cfg.cce_output_dir, &st) == -1 && _mkdir(cfg.cce_output_dir, MODE755) < 0)
	{
		_perror("_mkdir");
		return -1;
	}

	return 1;
}
