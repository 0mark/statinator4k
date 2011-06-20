// A very basic libnotify daemon
// original by Jeremy Jay

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <pwd.h>
#include <stdlib.h>
#include <linux/limits.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "notify.h"
#include "draw.h"

#define INRECT(x,y,rx,ry,rw,rh) ((x) >= (rx) && (x) < (rx)+(rw) && (y) >= (ry) && (y) < (ry)+(rh))
#define MIN(a,b)                ((a) < (b) ? (a) : (b))
#define MAX(a,b)                ((a) > (b) ? (a) : (b))
#define MAXNOTES                10

#define DEBUG(...) if( DEBUGGING ) fprintf(stderr, __VA_ARGS__)
char DEBUGGING=1;

notification *messages=NULL;

dbus_uint32_t curNid = 1;
dbus_uint32_t serial = 0xDEADBEEF;
DBusConnection* dbus_conn;

char notify_Notify(DBusMessage *msg);
char notify_GetCapabilities(DBusMessage *msg);
char notify_GetServerInformation(DBusMessage *msg);
char notify_CloseNotification(DBusMessage *msg);
char notify_NotificationClosed(unsigned int nid, unsigned int reason);
void setup(void);
void xrun(void);

/* dmenu stuff */
//static char text[BUFSIZ];
static int bh, mw, mh;
//static int inputw = 0;
//static int lines = 0;
static int monitor = -1;
//static int promptw;
//static size_t cursor = 0;
static const char *font = NULL;
//static const char *prompt = NULL;
static const char *normbgcolor = "#cccccc";
static const char *normfgcolor = "#000000";
static const char *selbgcolor  = "#0066ff";
static const char *selfgcolor  = "#ffffff";
static unsigned long normcol[ColLast];
static unsigned long selcol[ColLast];
static Atom utf8;
static Bool topbar = True;
static DC *dc;
//static Item *items = NULL;
//static Item *matches, *sel;
//static Item *prev, *curr, *next;
static Window root, win;
/* /dmenu stuff */
static char *notis[MAXNOTES];

// Command from file
char command[256];

char notify_init(char debug_enabled) {
	DBusError dbus_err;
	int ret;

	dbus_error_init(&dbus_err);
	dbus_conn=NULL;

	dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &dbus_err);
	if (NULL == dbus_conn)
		return 0;

	ret = dbus_bus_request_name(dbus_conn, "org.freedesktop.Notifications", DBUS_NAME_FLAG_REPLACE_EXISTING , &dbus_err);
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret)
		return 0;

	DEBUGGING=debug_enabled;
	
        const char* config = getenv("XDG_CONFIG_HOME");
        if(!config)
        {
                struct passwd *pw = getpwuid(getuid());
                config = pw->pw_dir;
        }
        
        char filename[PATH_MAX];
        sprintf( filename, "%s/cow-notify/config", config );
        sprintf( command, "xcowsay" );
        
        FILE* fp;
        fp = fopen(filename, "r");
        if(fp)
        {
                fgets( command, 256, fp );
                fclose(fp);
        }

	dbus_error_free(&dbus_err);

	dc = initdc();
	initfont(dc, font);
	setup();

	return 1;
}

void
setup(void) {
	int x, y, screen;
	XSetWindowAttributes wa;
#ifdef XINERAMA
	int n;
	XineramaScreenInfo *info;
#endif

	screen = DefaultScreen(dc->dpy);
	root = RootWindow(dc->dpy, screen);
	utf8 = XInternAtom(dc->dpy, "UTF8_STRING", False);

	normcol[ColBG] = getcolor(dc, normbgcolor);
	normcol[ColFG] = getcolor(dc, normfgcolor);
	selcol[ColBG] = getcolor(dc, selbgcolor);
	selcol[ColFG] = getcolor(dc, selfgcolor);

	/* menu geometry */
	bh = 6*dc->font.height + 2;
	//lines = MAX(lines, 0);
	mh = 1 * bh;
#ifdef XINERAMA
	if((info = XineramaQueryScreens(dc->dpy, &n))) {
		int i, di;
		unsigned int du;
		Window dw;

		XQueryPointer(dc->dpy, root, &dw, &dw, &x, &y, &di, &di, &du);
		for(i = 0; i < n; i++)
			if((monitor == info[i].screen_number)
			|| (monitor < 0 && INRECT(x, y, info[i].x_org, info[i].y_org, info[i].width, info[i].height)))
				break;
		x = info[i].x_org;
		y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
		mw = info[i].width;
		XFree(info);
	}
	else
#endif
	{
		x = 0;
		y = topbar ? 0 : DisplayHeight(dc->dpy, screen) - mh;
		mw = DisplayWidth(dc->dpy, screen);
	}
	/* menu window */
	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	win = XCreateWindow(dc->dpy, root, x, y, mw, mh, 0,
	                    DefaultDepth(dc->dpy, screen), CopyFromParent,
	                    DefaultVisual(dc->dpy, screen),
	                    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

	//grabkeyboard();
	resizedc(dc, mw, mh);
	//inputw = MIN(inputw, mw/3);
	//promptw = prompt ? textw(dc, prompt) : 0;
	//text[0] = '\0';
	//match();
}

// returns the first current notification or NULL ( and if n is supplied, number of total messages)
notification *notify_get_message(int *n) {
	notification *ptr;
	int temp=0;

	if( n==NULL ) n=&temp;

	*n=0;
	if( messages != NULL ) {
		// check/remove expired messages
		while( messages!=NULL && messages->expires_after!=0 && 
		(messages->started_at + messages->expires_after) < time(NULL) ) {
			notification *t = messages->next;
			notify_NotificationClosed(messages->nid, 1 + messages->closed*2);
			free(messages);
			messages = t;
		}

		ptr=messages;
		while( ptr!=NULL ) { ptr=ptr->next; (*n)++; }
	}

	return messages;
}

// check the dbus for notifications (1=something happened, 0=nothing)
char notify_check() {
	DBusMessage* msg;

	// non blocking read of the next available message
	dbus_connection_read_write(dbus_conn, 0);
	msg = dbus_connection_pop_message(dbus_conn);

	if (msg != NULL) { 
		if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")) 
			notify_Notify(msg);
		if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetCapabilities")) 
			notify_GetCapabilities(msg);
		if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetServerInformation")) 
			notify_GetServerInformation(msg);
		if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "CloseNotification")) 
			notify_CloseNotification(msg);

		dbus_message_unref(msg);
		dbus_connection_flush(dbus_conn);
		return 1;
	}
	return 0;
}
 
// to support libnotify events, we must implement:
//
// Methods:
//   org.freedesktop.Notifications.Notify (STRING app_name, UINT32 replaces_id, app_icon (ignored), STRING summary, STRING body, actions (ignored), hints (ignored), INT32 expire_timeout)
//     replaces_id = previous notification to replace
//     expire_timeout==0 for no expiration, -1 for default expiration
//     returns notification id (replaces_id if given)
//
//   org.freedesktop.Notifications.GetCapabilities
//     returns caps[1] = "body" (doesnt support any fancy features)
//
//   org.freedesktop.Notifications.GetServerInformation
//     returns "dwmstatus", "suckless", "0.1"
//     
//   org.freedesktop.Notifications.CloseNotification (nid)
//     forcefully hide and remove notification
//     emits NotificationClosed signal when done
//
// Signal:
//   org.freedesktop.Notifications.NotificationClosed -> (nid, reason )
//     whenever notification is closed(reason=3) or expires(reason=1)

char notify_NotificationClosed(unsigned int nid, unsigned int reason)
{
	DBusMessageIter args;
	DBusMessage* notify_close_msg;
	serial++;

	DEBUG("NotificationClosed(%d, %d)\n", nid, reason);

	notify_close_msg = dbus_message_new_signal("/org/freedesktop/Notifications", "org.freedesktop.Notifications", "NotificationClosed");
	if( notify_close_msg == NULL )
		return 0;

	dbus_message_iter_init_append(notify_close_msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &nid) ||
	!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &reason) ||
	!dbus_connection_send(dbus_conn, notify_close_msg, &serial))
	{
		dbus_message_unref(notify_close_msg);
		return 0;
	}
	dbus_message_unref(notify_close_msg);

	DEBUG("   Signal emitted\n");
	return 1;
}

//	since most libnotify clients dont respect my capabilities, this
//	simple helper will strip html tags and endlines from notifications
//	modifies string in place
//	strips &amp; - stuff instead of replacing with proper chars  */
//	* Jeremy Jay *
void _strip_body(char *text) {
	int i=0, j=0;
	char in_tag=0, in_amp=0;

	while( text[j] ) {
		if( text[j]=='\n' ) text[i++]=' ';
		else if( text[j]=='\t' ) text[i++]=' ';
		else if( text[j]=='<' ) in_tag=1;
		else if( text[j]=='>' && in_tag ) in_tag=0;
		else if( text[j]=='&' ) in_amp=1;
		else if( text[j]==';' && in_amp ) { in_amp=0; text[i++]=' '; }
		else if( !in_tag && !in_amp ) text[i++]=text[j];
		j++;
	}
	text[i]=0;
}

// Notify
char notify_Notify(DBusMessage *msg) {
	DBusMessage* reply;
	DBusMessageIter args;
	const char *appname;
	const char *summary;
	const char *body;
	dbus_uint32_t nid=0;
	dbus_int32_t expires=-1;
	notification *ptr = messages;
	notification *note = NULL;

	serial++;

	dbus_message_iter_init(msg, &args);
	dbus_message_iter_get_basic(&args, &appname);
	dbus_message_iter_next( &args );
	dbus_message_iter_get_basic(&args, &nid);
	dbus_message_iter_next( &args );
	dbus_message_iter_next( &args );  // skip icon
	dbus_message_iter_get_basic(&args, &summary);
	dbus_message_iter_next( &args );
	dbus_message_iter_get_basic(&args, &body);
	dbus_message_iter_next( &args );
	dbus_message_iter_next( &args );  // skip actions
	dbus_message_iter_next( &args );  // skip hints
	dbus_message_iter_get_basic(&args, &expires);

	DEBUG("Notify('%s', %u, -, '%s', '%s', -, -, %d)\n",appname, nid, summary, body, expires);

	if( nid!=0 ) { // update existing message
		note = messages;
		if( note!=NULL )
			while( note->nid != nid && note->next!=NULL ) note=note->next;

		if( note==NULL || note->nid!=nid ) { // not found, re-create
			note = calloc(sizeof(notification), 1);
			note->nid=nid;
			nid=0;
		}
	} else {
		note = calloc(sizeof(notification), 1);
		note->nid=curNid++;
		note->started_at = time(NULL);
	}
	note->expires_after = (time_t)(expires<0?EXPIRE_DEFAULT:expires*EXPIRE_MULT);
	note->closed=0;
	strncpy( note->appname, appname, 20);
	strncpy( note->summary, summary, 64);
	strncpy( note->body,    body, 256);
	_strip_body(note->body);
	DEBUG("   body stripped to: '%s'\n", note->body);

	if( nid==0 ) {
		if( ptr==NULL ) messages=note;
		else {
			while( ptr->next != NULL ) ptr=ptr->next;
			ptr->next=note;
		}
	}
	
        char buffer[2048];
        if(!strlen(note->body))
        {
                sprintf( buffer, "%s \"%s\" &", command, note->summary ? note->summary : "");
        }
        else
        {
                if(!strlen(note->summary))
                {
                        sprintf( buffer, "%s \"%s\" &", command, note->body ? note->body : "");
                }
                else
                {
                        sprintf( buffer, "%s \"<%s> %s\" &", command, note->summary ? note->summary : "", note->body ? note->body : "");
                }
        }
        //system( buffer );
	// heere
	//                 mw   mh
	printf("-%s-\n",buffer);
	//drawrect(dc, 0, 0, 100, 20, True, BG(dc, normcol));
	XMapRaised(dc->dpy, win);
	dc->x = 0;
	dc->y = 0;
	dc->h = bh;
	drawrect(dc, 0, 0, mw, mh, True, BG(dc, normcol));


	dc->x = 0;
	dc->y = 0;
	dc->h = bh;
	dc->w = mw;
	drawtext(dc, buffer, normcol);
	mapdc(dc, win, mw, mh);
	xrun();
	

	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &(note->nid)) ||
	!dbus_connection_send(dbus_conn, reply, &serial))
	{
		return 1;
	}
	dbus_message_unref(reply);

	DEBUG("   Notification %d created.\n", note->nid);
	return 1;
}

void
xrun(void) {
	XEvent ev;

	while(!XNextEvent(dc->dpy, &ev))
		switch(ev.type) {
		  //		case Expose:
		  //	if(ev.xexpose.count == 0)
		  //		drawmenu();
			break;
/*		case KeyPress:
			keypress(&ev.xkey);
			break;
		case SelectionNotify:
			if(ev.xselection.property == utf8)
				paste();
			break;*/
		case VisibilityNotify:
			if(ev.xvisibility.state != VisibilityUnobscured)
				XRaiseWindow(dc->dpy, win);
			break;
		}
}

// CloseNotification
char notify_CloseNotification(DBusMessage *msg) {
	DBusMessage* reply;
	DBusMessageIter args;
	dbus_uint32_t nid=0;
	notification *ptr = messages;

	dbus_message_iter_init(msg, &args);
	dbus_message_iter_get_basic(&args, &nid);

	DEBUG("CloseNotification(%d)\n", nid);

	if( ptr!=NULL && ptr->nid==nid ) {
		ptr->expires_after=(time(NULL) - ptr->started_at)*EXPIRE_MULT;
		ptr->closed=1;
	} else if( ptr!=NULL ) {
		while( ptr->next != NULL && ptr->next->nid != nid ) {
			ptr=ptr->next;
		}

		if( ptr->next != NULL && ptr->next->nid==nid ) {
			ptr = ptr->next;
			ptr->expires_after=(time(NULL) - ptr->started_at)*EXPIRE_MULT;
			ptr->closed=1;
		}
	}

	reply = dbus_message_new_method_return(msg);
	if( !dbus_connection_send(dbus_conn, reply, &serial)) return 1;
	dbus_message_unref(reply);

	DEBUG("   Close Notification Queued.\n");
	return 1;
}

// GetCapabilites
char notify_GetCapabilities(DBusMessage *msg) {
	DBusMessage* reply;
	DBusMessageIter args;
	DBusMessageIter subargs;
	int ncaps = 1;
	
	char *caps[1] = {"body"}, **ptr = caps;  // workaround (see specs)
	serial++;

	printf("GetCapabilities called!\n");

	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);
	if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, NULL, &subargs ) ||
			 !dbus_message_iter_append_fixed_array(&subargs, DBUS_TYPE_STRING, &ptr, ncaps) ||
			 !dbus_message_iter_close_container(&args, &subargs) ||
	!dbus_connection_send(dbus_conn, reply, &serial))
	{
		 return 1;
	}

	dbus_message_unref(reply);
	return 0;
}

// GetServerInformation
char notify_GetServerInformation(DBusMessage *msg) {
	DBusMessage* reply;
	DBusMessageIter args;
	
	char* info[4] = {"cow-notify", "cow-notify", "0.1", "1.0"};
	serial++;

	printf("GetServerInfo called!\n");

	reply = dbus_message_new_method_return(msg);

	dbus_message_iter_init_append(reply, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[0]) ||
	!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[1]) ||
	!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[2]) ||
	!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &info[3]) ||
	!dbus_connection_send(dbus_conn, reply, &serial))
	{
		return 1;
	}

	dbus_message_unref(reply);
	return 0;
}
