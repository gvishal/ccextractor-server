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
#include <sys/stat.h>
#include <sys/ioctl.h>

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
				return ERRNO;
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
	assert(vptr != NULL);
	assert(n > 0);

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
			{
				nwritten = 0;
			}
			else 
			{
				perror("write() error");
				return -1;
			}
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
		case CFG_ERR:
			return "Can't parse config file";
		case CFG_NUM:
			return "Number expected";
		case CFG_STR:
			return "String expected";
		case CFG_BOOL:
			return "Boolean expected";
		case CFG_UNKNOWN:
			return "Unknown key-value pair";
		case B_SRV_GAI:
			return gai_strerror(errno);
		case B_SRV_ERR:
			return "Coudn't bind to the port";
		case DEL_L_EOF:
			return "Unexpected end-of-file";
		case ERRNO:
			return strerror(errno);
		default:
			return "Unknown";
	}
}

void c_log(int cli_id, const char *fmt, ...)
{
	/* TODO: locks for concurrent access */
	if (!cfg.log_clients && cli_id != 0)
		return;

	va_list args;
	va_start(args, fmt);	

	time_t t = time(NULL);
	struct tm *t_tm = localtime(&t);
	char buf[30] = {0};
	strftime(buf, 30, "%H:%M:%S", t_tm);
	fprintf(stderr, "%s ", buf);

	if (cli_id == 0)
		fprintf(stderr, "[S] ");
	else 
		fprintf(stderr, "[%d] ", cli_id);

	vfprintf(stderr, fmt, args);

	fflush(stderr);

	va_end(args);
}

void _signal(int sig, void (*func)(int)) 
{
	struct sigaction act;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(sig, &act, NULL)) {
		_log(0, "sigaction() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return;
}

int _mkdir(const char *dir, mode_t mode) 
{
	char tmp[256];
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", dir);
	len = strlen(tmp);
	if ('/' == tmp[len - 1])
		tmp[len - 1] = 0;

	for (char *p = tmp + 1; *p; p++) 
	{
		if (*p != '/')
			continue;

		*p = 0;
		if (mkdir(tmp, mode) < 0 ) 
		{
			if (errno != EEXIST)
				return ERRNO;
		}
		*p = '/';
	}

	if (mkdir(tmp, mode) < 0) 
	{
		if (errno != EEXIST)
			return ERRNO;
	}

	return 0;
}

void rand_str(char *s, size_t len)
{
	for (size_t i = 0; i < len; i++)
		s[i] = rand() % (1 + 'z' - 'a') + 'a';

	return;
}

int delete_n_lines(FILE **fp, const char *filepath, size_t n)
{
	char *line = NULL;
	size_t len = 0;
	int rc;

	rewind(*fp);

	/* offset first lines */
	for (size_t i = 0; i < n; i++) 
	{
		if ((rc = getline(&line, &len, *fp)) < 0)
			return DEL_L_EOF;
	}

	FILE *tmp = fopen(TMP_FILE_PATH, "w+");
	if (NULL == tmp)
		return ERRNO;

	if (setvbuf(tmp, NULL, _IOLBF, 0) < 0) 
	{
		fclose(tmp);
		return ERRNO;
	}

	while ((rc = getline(&line, &len, *fp)) != -1)
		fwrite(line, sizeof(char), rc, tmp);

	fclose(*fp);

	*fp = tmp;

	if (rename(TMP_FILE_PATH, filepath) != 0)
		return ERRNO;

	return 1;
}

char *nice_str(const char *s, size_t *len)
{
	if (NULL == s)
		return NULL;

	assert(*len > 0);

	char *ret;

	if ((ret = (char *) malloc(sizeof(char) * (*len))) == NULL)
	{
		_log("malloc() error: %s\n", strerror(errno));
		return NULL;
	}

	memset(ret, 0, sizeof(char) * (*len));

	int j = 0;
	for (unsigned i = 0; i < *len; i++) {
		if (!isprint(s[i]))
			continue;

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
