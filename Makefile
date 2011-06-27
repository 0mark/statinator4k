include config.mk

CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -Os -s dwmstatus.c -o dwmstatus

all:
	${CC} ${CFLAGS} ${WITH_NOTIFY} ${WITH_X11}

clean:
	rm dwmstatus
