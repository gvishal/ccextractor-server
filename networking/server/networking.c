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
ssize_t read_block(int fd, char *command, char *buf, size_t *buf_len)
{
	assert(command != NULL);

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

	/* TODO: use strtol */
    size_t len = atoi(len_str);

	if (len > 0)
	{
		if (buf == NULL || buf_len == NULL)
			return BLK_SIZE;

		assert(*buf_len > 0);

		size_t ign_bytes = 0;
		if (len > *buf_len)
		{
			ign_bytes = len - *buf_len;
			/* XXX: Don't ignore */
			_log("read_block() warning: Buffer overflow, ignoring %d bytes\n",
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
	}

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

ssize_t write_block(int fd, char command, const char *buf, size_t buf_len)
{
	int rc;
	ssize_t nwritten = 0;

	if ((rc = write_byte(fd, command)) < 0)
		return -1;
	else if (rc != 1)
		return 0;
	nwritten++;

	char len_str[INT_LEN] = {0};
	snprintf(len_str, INT_LEN, "%zd", buf_len);
	if ((rc = writen(fd, len_str, INT_LEN)) < 0)
		return -1;
	else if (rc != INT_LEN)
		return 0;
	nwritten += rc;

	if ((rc = writen(fd, buf, buf_len)) < 0)
		return -1;
	else if (rc != (int) buf_len)
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

int bind_server(int port, int *fam)
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
			_log("socket() error: %s\n", strerror(errno));

			if (p->ai_next != NULL)
				printf("trying next addres ...\n");

			continue;
		}

		if (AF_INET6 == p->ai_family)
		{
			int no = 0;
			if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no)) < 0)
			{
				_perror("setsockopt");

				if (p->ai_next != NULL)
					printf("trying next addres ...\n");

				continue;
			}
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0)
		{

			_log("bind() error: %s\n", strerror(errno));
			if (p->ai_next != NULL)
				printf("trying next addres ...\n");

			close(sockfd);

			continue;
		}

		*fam = p->ai_family;

		break;
	}

	freeaddrinfo(ai);

	if (NULL == p)
		return B_SRV_ERR;

	if (0 != listen(sockfd, SOMAXCONN))
	{
		close(sockfd);
		return ERRNO;
	}

	return sockfd;
}
