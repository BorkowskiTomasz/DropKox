all: libdrop.a

CC=gcc -I.
CFLAGS = -fPIC -Wall -pedantic -std=c99

libdrop.a: drop.o
	ar crs libdrop.a drop.o 
.PHONY: all	
clean:
	rm drop.o libdrop.a
