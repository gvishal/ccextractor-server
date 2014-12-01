FLAGS = -Wall -Wextra -pedantic
ALL_FLAGS = -std=gnu99 -g 

main: main.c networking.c networking.h 
	gcc $(ALL_FLAGS) $(FLAGS) main.c networking.c -o main

run: main
	./main 127.0.0.1 2048 data/the.tudors.bin 30000000 

prun: main
	./main 127.0.0.1 2048 data/145.bin 30000000 &
	./main 127.0.0.1 2048 data/the.tudors.bin 30000000 &
	./main 127.0.0.1 2048 data/89.bin 30000000 &

clean:
	rm -rf main
