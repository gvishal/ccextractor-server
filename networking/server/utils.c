#include "utils.h"
#include "params.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>

ssize_t readn(int fd, void *vptr, size_t n) 
{
	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) 
	{
		if (NULL == vptr) 
		{
			char c;
			nread = read(fd, &c, 1);
		}
		else
		{
			nread = read(fd, ptr, nleft);
		}

		if (nread < 0) 
		{
			if (EINTR == errno)
				nread = 0;
			else if (EAGAIN == errno)
				break;
			else 
				return -1;
		}
		else if (0 == nread) 
		{
			break; /* EOF */
		}

		nleft -= nread;
		ptr += nread;
	}

	return n - nleft;
}

ssize_t writen(int fd, const void *vptr, size_t n)
{
	assert(fd > 0);
	assert((n > 0 && vptr != NULL) || (n == 0 && vptr == NULL));

	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) 
	{
		if ((nwritten = write(fd, ptr, nleft)) < 0) 
		{
			if (errno == EINTR)
				nwritten = 0;
			else 
				return -1;
		}
		else if (0 == nwritten) 
		{
			break; 
		}

		nleft -= nwritten;
		ptr += nwritten;
	}

	return n;
}

ssize_t write_byte(int fd, char ch)
{
	return writen(fd, &ch, 1);
}

ssize_t read_byte(int fd, char *ch)
{
	assert(ch != 0);

	return readn(fd, ch, 1);
}

const char *m_strerror(int rc)
{
	switch(rc)
	{
		case BLK_SIZE:
			return "Wrong block size";
		case END_MARKER:
			return "No end marker present";
		case ERRNO:
			return strerror(errno);
		default:
			return "Unknown";
	}
}

int open_log_file()
{
	if ((logfile.path = malloc(PATH_MAX)) == NULL)
	{
		logfatal("malloc");
		return -1;
	}
	memset(logfile.path, 0, PATH_MAX);
	logfile.fp = NULL;

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char time_buf[30] = {0};
	strftime(time_buf, 30, "%F=%H-%M-%S", t_tm);
	snprintf(logfile.path, PATH_MAX, "%s/%s.log", cfg.log_dir, time_buf);

	if ((logfile.fp = fopen(logfile.path, "w")) == NULL)
	{
		logfatal("fopen");
		return -1;
	}

	if (setvbuf(logfile.fp, NULL, _IONBF, 0) < 0)
	{
		logfatal("setvbuf");
		return -1;
	}

	printf("log file: %s\n", logfile.path);

	return 1;
}

void printlog(unsigned dlevel, int cli_id,
		const char *file, int line, const char *fmt, ...)
{
	assert(fmt != NULL);

	if ((dlevel > cfg.log_vlvl) && dlevel != FATAL)
		return;

	va_list args;
	va_start(args, fmt);

	static char logbuf[LOG_MSG_SIZE];
	char *end = logbuf;

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	end += strftime(end, 30, "[%H:%M:%S]", t_tm);

	if (cli_id == 0)
		end += sprintf(end, "[S]");
	else
		end += sprintf(end, "[%d]", cli_id);

	switch (dlevel)
	{
	case FATAL:
		end += sprintf(end, "[FATAL]");
		end += sprintf(end, "[%s:%d]", file, line);
		break;
	case ERR:
		end += sprintf(end, "[ERROR]");
		end += sprintf(end, "[%s:%d]", file, line);
		break;
	case WARNING:
		end += sprintf(end, "[WARNIG]");
		end += sprintf(end, "[%s:%d]", file, line);
		break;
	case INFO:
		end += sprintf(end, "[INFO]");
		break;
	case DEBUG:
		end += sprintf(end, "[DEBUG]");
		end += sprintf(end, "[%s:%d]", file, line);
		break;
	default:
		break;
	}

	end += sprintf(end, " ");

	end += vsprintf(end, fmt, args);

	end += sprintf(end, "\n");

	if (logfile.fp == NULL)
		logfile.fp = stderr;

	if (0 != lockf(fileno(logfile.fp), F_LOCK, 0))
		logerr("lockf");

	fprintf(logfile.fp, "%s", logbuf);

	if (logfile.fp != stderr && dlevel == FATAL)
		fprintf(stderr, "%s", logbuf);

	if (0 != lockf(fileno(logfile.fp), F_ULOCK, 0))
		logerr("lockf");

	va_end(args);
}

int m_signal(int sig, void (*func)(int))
{
	struct sigaction act;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(sig, &act, NULL)) {
		logerr("sigaction");
		return -1;
	}

	return 1;
}

int mkpath(const char *path, mode_t mode)
{
	char tmp[256];
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if ('/' == tmp[len - 1])
		tmp[len - 1] = 0;

	for (char *p = tmp + 1; *p; p++) 
	{
		if (*p != '/')
			continue;

		*p = 0;
		if (mkdir(tmp, mode) < 0)
		{
			if (errno != EEXIST)
			{
				logfatal("mkdir");
				return -1;
			}
		}
		*p = '/';
	}

	if (mkdir(tmp, mode) < 0) 
	{
		if (errno != EEXIST)
		{
			logfatal("mkdir");
			return -1;
		}
	}

	return 0;
}

void rand_str(char *s, size_t len)
{
	for (size_t i = 0; i < len; i++)
		s[i] = rand() % (1 + 'z' - 'a') + 'a';

	return;
}

int delete_n_lines(FILE **fp, char *filepath, size_t n)
{
	char *line = NULL;
	size_t len = 0;
	ssize_t nread = 0;
	int rc = 1;

	rewind(*fp);

	/* offset first lines */
	for (size_t i = 0; i < n; i++) 
	{
		if ((rc = getline(&line, &len, *fp)) < 0)

		{
			logerr("getline");
			goto out;
		}
	}

	char tmp_path[PATH_MAX] = {0};
	size_t l = strmov(tmp_path, filepath);
	tmp_path[l] = '_';

	FILE *tmp = fopen(tmp_path, "w+");
	if (NULL == tmp)
	{
		logerr("fopen");
		rc = -1;
		goto out;
	}

	if ((rc = setvbuf(tmp, NULL, _IOLBF, 0)) < 0)
	{
		logerr("setvbuf");
		fclose(tmp);
		goto out;
	}

	while ((nread = getline(&line, &len, *fp)) != -1)
		fwrite(line, sizeof(char), nread, tmp);

	fclose(*fp);

	*fp = tmp;

	if ((rc = rename(tmp_path, filepath)) < 0)
		logerrmsg("rename() %s --> %s error: %s",
				tmp_path, filepath, strerror(errno));

out:
	if (line != NULL)
		free(line);

	return rc;
}

char *nice_str(const char *s, size_t *len)
{
	if (NULL == s)
		return NULL;

	assert(*len > 0);

	char *ret;

	if ((ret = (char *) malloc(sizeof(char) * (*len + 1))) == NULL)
	{
		logerr("malloc"); /* TODO: sometimes it fails here */
		return NULL;
	}

	memset(ret, 0, sizeof(char) * (*len + 1));

	int j = 0;
	int is_sp = FALSE;
	for (unsigned i = 0; i < *len; i++) {
		if (!isprint(s[i]))
			continue;

		if (isspace(s[i]))
		{
			if (j != 0)
				is_sp = TRUE;
			continue;
		}

		if (is_sp)
		{
			ret[j] = ' ';
			j++;
			is_sp = FALSE;
		}

		ret[j] = s[i];
		j++;
	}

	*len = j;

	return ret;
}

int set_nonblocking(int fd)
{
	int f;
#ifdef O_NONBLOCK
	if ((f = fcntl(fd, F_GETFL, 0)) < 0)
		f = 0;

	return fcntl(fd, F_SETFL, f | O_NONBLOCK); 
#else
	f = 1;
	return ioctl(fd, FIONBIO, &f);
#endif
}

size_t strmov(char *dest, const char *src)
{
	const char *end = src;

	while ((*dest++ = *end++));

	return end - src - 1;
}

int set_tz()
{
	if (setenv("TZ", cfg.env_tz, 1) < 0)
	{
		logerr("setenv");
		return -1;
	}

	tzset();

	return 1;
}
