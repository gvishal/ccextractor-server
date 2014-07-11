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
	cfg.arch_info_filename = DFT_ARCHIVE_INFO_FILENAME;
	cfg.create_logs = DFT_CREATE_LOGS;
	cfg.log_dir = DFT_LOG_DIR;
	cfg.log_clients = DFT_CLIENT_LOGS;
	cfg.use_pwd = DFT_USE_PWD;
	cfg.buf_max_lines = DFT_BUF_MAX_LINES;
	cfg.buf_min_lines = DFT_BUF_MIN_LINES;

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
		_log("fopen() error: %s\n", strerror(errno));
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
	int rc;

	while ((read = getline(&line, &len, fp)) != -1) 
	{
		key_len = 0;
		value_len = 0;

		for (int i = 0; i < read; i++) {
			if ('=' == line[i])
			{
				key_len = i - 1; /* -1 for ' ' */
				value = &line[i + 2]; /* skip '=' and ' ' */
			}
			else if (';' == line[i])
			{
				value_len = i - (value - line); /* skip ';' */
				break;
			} 
			else if ('#' == line[i])
			{
				break;
			}
		}

		if ((key_len <= 0) || (value_len <= 0))
		{
			/* then this line is empty or it's error */
			char *p = line;
			while ((' ' == *p) || ('\t' == *p)) p++;
			if ((*p != '\n') && (*p != '#'))
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
			if ((val_num = atoi(value)) <= 0)
			{
				rc = CFG_ERR;
				goto out;
			}
		}

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
		else  
		{
			rc = CFG_UNKNOWN;
			goto out;
		}
	}

out:
	return rc;
	fclose(fp);
}
