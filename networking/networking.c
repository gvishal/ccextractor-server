#include "networking.h"

ssize_t 
readn(int fd, void *vptr, size_t n) 
{
	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) 
	{
		if ((nread = read(fd, ptr, nleft)) < 0) 
		{
			if (errno == EINTR)
			{
				nread = 0;
			}
			else 
			{
				perror("read() error");
				return -1;
			}
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

ssize_t 
writen(int fd, const void *vptr, size_t n)
{
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

ssize_t
read_block(int fd, char *buf, size_t *buf_len)
{
	ssize_t rc;
	ssize_t nread = 0;

	char len_str[INT_LEN] = {0};
	if ((rc = readn(fd, len_str, INT_LEN)) < 0) {
		return -1;
	} else if (rc != INT_LEN) {
		return 0;
	}
	nread += rc;

    size_t len = atoi(len_str);
	if (0 == len) {
		fprintf(stderr, "read_block() error: Wrong block size\n");
		return -1;
	}

	if (len > *buf_len) {
		len = *buf_len; //XXX: why?
	}

	if ((rc = readn(fd, buf, len)) < 0) {
		return -1;
	} else if ((size_t) rc != len) {
		return 0;
	}
	nread += rc;
	*buf_len = len;

	char end[2] = {0}; // for \r\n
	if ((rc = readn(fd, end, 2)) < 0) {
		return -1;
	} else if ((size_t) rc != 2) {
		return 0;
	}
	nread += rc;

	if (end[0] != '\r' || end[1] != '\n') {
		fprintf(stderr, "read_block() error: No end marker present\n");
		return -1;
	}

	return nread;
}

ssize_t
write_block(int fd, const char *buf, size_t buf_len)
{
	char len_str[INT_LEN + 1] = {0};
	memcpy(len_str, buf, INT_LEN);

    size_t len = atoi(len_str);
	if (0 == len) {
		fprintf(stderr, "write_block() error: Wrong block size\n");
		return -1;
	}
	if (len > buf_len) {
		len = buf_len;
	}

	return writen(fd, buf, INT_LEN + len);
}

ssize_t 
write_byte(int fd, char status)
{
	return writen(fd, &status, 1);
}

ssize_t 
read_byte(int fd, char *status)
{
	return readn(fd, status, 1);
}

void 
print_address(const struct sockaddr *sa, socklen_t salen)
{
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
	int result = getnameinfo(sa, salen, host, sizeof(host), 
							 serv, sizeof(serv), 0);
	if (0 != result) {
		fprintf(stderr, "getnameinfo() error: %s\n", gai_strerror(result));
	}

	printf("%s:%s", host, serv);
}
