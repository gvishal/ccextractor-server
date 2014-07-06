#include "networking.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/ioctl.h>

/* block format: */
/* command | lenght        | data         | \r\n    */
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
		return ERRNO;
	else if ((size_t) rc != 1) 
		return 0;
	nread += rc;

	char len_str[INT_LEN] = {0};
	if ((rc = readn(fd, len_str, INT_LEN)) < 0)
		return ERRNO;
	else if (rc != INT_LEN)
		return 0;
	nread += rc;

    size_t len = atoi(len_str);
	if (len <= 0) 
		return BLK_SIZE;

	int ign_bytes = 0;
	if (len > *buf_len) 
	{
		ign_bytes = len - *buf_len;
		/* XXX: Don't ignore */
		_log(0, "read_block() warning: Buffer overflow, ignoring %d bytes\n",
				ign_bytes);
		len = *buf_len; 
	}

	if ((rc = readn(fd, buf, len)) < 0)
		return ERRNO;
	else if ((size_t) rc != len)
		return 0;
	nread += rc;
	*buf_len = len;

	if ((rc = readn(fd, 0, ign_bytes)) < 0)
		return ERRNO;
	else if ((size_t) rc != ign_bytes)
		return 0;
	nread += rc;

	char end[2] = {0}; 
	if ((rc = readn(fd, end, sizeof(end))) < 0)
		return ERRNO;
	else if ((size_t) rc != sizeof(end))
		return 0;
	nread += rc;

	if (end[0] != '\r' || end[1] != '\n')
		return END_MARKER;

	return nread;
}

int bind_server(int port) 
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	char port_str[INT_LEN] = {0};
	snprintf(port_str, INT_LEN, "%d", port);

	struct addrinfo *ai;
	int rc = getaddrinfo(NULL, port_str, &hints, &ai);
	if (0 != rc) 
	{
		errno = rc;
		return B_SRV_GAI;
	}

	struct addrinfo *p;
	int sockfd;
	/* Try each address until we sucessfully bind */
	for (p = ai; p != NULL; p = p->ai_next) 
	{
		sockfd = socket(p->ai_family, SOCK_STREAM, p->ai_protocol);

		if (-1 == sockfd) 
		{
			_log(0, "socket() error: %s\n", strerror(errno));
			_log(0, "trying next address");

			continue;
		}

		int opt_val = 1;
		if (setsockopt(sockfd, SOL_SOCKET,  SO_REUSEADDR,
				(char *)&opt_val, sizeof(opt_val)) < 0) 
		{
			_log(0, "setsockopt() error: %s\n", strerror(errno));
			_log(0, "trying next address");
			close(sockfd);

			continue;
		}

		if (ioctl(sockfd, FIONBIO, (char *)&opt_val) < 0)
		{
			_log(0, "ioctl() error: %s\n", strerror(errno));
			_log(0, "trying next address");
			close(sockfd);

			continue;
		}

		if (0 == bind(sockfd, p->ai_addr, p->ai_addrlen))
			break;

		_log(0, "bind() error: %s\n", strerror(errno));
		_log(0, "trying next address");
		close(sockfd);
	}

	freeaddrinfo(ai);

	if (NULL == p)
		return B_SRV_ERR;

	if (0 != listen(sockfd, SOMAXCONN))
		return ERRNO;

	return sockfd;
}
