#include "networking.h"
#include "utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>

#define DEBUG_OUT 1

/*
 * Writes data according to protocol to descriptor
 * block format:
 * command | lenght        | data         | \r\n
 * 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes
 */
ssize_t write_block(int fd, char command, const char *buf, size_t buf_len);

/*
 * Established connection to speciefied addres.
 * Returns socked id
 */
int tcp_connect(const char *addr, const char *port);

/*
 * Asks password from stdin, sends it to the server and waits for
 * it's response
 */
int ask_passwd(int sd);

/* Reads n bytes from descriptor */
ssize_t readn(int fd, void *vptr, size_t n);

/* Writes n bytes to descriptor */
ssize_t writen(int fd, const void *vptr, size_t n);

/* Convinence functions */
ssize_t write_byte(int fd, char status);
ssize_t read_byte(int fd, char *status);

#if DEBUG_OUT
void pr_command(char c);
#endif

int srv_sd = -1; /* Server socket descriptor */

void connect_to_srv(const char *addr, const char *port)
{
	if (NULL == addr)
	{
		printf("Server addres is not set\n");
		printf("Unable to connect\n");
		exit(EXIT_FAILURE);
	}

	if (NULL == port)
		port = "2048";

	printf("Connecting to %s:%s\n", addr, port);

	if ((srv_sd = tcp_connect(addr, port)) < 0)
	{
		printf("Unable to connect\n");
		exit(EXIT_FAILURE);
	}

	if (ask_passwd(srv_sd) < 0)
	{
		printf("Unable to connect\n");
		exit(EXIT_FAILURE);
	}

	printf("Connected to %s:%s\n", addr, port);
}

void net_send_header(const char *data, size_t len)
{
	assert(srv_sd > 0);

#if DEBUG_OUT
	fprintf(stderr, "[C] Sending header (len = %zd): \n", len);
	fprintf(stderr, "File created by %02X version %02X%02X\n", data[3], data[4], data[5]);
	fprintf(stderr, "File format revision: %02X%02X\n", data[6], data[7]);
#endif

	if (write_block(srv_sd, BIN_MODE, NULL, 0) <= 0)
	{
		printf("Can't send BIN header\n");
		return;
	}

	if (writen(srv_sd, data, len) <= 0)
	{
		printf("Can't send BIN header\n");
		return;
	}

	char ok;
	if (read_byte(srv_sd, &ok) != 1)
		return;

#if DEBUG_OUT
	fprintf(stderr, "[S] ");
	pr_command(ok);
	fprintf(stderr, "\n");
#endif

	if (ERROR == ok)
	{
		printf("Internal server error\n"); 
		return;
	}
}

void net_send_cc(const char *data, size_t len)
{
	assert(srv_sd > 0);

#if DEBUG_OUT
	fprintf(stderr, "[C] Sending %zd bytes\n", len);
#endif

	ssize_t rc;
	if ((rc = writen(srv_sd, data, len)) < 0)
		perror("write() error");
	if (rc != INT_LEN)
		return;

	return;
}

/* block format: */
/* command | lenght        | data         | \r\n */
/* 1 byte  | INT_LEN bytes | lenght bytes | 2 bytes */
ssize_t
write_block(int fd, char command, const char *buf, size_t buf_len)
{
#if DEBUG_OUT
	fprintf(stderr, "[C] ");
#endif

	int rc;
	ssize_t nwritten = 0;

	if ((rc = write_byte(fd, command)) < 0)
		return -1;
	else if (rc != 1)
		return 0;
	nwritten++;

#if DEBUG_OUT
	pr_command(command);
	fprintf(stderr, " ");
#endif

	char len_str[INT_LEN] = {0};
	snprintf(len_str, INT_LEN, "%zd", buf_len);
	if ((rc = writen(fd, len_str, INT_LEN)) < 0)
		return -1;
	else if (rc != INT_LEN)
		return 0;
	nwritten += rc;

#if DEBUG_OUT
	fwrite(len_str, sizeof(char), INT_LEN, stderr);
	fprintf(stderr, " ");
#endif

	if ((rc = writen(fd, buf, buf_len)) < 0)
		return -1;
	else if (rc != (int) buf_len)
		return 0;
	nwritten += rc;

#if DEBUG_OUT
	fwrite(buf, sizeof(char), buf_len - 2, stderr);
	fprintf(stderr, " ");
#endif

	if ((rc = write_byte(fd, '\r')) < 0)
		return -1;
	else if (rc != 1)
		return 0;

#if DEBUG_OUT
	fprintf(stderr, "\\r");
#endif

	nwritten++;
	if ((rc = write_byte(fd, '\n')) < 0)
		return -1;
	else if (rc != 1)
		return 0;
	nwritten++;

#if DEBUG_OUT
	fprintf(stderr, "\\n\n");
#endif

	return nwritten;
}

int tcp_connect(const char *host, const char *port)
{
	assert(host != NULL);
	assert(port != NULL);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *ai;
	int rc = getaddrinfo(host, port, &hints, &ai);
	if (rc != 0) {
		printf("getaddrinfo() error: %s\n", gai_strerror(rc));
		return -1;
	}

	struct addrinfo *p;
	int sockfd;
	/* Try each address until we sucessfully connect */
	for (p = ai; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, SOCK_STREAM, p->ai_protocol);

		if (-1 == sockfd) {
			printf("socket() error: %s\n", strerror(errno));
			if (p->ai_next != NULL)
				printf("trying next addres ...\n");

			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
			break;

		printf("connect() error: %s\n", strerror(errno));
		if (p->ai_next != NULL)
			printf("trying next addres ...\n");

		close(sockfd);
	}

	freeaddrinfo(ai);

	if (NULL == p)
		return -1;

	return sockfd;
}

int ask_passwd(int sd)
{
	assert(srv_sd > 0);

	struct termios old, new;
	int rc;
	size_t len = 0;
	char *pw = NULL;

	char ok;

	do {
		do {
			if (read_byte(sd, &ok) != 1)
			{
				printf("read() error: %s", strerror(errno));
				return -1;
			}

#if DEBUG_OUT
			fprintf(stderr, "[S] ");
			pr_command(ok);
			fprintf(stderr, "\n");
#endif

			if (OK == ok)
			{
				return 1;
			}
			else if (CONN_LIMIT == ok) 
			{
				printf("Too many connections to the server, try later\n");
				return -1;
			} 
			else if (ERROR == ok)
			{
				printf("Internal server error\n");
				return -1;
			}

		} while(ok != PASSWORD);

		printf("Enter password: ");
		fflush(stdout);

		if (tcgetattr(STDIN_FILENO, &old) != 0)
		{
			printf("tcgetattr() error: %s\n", strerror(errno));
		}

		new = old;
		new.c_lflag &= ~ECHO;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new) != 0)
		{
			printf("tcgetattr() error: %s\n", strerror(errno));
		}

		rc = getline(&pw, &len, stdin);
		rc--; /* -1 for \n */

		if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &old) != 0)
		{
			printf("tcgetattr() error: %s\n", strerror(errno));
		}

		printf("\n");
		fflush(stdout);

		if (write_block(sd, PASSWORD, pw, rc) < 0)
			return -1;

		if (read_byte(sd, &ok) != 1)
			return -1;

#if DEBUG_OUT
		fprintf(stderr, "[S] ");
		pr_command(ok);
		fprintf(stderr, "\n");
#endif

		if (WRONG_PASSWORD == ok)
		{
			printf("Wrong password\n");
			fflush(stdout);
		}
		else if (ERROR == ok)
		{
			printf("Internal server error\n");
			return -1;
		}
	} while(OK != ok);

	return 1;
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

#if DEBUG_OUT
void pr_command(char c)
{
	switch(c)
	{
		case OK:
			fprintf(stderr, "OK");
			break;
		case BIN_MODE:
			fprintf(stderr, "BIN_MODE");
			break;
		case WRONG_PASSWORD:
			fprintf(stderr, "WRONG_PASSWORD");
			break;
		case UNKNOWN_COMMAND:
			fprintf(stderr, "UNKNOWN_COMMAND");
			break;
		case ERROR:
			fprintf(stderr, "ERROR");
			break;
		case CONN_LIMIT:
			fprintf(stderr, "CONN_LIMIT");
			break;
		case PASSWORD:
			fprintf(stderr, "PASSWORD");
			break;
		default:
			fprintf(stderr, "UNKNOWN (%d)", (int) c);
			break;
	}
}
#endif

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
