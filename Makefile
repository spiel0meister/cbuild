CC=gcc
CFLAGS=-Wall -Wextra -ggdb

cbuild: cbuild.c cbuild.h
	$(CC) $(CFLAGS) -o $@ $<
