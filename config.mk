# statinator4k version (we start with 20.1 because we can!)
VERSION = 20.1

# output formater
FORMATER = "-DFORMAT_METHOD=\"formats_dwm.h\""
#FORMATER = "-DFORMAT_METHOD=\"formats_dwm_colorbar.h\""
#FORMATER = "-DFORMAT_METHOD=\"formats_dwm_sprinkles.h\""
#FORMATER = "-DFORMAT_METHOD=\"formats_html.h\""

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# X11 adds ~200k mem usage (needed for dwm / set root window title)
X11_INCS = -I/usr/X11R6/include
X11_LIBS = -L/usr/X11R6/lib -lX11
X11_FLAGS = -DUSE_X11

# Sockets are needed for cmus, mpd and mail support
SOCKET_FLAGS=-DUSE_SOCKETS

# dbus/notify adds ~250k mem usage
NOTIFY_INCS = `pkg-config --cflags dbus-1`
NOTIFY_LIBS = `pkg-config --libs dbus-1`
NOTIFY_FLAGS = -DUSE_NOTIFY
NOTIFY_CFILES = notify.c

# alsa mixer support adds ~400k mem usage (doh! ich HAVE to find a sane way for this!)
ALSAVOL_LIBS = -lasound
ALSAVOL_FLAGS = -DUSE_ALSAVOL

INCS = -I. -I/usr/include ${X11_INCS} ${NOTIFY_INCS}
LIBS = -L/usr/lib -lc ${X11_LIBS} ${NOTIFY_LIBS} ${ALSAVOL_LIBS}

CPPFLAGS = -DVERSION=\"${VERSION}\" ${X11_FLAGS} ${SOCKET_FLAGS} ${NOTIFY_FLAGS} ${ALSAVOL_FLAGS} ${FORMATER}
CFLAGS = -g -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
