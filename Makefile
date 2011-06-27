include config.mk

CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -Os -s -lX11 dwmstatus.c -o dwmstatus

all:
	${CC} ${CFLAGS} ${WITH_NOTIFY}

clean:
	rm dwmstatus
