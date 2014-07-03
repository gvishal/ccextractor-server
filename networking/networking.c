#include "networking.h"

ssize_t 
readn(int fd, void *vptr, size_t n) 
{
	assert(n >= 0);

	size_t nleft;
	ssize_t nread;
	char *ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) 
	{
		if (NULL == vptr) {
			char c;
			nread = read(fd, &c, 1);
		}
		else
		{
			nread = read(fd, ptr, nleft);
		}

		if (nread < 0) 
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

/* block format: */
/* command | lenght        | data         | \r\n */
/* 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes */
ssize_t
read_block(int fd, char *command, char *buf, size_t *buf_len)
{
	assert(command != NULL);
	assert(buf != NULL);
	assert(buf_len != NULL);
	assert(*buf_len > 0);

	ssize_t rc;
	ssize_t nread = 0;

	if ((rc = readn(fd, command, 1)) < 0)
		return -1;
	else if ((size_t) rc != 1) 
		return 0;
	nread += rc;

	char len_str[INT_LEN] = {0};
	if ((rc = readn(fd, len_str, INT_LEN)) < 0)
		return -1;
	else if (rc != INT_LEN)
		return 0;
	nread += rc;

    size_t len = atoi(len_str);
	if (len <= 0) 
	{
		fprintf(stderr, "read_block() error: Wrong block size\n");
		return -1;
	}

	int ign_bytes = 0;
	if (len > *buf_len) 
	{
		ign_bytes = len - *buf_len;
		fprintf(stderr, 
				"read_block() warning: Buffer overflow, ignoring %d bytes\n",
				ign_bytes);
		len = *buf_len; 
	}

	if ((rc = readn(fd, buf, len)) < 0)
		return -1;
	else if ((size_t) rc != len)
		return 0;
	nread += rc;
	*buf_len = len;

	if ((rc = readn(fd, 0, ign_bytes)) < 0)
		return -1;
	else if ((size_t) rc != ign_bytes)
		return 0;
	nread += rc;

	char end[2] = {0}; 
	if ((rc = readn(fd, end, sizeof(end))) < 0)
		return -1;
	else if ((size_t) rc != sizeof(end))
		return 0;
	nread += rc;

	if (end[0] != '\r' || end[1] != '\n') {
		fprintf(stderr, "read_block() error: No end marker present\n");
		return -1;
	}

	return nread;
}

/* block format: */
/* command | lenght        | data         | \r\n */
/* 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes */
ssize_t
write_block(int fd, char command, const char *buf, size_t buf_len)
{
	assert(buf != NULL);
	assert(buf_len > 0);

	int rc;
	ssize_t nwritten = 0;

	if ((rc = write_byte(fd, command)) < 0)
		return -1;
	else if (rc != 1)
		return 0;
	nwritten++;

	char len_str[INT_LEN] = {0};
	snprintf(len_str, INT_LEN, "%d", buf_len);
	if ((rc = writen(fd, len_str, INT_LEN)) < 0)
		return -1;
	else if (rc != INT_LEN)
		return 0;
	nwritten += rc;

	if ((rc = writen(fd, buf, buf_len)) < 0)
		return -1;
	else if (rc != buf_len)
		return 0;
	nwritten += rc;

	if ((rc = write_byte(fd, '\r')) < 0)
		return -1;
	else if (rc != 1)
		return 0;

	nwritten++;
	if ((rc = write_byte(fd, '\n')) < 0)
		return -1;
	else if (rc != 1)
		return 0;
	nwritten++;

	return nwritten;
}

ssize_t 
write_byte(int fd, char ch)
{
	return writen(fd, &ch, 1);
}

ssize_t 
read_byte(int fd, char *ch)
{
	assert(ch != 0);

	return readn(fd, ch, 1);
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
