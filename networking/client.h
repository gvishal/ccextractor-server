#ifndef CLIENT_H
#define CLIENT_H

int connect_to_addr(const char *host, const char *port);

void check_passwd(int fd);

#endif /* end of include guard: CLIENT_H */

