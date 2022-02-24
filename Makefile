CC=gcc
LDFLAGS=-lpthread
CFLAGS=-g -pedantic -std=gnu17 -Wall -Wextra

.PHONY: all
all: nyuenc

nyuenc: nyuenc.o

nyuenc.o: nyuenc.c nyuenc.h

.PHONY: clean
clean:
	rm -f *.o nyuenc