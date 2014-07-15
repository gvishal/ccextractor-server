#include "utils.h"
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

void rand_str(char *s, size_t len) {
	for (size_t i = 0; i < len; i++)
		s[i] = rand() % (1 + '~' - ' ') + ' ';

	/* s[rand() % len] = '\t'; */
	/* s[rand() % len] = '\r'; */
	/* s[rand() % len] = '\n'; */

	return;
}

int main(int argc, char *argv[])
{
	(void) setlocale(LC_ALL, "");

	if (4 != argc) {
		fprintf(stderr, "Usage: %s host port file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	connect_to_srv(argv[1], argv[2]);

	FILE *fp = fopen(argv[3], "r");
	if (NULL == fp)
	{
		perror("fopen() error");
		exit(EXIT_FAILURE);
	}

	char *line;

	size_t len = 0;
	int read;

	int rand_num = 0;

	while(1)
	{
		if (0 != fseek(fp, 3, SEEK_SET)) { //skip bom
			perror("fseek");
			exit(EXIT_FAILURE);
		}

		while ((read = getline(&line, &len, fp)) != -1) {
			rand_num %= 15;
			if (0 == rand_num) 
			{
				rand_num = 0;
				char pr_name[40] = {0};
				rand_str(pr_name, 39);

				net_set_new_program(pr_name);
			}
			rand_num++;

			net_append_cc_n(line, read);

			net_send_cc();

			sleep(1);
		}
	}

	fclose(fp);

	return 0;
}
