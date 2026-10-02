CC = gcc
CFLAGS = -Wall -Wextra -std=c11

all: server

server: src/server.c
	$(CC) $(CFLAGS) src/server.c -o server

run: server
	./server 8080

clean:
	rm -f server