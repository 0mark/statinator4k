# dbus/notify adds ~250k mem usage
WITH_NOTIFY=-DUSE_NOTIFY `pkg-config --cflags --libs dbus-1` notify.c
