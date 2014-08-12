#include "networking.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <locale.h>

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

#define BUF_SIZE 20480

#define ull unsigned long long

void rand_str(char *s, size_t len)
{
	srand(time(NULL));
    for (size_t i = 0; i < len; i++)
        s[i] = rand() % (1 + 'z' - 'a') + 'a';

    return;
}

int main(int argc, char *argv[])
{
	(void) setlocale(LC_ALL, "");

	if (5 != argc) {
		fprintf(stderr, "Usage: %s host port file delay\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char name[8] = {0};
	rand_str(name, 7);
	connect_to_srv(argv[1], argv[2], name);

	FILE *fp = fopen(argv[3], "r");
	if (NULL == fp)
	{
		perror("fopen() error");
		exit(EXIT_FAILURE);
	}

	char buf[BUF_SIZE] = {0};
	char *p = buf;

	size_t rc = 0;

	if ((rc = fread(buf, sizeof(char), 11, fp)) != 11)
	{
		printf("%d, Premature end of file!\n", __LINE__);
		exit(EXIT_FAILURE);
	}

	if (memcmp(buf, "\xCC\xCC\xED", 3))
	{
		printf("no header\n");
		exit(0);
	}

	net_send_header(buf, 11);

	uint16_t blk_cnt;
	size_t len;

	int delay = atoi(argv[4]);

	while (!feof(fp))
	{
		p = buf;

		if ((rc = fread(p, sizeof(char), 10, fp)) != 10)
		{
			printf("%d, Premature end of file!\n", __LINE__);
			exit(EXIT_FAILURE);
		}
		
		p += rc;
		if ((blk_cnt = *((uint16_t *)(p - 2))) == 0)
			continue;


		if ((len = blk_cnt * 3) > BUF_SIZE) 
		{
			fprintf(stderr, "buffer overflow\n");
			exit(EXIT_FAILURE);
		}

		if ((rc = fread(p, sizeof(char), len, fp)) != len)
		{
			printf("%d, Premature end of file!\n", __LINE__);
			exit(EXIT_FAILURE);
		}
		p += rc;

		net_send_cc(buf, len + 10);

		nanosleep((struct timespec[]){{0, delay}}, NULL);
	}

	fclose(fp);

	return 0;
}
