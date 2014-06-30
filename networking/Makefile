# FLAGS = -Wall -Wextra

all: server client

server: server.c server.h networking.c networking.h
	gcc $(FLAGS) networking.c server.c -o server

client: client.c main.c client.h networking.c networking.h
	gcc $(FLAGS) networking.c main.c client.c -o client

clean:
	rm -rf server client
