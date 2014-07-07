#include "client.h"
#include "utils.h"
#include "networking.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

int 
main(int argc, char *argv[])
{
	(void) setlocale(LC_ALL, "");

	if (4 != argc) {
		fprintf(stderr, "Usage: %s host port file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    int sockfd = connect_to_addr(argv[1], argv[2]);
    if (sockfd < 0) {
        exit(EXIT_FAILURE);
	}

	check_passwd(sockfd);

	fprintf(stdout, "Connected to %s:%s\n", argv[1], argv[2]);

	FILE *fp = fopen(argv[3], "r");
	if (NULL == fp)
	{
		perror("fopen() error");
		exit(EXIT_FAILURE);
	}

	char *line;

	size_t len = 0;
	int read;

	while(1)
	{
		if (0 != fseek(fp, 3, SEEK_SET)) { //skip bom
			perror("fseek");
			exit(EXIT_FAILURE);
		}

		while ((read = getline(&line, &len, fp)) != -1) {
			read -= 2; // for \r\n

			write_block(sockfd, CC, line, read);

			fwrite(line, sizeof(char), read + 2, stdout);
			fflush(stdout);

			char ok;
			read_byte(sockfd, &ok);

			sleep(1);
		}
	}

	fclose(fp);
	close(sockfd);

	return 0;
}