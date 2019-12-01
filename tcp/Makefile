CC = gcc
CFLAGS = -g -pthread

all:
	$(CC) $(CFLAGS) -o client_linux client/main.c
	$(CC) $(CFLAGS) -o server_linux server/main.c
	rm -rf *.dSYM

clean:
	rm -f client_linux server_linux *.stderr *.stdout