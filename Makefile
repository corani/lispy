CC = cc
CFLAGS = -std=c2x -Wall -Wextra -Wpedantic -Werror -ggdb
LDFLAGS = -ledit

all: lispy

lispy: main.c mpc/mpc.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)