CC=clang
CFLAGS=-O2 -g -Wall -Wno-missing-braces


all: allrgb


sino.o: sino.c sino.h
	$(CC) -c $(CFLAGS) sino.c


allrgb.o: allrgb.c sino.h
	$(CC) -c $(CFLAGS) allrgb.c


allrgb: sino.o allrgb.o
	$(CC) -o allrgb sino.o allrgb.o -lm


