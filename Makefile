
CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -Os -s -lX11 dwmstatus.c -o dwmstatus

# comment any of these to disable

# libmpd adds ~1mb mem usage
#WITH_MPD=-DUSE_MPD -lmpd

# dbus/notify adds ~250k mem usage
WITH_NOTIFY=-DUSE_NOTIFY `pkg-config --cflags --libs dbus-1` notify.c

# if it isn't a laptop this doesn't make sense
WITH_BATTERIES=-DUSE_BATTERIES

# if its a wired connection this doesn't make sense
WITH_WIFI=-DUSE_WIFI

all:
	${CC} ${CFLAGS} ${WITH_MPD} ${WITH_NOTIFY} ${WITH_BATTERIES} ${WITH_WIFI}

clean:
	rm dwmstatus
