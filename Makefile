CC=gcc -std=c2x
CFLAGS=-Wall -Wextra -ggdb

cbuild: cbuild.c cbuild.h
	$(CC) $(CFLAGS) -o $@ $<
