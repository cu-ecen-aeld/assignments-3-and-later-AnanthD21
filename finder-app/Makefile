CROSS_COMPILE = 
CC = $(CROSS_COMPILE)gcc

CFLAGS = -g -Wall -Werror

all: writer

default: writer

writer: writer.o
	$(CC) $< -o $@
	
writer.o: writer.c
	$(CC) $(CFLAGS) -c $< -o $@
	
clean: 
	rm -f writer writer.o

.PHONY: clean

