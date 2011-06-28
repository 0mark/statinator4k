include config.mk

CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -Os -s dwmstatus.c -o dwmstatus

all:
	${CC} ${CFLAGS} ${WITH_X11} ${WITH_MPD} ${WITH_NOTIFY}

clean:
	rm dwmstatus
