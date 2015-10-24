CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread -lnsl -lrt

OBJS = nethelp.o dfs.o

all: dfs

nethelp.o: nethelp.c
        $(CC) $(CFLAGS) -c nethelp.c

dfs.o: dfs.c
        $(CC) $(CFLAGS) -c dfs.c

dfs: dfs.o nethelp.o
        $(CC) $(LDFLAGS) -o $@ dfs.o nethelp.o

clean:
        rm -f dfs *.o *~ core
