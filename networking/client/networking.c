#include "networking.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

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

int connect_to_addr(const char *host, const char *port)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *ai;
	int result = getaddrinfo(host, port, &hints, &ai);
	if (0 != result) {
		fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(result));
		return -1;
	}

    struct addrinfo *p;
	int sockfd;
	/* Try each address until we sucessfully connect */
    for (p = ai; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, SOCK_STREAM, p->ai_protocol);

		if (-1 == sockfd) {
			struct sockaddr_in *sin = 
				(struct sockaddr_in *) (p->ai_addr);
			char abuf[INET_ADDRSTRLEN];

			if (NULL == inet_ntop(p->ai_family, &sin->sin_addr, abuf,
						INET_ADDRSTRLEN)) 
			{
				perror("inet_ntop");
			}
			perror("socket");
			fprintf(stderr, "Skipping...\n");

			continue;
		}

		if (1 != connect(sockfd, p->ai_addr, p->ai_addrlen))
			break;

		perror("connect");
		fprintf(stderr, "Skipping...\n");

		close(sockfd);
    }

    if (NULL == p) {
    	fprintf(stderr, "Could not connect to %s:%s\n", host, port);
		return -1;
	}

	freeaddrinfo(ai);

	return sockfd;
}

void
check_passwd(int fd)
{
	struct termios old, new;
	int rc;
	size_t len = 0;
	char len_str[INT_LEN] = {0};
	int i;
	char *pw = NULL;

	char ok;

	do {
		do {
			if (read_byte(fd, &ok) <= 0)
				return;
		} while(ok != PASSW);

		printf("Enter password: ");

		if (tcgetattr(STDIN_FILENO, &old) != 0) 
		{
			perror("tcgetattr() error");
			exit(EXIT_FAILURE);
		}

		new = old;
		new.c_lflag &= ~ECHO;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new) != 0)
		{
			perror("tcgetattr() error");
			exit(EXIT_FAILURE);
		}

		rc = getline(&pw, &len, stdin);
		rc--; /* -1 for \n */

		if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &old) != 0)
		{
			perror("tcgetattr() error");
			exit(EXIT_FAILURE);
		}
		printf("\n");

		write_block(fd, PASSW, pw, rc);

		read_byte(fd, &ok);

		if (WRONG_PASSW == ok) {
			printf("Wrong password\n");
		}

	} while(OK != ok);

	return;
}
