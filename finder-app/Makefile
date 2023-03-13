CFLAGS=-g -O0 -std=c17 -Wall -Wextra -pedantic -Werror

CROSS_COMPILE ?=
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
AS=$(CROSS_COMPILE)as
NM=$(CROSS_COMPILE)nm
RANLIB=$(CROSS_COMPILE)ranlib

all: writer

clean:
	$(RM) writer *.o

writer: writer.c
	$(CC) $(CFLAGS) -o $@ $^
