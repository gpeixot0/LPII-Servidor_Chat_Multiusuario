CC = gcc
CFLAGS = -pthread

all: server client

server: serverM.c libtslog.h
	$(CC) $(CFLAGS) Servidor.c -o servidor

client: Cliente.c
	$(CC) $(CFLAGS) Cliente.c -o cliente

clean:
	rm -f server client
