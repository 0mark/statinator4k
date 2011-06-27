# X11 adds ~200k mem usage (needed for dwm / set root window title)
WITH_X11=-DUSE_X11 -lX11

# MPD Support
WITH_MPD=-DUSE_MPD

# dbus/notify adds ~250k mem usage
WITH_NOTIFY=-DUSE_NOTIFY `pkg-config --cflags --libs dbus-1` notify.c
