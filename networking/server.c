#include "networking.h"
#include "server.h"
#include <poll.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <limits.h> 

#define FALSE 0
#define TRUE 1

#define PASSW_LEN 16

struct pollfd fds[MAX_CONN];
struct cli_t clients[MAX_CONN];
char passw[PASSW_LEN + 1] = {0};

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

	my_signal(SIGALRM, unmute_clients);

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
		if (poll(fds, nfds, -1) < 0) 
		{ 
			if (EINTR == errno) 
				continue;

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

					write_byte(new_sd, PASSW);
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
					memcpy(&clients[j], &clients[j + 1], sizeof(struct cli_t));
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
			close_conn(i);
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
	size_t len = rand() % (1 + PASSW_LEN - 6) + 6;
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
	size_t len = BUFFER_SIZE;

	char buf[BUFFER_SIZE] = {0};
	if ((rc = read_block(fds[id].fd, &c, buf, &len)) <= 0)
		return -1;

	if (clients[id].muted_since > 0)
		return 0;

	if (c != PASSW && !clients[id].is_logged)
	{
		print_cli_addr(id);
		fprintf(stderr, "No password presented\n");
		return -1;
	}

	switch (c)
	{
	case PASSW:
		print_cli_addr(id);
		printf("Password: ");
		fwrite(buf, len, sizeof(char), stdout);
		printf("\n");

		if (0 != strcmp(passw, buf))
		{
			print_cli_addr(id);
			printf("Wrong password");
			printf("\n");

			clients[id].muted_since = time(NULL);

			alarm(WRONG_PASSW_DELAY);

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
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		print_cli_addr(id);
		printf("tmp file: %s\n", clients[id].buf_file_path);

		if (write_byte(fds[id].fd, OK) != 1)
			return -1;

		update_users_file();
		break;

	case CC:
		print_cli_addr(id);
		printf("Caption block, size: %d\n", len);

		if (0 != flock(fileno(clients[id].buf_fp), LOCK_EX)) {
			perror("flock() error");
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		fprintf(clients[id].buf_fp, "%d %d ", clients[id].cur_line, len);
		fwrite(buf, sizeof(char), len, clients[id].buf_fp);
		fprintf(clients[id].buf_fp, "\r\n");

		if (0 != flock(fileno(clients[id].buf_fp), LOCK_UN)) {
			perror("flock() error");
			write_byte(fds[id].fd, SERV_ERROR);
			return -1;
		}

		clients[id].cur_line++;

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
	memset(&clients[id], 0, sizeof(struct cli_t));

	update_users_file();
}

int update_users_file()
{
	FILE *fp = fopen(USERS_FILE_PATH, "w+");

	int i;
	for (i = 1; i < MAX_CONN; i++) /* 0 for listener sock */
	{
		if (!clients[i].is_logged)
			continue;
		
		fprintf(fp, "%s:%s\n", clients[i].host, clients[i].serv);
	}

	fclose(fp);
}

void unmute_clients()
{
	int i;
	for (i = 1; i < MAX_CONN; i++) /* 0 for listener sock */
	{
		if (clients[i].is_logged || fds[i].fd == 0 || 
				clients[i].muted_since == 0)
		{
			continue;
		}

		if (time(NULL) - clients[i].muted_since < WRONG_PASSW_DELAY) 
			continue;

		clients[i].muted_since = 0;
		/* Ask for password */
		write_byte(fds[i].fd, PASSW); 
	}
}

void my_signal(int sig, void (*func)(int)) 
{
	struct sigaction act;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if (sigaction(sig, &act, NULL)) {
		perror("sigaction() error");
		exit(EXIT_FAILURE);
	}

	return;
}
