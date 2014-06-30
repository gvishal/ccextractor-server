#include "networking.h"
#include "server.h"
#include <poll.h>
#include <stropts.h>
#include <sys/ioctl.h>

int client_sock;

#define FALSE 0
#define TRUE 1

struct pollfd fds[MAX_CONN];
struct cli_t clients[MAX_CONN];
char passw[MAX_PASSWORD_LEN + 1] = {0};

int main(int argc, char *argv[])
{
	if (2 != argc) {
		fprintf(stderr, "Usage: %s port \n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* line buffering in stdout */
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf() error");
        exit(EXIT_FAILURE);
    }

	int listen_sd = bind_server(argv[1]);
	if (listen_sd < 0) {
		exit(EXIT_FAILURE);
	}

	printf("Server is binded to %s\n", argv[1]);
	gen_passw(passw);
	printf("Password: %s\n", passw);

	fds[0].fd = listen_sd;
	fds[0].events = POLLIN;
	clients[0].is_logged = 1;
	nfds_t nfds = 1;

	int i, j;
	int rc;
	int compress_array;

	char buffer[BUFFER_SIZE];

	while (1)
	{
		if (poll(fds, nfds, -1) < 0) /* no timeout */
		{ 
			perror("poll() error");
			break;
		}

		int current_size = nfds;
		for (i = 0; i < current_size; i++) {
			if (0 == fds[i].revents)
				continue;

			if (fds[i].revents & POLLHUP)
			{
				compress_array = TRUE;
				close_conn(i);
				continue;
			}

			if (fds[i].revents & POLLERR) 
			{
				printf("Error! Poll revents: %d\n", fds[i].revents);
			}

			if (fds[i].fd == listen_sd) 
			{
				struct sockaddr cliaddr;
				socklen_t clilen = sizeof(struct sockaddr);

				int new_sd;
				while((new_sd = accept(listen_sd, &cliaddr, &clilen)) >= 0) 
				{
					if (0 != (rc = getnameinfo(&cliaddr, clilen, 
							clients[nfds].host, sizeof(clients[nfds].host), 
							clients[nfds].serv, sizeof(clients[nfds].serv), 
							0)))
					{
						fprintf(stderr, "getnameinfo() error: %s\n",
								gai_strerror(rc));
					}

					fds[nfds].fd = new_sd;
					fds[nfds].events = POLLIN;
					clients[nfds].is_logged = 0;

					print_cli_addr(nfds);
					printf("Connected\n");

					nfds++;
				}

				if (EWOULDBLOCK != errno) /* accepted all of connections */
				{
					perror("accept() failed");
					goto end_server;
				}
			}
			else if (clinet_command(i) < 0)
			{
				compress_array = TRUE;
				close_conn(i);
			} 
		} /* End of loop through pollable descriptors */

		if (compress_array) {
			compress_array = FALSE;
			for (i = 0; i < nfds; i++) {
				if (fds[i].fd >= 0)
					continue;

				for(j = i; j < nfds; j++)
				{
					fds[j].fd = fds[j + 1].fd;
					clients[j].is_logged = clients[j + 1].is_logged;
					strcpy(clients[j].host, clients[j + 1].host);
					strcpy(clients[j].serv, clients[j + 1].serv);
				}
				i--;
				nfds--;
			}
		}
	}

end_server:
	for (i = 0; i < nfds; i++)
	{
		if(fds[i].fd >= 0)
			close(fds[i].fd);
	}
	return 0;
}

int bind_server(const char *port) 
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo *ai;
	int result = getaddrinfo(NULL, port, &hints, &ai);
	if (0 != result) 
	{
		fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(result));
		return -1;
	}

	struct addrinfo *p;
	int sockfd;
	/* Try each address until we sucessfully bind */
	for (p = ai; p != NULL; p = p->ai_next) 
	{
		sockfd = socket(p->ai_family, SOCK_STREAM, p->ai_protocol);

		if (-1 == sockfd) 
		{
			perror("socket() error");
			fprintf(stderr, "Skipping...\n");

			continue;
		}

		int opt_val = 1;
		if (setsockopt(sockfd, SOL_SOCKET,  SO_REUSEADDR,
				(char *)&opt_val, sizeof(opt_val)) < 0) 
		{
			perror("setsockopt() error");
			close(sockfd);

			fprintf(stderr, "Skipping...\n");
			continue;
		}

		if (ioctl(sockfd, FIONBIO, (char *)&opt_val) < 0)
		{
			perror("ioctl() error");
			close(sockfd);

			fprintf(stderr, "Skipping...\n");
			continue;
		}

		if (0 == bind(sockfd, p->ai_addr, p->ai_addrlen))
			break;

		perror("bind() error");
		fprintf(stderr, "Skipping...\n");

		close(sockfd);
	}

	if (NULL == p)
	{
		fprintf(stderr, "Could not bind to %s\n", port);
		return -1;
	}

	freeaddrinfo(ai);

	if (0 != listen(sockfd, SOMAXCONN))
	{
		perror("listen() error");
		return -1;
	}

	return sockfd;
}


void gen_passw(char *p)
{
	size_t i;
	size_t len = rand() % (1 + MAX_PASSWORD_LEN - 6) + 6;
	for (i = 0; i < len; i++)
	{
		p[i] = rand() % (1 + 'z' - 'a') + 'a';
	}

	return;
}

int clinet_command(int id)
{
	char c;
	int rc;

	if (read_byte(fds[id].fd, &c) <= 0)
		return -1;

	if (c != PASSW && !clients[id].is_logged)
	{
		print_cli_addr(id);
		fprintf(stderr, "No password presented\n");
		return -1;
	}

	char buf[BUFFER_SIZE] = {0};

	switch (c)
	{
	case PASSW:
		if (readn(fds[id].fd, buf, MAX_PASSWORD_LEN) <= 0)
			return -1;

		print_cli_addr(id);
		printf("Password: ");
		fwrite(buf, MAX_PASSWORD_LEN, sizeof(char), stdout);
		printf("\n");

		if (0 != memcmp(passw, buf, MAX_PASSWORD_LEN))
		{
			print_cli_addr(id);
			printf("Wrong password");
			printf("\n");

			if (write_byte(fds[id].fd, WRONG_PASSW) != 1)
				return -1;

			return 0;
		}

		clients[id].is_logged = 1;

		print_cli_addr(id);
		printf("Correct password\n");

		snprintf(clients[id].buf_file_path, BUF_FILE_PATH_LEN, 
				"%s/%s:%s.txt", BUF_FILE_DIR, clients[id].host, clients[id].serv);

		clients[id].buf_fp = fopen(clients[id].buf_file_path, "w+");
		if (clients[id].buf_fp == NULL)
		{
			perror("fopen() error");
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		if (setvbuf(clients[id].buf_fp, NULL, _IOLBF, 0) < 0) {
			perror("setvbuf() error");
			exit(EXIT_FAILURE);
		}

		print_cli_addr(id);
		printf("tmp file: %s\n", clients[id].buf_file_path);

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		break;

	case CC:
		if ((rc = read_block(fds[id].fd, buf, sizeof(buf))) <= 0)
			return -1;

		print_cli_addr(id);
		printf("Caption block, size: ");
		fwrite(buf, INT_LEN, sizeof(char), stdout);
		printf("\n");

		if (0 != flock(fileno(clients[id].buf_fp), LOCK_EX)) {
			perror("flock() error");
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}


		char t[INT_LEN] = {0};
		sprintf(t, "%d", clients[id].current_time);
		fwrite(t, sizeof(char), INT_LEN, clients[id].buf_fp);
		fwrite(buf, sizeof(char), rc, clients[id].buf_fp);

		/* XXX: remove first line from buf_fp when current_time > BUF_FILE_LINES */
		if (clients[id].current_time >= BUF_FILE_LINES)
		{
			if (delete_first_buf_line(id) < 0)
				return -1;
		}

		if (0 != flock(fileno(clients[id].buf_fp), LOCK_UN)) {
			perror("flock() error");
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		clients[id].current_time++;

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		break;

	default:
		print_cli_addr(id);
		fprintf(stderr, "Unsupported command: %d\n", (int)c);

		write_byte(fds[id].fd, WRONG_COMMAND);
		return -1;
	}

	return 1;
}

int delete_first_buf_line(int id)
{
	char *line = NULL;
	size_t len = 0;
	int rc;
	int n;

	rewind(clients[id].buf_fp);

	/* offset first line */
	char line_str[INT_LEN];
	fread(line_str, sizeof(char), INT_LEN, clients[id].buf_fp);
	fflush(clients[id].buf_fp);

	char buf[BUFFER_SIZE];
	rc = read_block(fileno(clients[id].buf_fp), buf, sizeof(buf)); 

	FILE *tmp = fopen(TMP_FILE_PATH, "w+");
	if (setvbuf(tmp, NULL, _IOLBF, 0) < 0) {
		perror("setvbuf() error");
		return -1;
	}

	while(1)
	{
		memset(line_str, 0, INT_LEN);
		if (fread(line_str, sizeof(char), INT_LEN, clients[id].buf_fp) != INT_LEN)
			break;

		fflush(clients[id].buf_fp);

		if ((rc = read_block(fileno(clients[id].buf_fp), buf, sizeof(buf))) == 0)
			break;
		else if (rc < 0)
			return -1;

		fwrite(line_str, sizeof(char), INT_LEN, tmp);
		fwrite(buf, sizeof(char), rc, tmp);
	}

	fclose(clients[id].buf_fp);
	clients[id].buf_fp = tmp;

	if (rename(TMP_FILE_PATH, clients[id].buf_file_path) != 0)
	{
		perror("rename() error");
		write_byte(fds[id].fd, SERV_ERROR);
		return -1;
	}

	return 1;
}

void print_cli_addr(int id)
{
	printf("%s:%s [%d] ", clients[id].host, clients[id].serv, fds[id].fd);
	/* printf("%d (%s:%s) ", fds[id].fd, clients[id].host, clients[id].serv); */
}

void close_conn(int id)
{
	print_cli_addr(id);
	printf("Disconnected\n");

	if (clients[id].buf_fp != NULL)
	{
		if (unlink(clients[id].buf_file_path) < 0) 
			perror("unlink() error");
		fclose(clients[id].buf_fp);
	}

	close(fds[id].fd);
	fds[id].fd = -1;
	clients[id].is_logged = 0;
	clients[id].buf_fp = NULL;
}
