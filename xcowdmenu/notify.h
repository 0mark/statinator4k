#include <dbus/dbus.h>

// default message delay in seconds
#define EXPIRE_DEFAULT 10

// multiply requested delay by this
// (slow it down since only one line)
#define EXPIRE_MULT    2

typedef struct _notification {
	dbus_uint32_t nid;
	time_t started_at;
	time_t expires_after;
	char closed;

	char appname[20];
	char summary[64];
	char body[256];
	struct _notification *next;
} notification;

// initialize notifications
char notify_init(char debug_enabled);

// returns the first current notification into status (number of total messages in n)
notification *notify_get_message(int *n);

// check the dbus for notifications (1=something happened, 0=nothing)
char notify_check();
