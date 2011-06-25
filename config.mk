WITH_CPU=-DUSE_CPU

WITH_WIFI=-DUSE_WIFI

WITH_BATTERIES=-DUSE_BATTERIES

# dbus/notify adds ~250k mem usage
WITH_NOTIFY=-DUSE_NOTIFY `pkg-config --cflags --libs dbus-1` notify.c
