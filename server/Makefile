CFLAGS=-g -O0 -std=gnu17 -Wall -Wextra -pedantic -Werror

CROSS_COMPILE ?=
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
AS=$(CROSS_COMPILE)as
NM=$(CROSS_COMPILE)nm
RANLIB=$(CROSS_COMPILE)ranlib

all: aesdsocket

clean:
	$(RM) aesdsocket *.o

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $^
