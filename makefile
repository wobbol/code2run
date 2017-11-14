CC = clang
DEBUG = -ggdb

all: srv cli

srv: srv.c
	$(CC) srv.c -o srv $(DEBUG)

cli: cli.c
	$(CC) cli.c -o cli $(DEBUG)
