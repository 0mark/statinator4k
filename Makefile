include config.mk

CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -Os -s -lX11 dwmstatus.c -o dwmstatus

all:
	${CC} ${CFLAGS} ${WITH_MPD} ${WITH_NOTIFY} ${WITH_BATTERIES} ${WITH_WIFI} ${WITH_CPU}

clean:
	rm dwmstatus
