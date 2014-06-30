#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>

#include <locale.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#include "networking.h"
#include "client.h"

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
	char pw[MAX_PASSWORD_LEN + 1] = {0};
	char ok;

	do {
		printf("Enter password: ");
		scanf("%s", pw);

		write_byte(fd, PASSW);
		writen(fd, pw, MAX_PASSWORD_LEN);

		read_byte(fd, &ok);

		if (WRONG_PASSW == ok) {
			printf("Wrong password\n");
		}

	} while(WRONG_PASSW == ok);

	return;
}
