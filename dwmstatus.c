/**
 * dwm status program
 *  - handles libnotify events
 *  - gets cpu usage directly from /proc/stats
 *  - gets wifi signal level directly from /proc/net/wireless
 *  - gets battery info from /proc/acpi/battery/BAT*
 *
 * TODO:
 *  - Current mpd stuff needs libmpd. We could do without.
 *    Perl example:
 *     - not my $socket = IO::Socket::INET->new(PeerAddr => ($config{'mpdhost'}||"localhost"), PeerPort => ($config{'mpdport'}||"6600"))
 *       $s =~ /volume: ([^\n]+)\nrepeat: ([^\n]+)\nrandom: ([^\n]+)\n[^\n]+\n[^\n]+\n[^\n]+\nplaylistlength: ([^\n]+)\n[^\n]+\nstate: ([^\n]+)\n(song: ([^\n]+)\n[^\n]+\ntime: ([^:]+):([^\n]+)\n)?/; 
 *     - Old mpd stuff is commented out now
 *  - Use inline funcs for formating, like get_cpu already does.
 *
 * Written by Jeremy Jay
 * December 2008
 *
 * Modified later by Stefan Mark, and likely others
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#ifdef USE_BATTERIES
#define __USE_BSD
#include <dirent.h>
#endif

#ifdef USE_NOTIFY
#include <dbus/dbus.h>
#include "notify.h"
#endif

// appending printf macro
#define aprintf(STR, ...) snprintf(STR+strlen(STR), 256-strlen(STR), __VA_ARGS__)

#define DATE_STRING "%d %b %Y - %I:%M"


#ifdef USE_CPU
typedef struct cpu_stat {
	unsigned int user;
	unsigned int nice;
	unsigned int system;
	unsigned int idle;
} cpu_stat;
#endif

#ifdef USE_WIFI
typedef struct wifi_stat {
	char devname[20];
	unsigned int wstatus;
	unsigned int perc;
} wifi_stat;
#endif

#ifdef USE_BATTERIES
int num_batteries = -1;
enum { BatCharged, BatCharging, BatDischarging };
typedef struct bstat {
	int state;
	int rate;
	int remaining;
	int capacity;
	char *name;
} bstat;
bstat *battery_stats;
#endif

#ifdef USE_NOTIFY
typedef struct nstat {
	notification *message;
} nstat;
nstat notify_stat;
#endif


#include "config.h"


typedef char (*status_f)(char *);
int num_big_status=0, num_normal_status=0;
status_f big_statuslist[16];
status_f normal_statuslist[16];


#ifdef USE_CPU
char get_cpu(char *status) {
	cpu_stat stat;
	FILE *fp = fopen("/proc/stat", "r");

	if(fscanf(fp, "cpu %u %u %u %u", &stat.user, &stat.nice, &stat.system, &stat.idle) != 4) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	cpu_format(status, &stat);

	return 1;
}
#endif


#ifdef USE_WIFI
char get_wifi(char *status) {
	wifi_stat stat;
	unsigned int ch=0;

	FILE *fp = fopen("/proc/net/wireless", "r");
	while((ch=fgetc(fp)) != '\n' && ch!=EOF);  // skip 2 header lines
	while((ch=fgetc(fp)) != '\n' && ch!=EOF);
	if(fscanf(fp, "%s %u %u", stat.devname, &stat.wstatus, &stat.perc) != 3) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	stat.devname[strlen(stat.devname)-1] = 0;
	wifi_format(status, &stat);

	return 1;
}
#endif


#ifdef USE_BATTERIES
char get_battery(char *status) {
	int i=0;
	char label[128], value[128];
	char filename[256];
	FILE *fp;

	if(num_batteries==0) return 0;

	for(i=0; i<num_batteries; i++) {
		//int rate=0, remaining=0;
		sprintf(filename, "/proc/acpi/battery/%s/state", battery_stats[i].name);
		fp = fopen(filename, "r");

		while(!feof(fp)) {
			if(fscanf(fp, " %[^:]: %[^\n]\n", label, value)!=2)
				break;

			if(strncmp(label, "charging state", 14)==0) {
				if(strncmp(value, "charging", 8)==0)
					battery_stats[i].state = BatCharging;
				else if(strncmp(value, "discharging", 11)==0)
					battery_stats[i].state = BatDischarging;
				else if(strncmp(value, "charged", 7)==0)
					battery_stats[i].state = BatCharged;
			}
			else if(strncmp(label, "present rate", 12)==0) {
				battery_stats[i].rate = atoi(value);
				if(battery_stats[i].rate < 0) battery_stats[i].rate=1;
			}
			else if(strncmp(label, "remaining capacity", 18)==0) {
				battery_stats[i].remaining = atoi(value);
			}

		}
		fclose(fp);
	}

	battery_format(status);

	return 1;
}

// find valid batteries in /proc/acpi/battery/BAT*
void check_batteries() {
	FILE *fp;
	char label[128], value[128];
	char filename[256];
	struct dirent **batdirs;
	int i;

	int nentries = scandir("/proc/acpi/battery/", &batdirs, NULL, alphasort);
	if(nentries<=2) {
		num_batteries=0;
		return;
	}

	battery_stats = calloc(sizeof(bstat), nentries - 2);

	for(i=0; i<nentries; i++) {
		if(strncmp("BAT", batdirs[i]->d_name, 3)==0) {
			sprintf(filename, "/proc/acpi/battery/%s/info", batdirs[i]->d_name);
			fp = fopen(filename, "r");

			while(fp && !feof(fp)) {
				if(fscanf(fp, " %[^:]: %[^\n]\n", label, value) != 2)
					break;

				if(strncmp(label, "present", 7)==0) {
					if(strncmp(value, "no", 2)==0) break;
					else {
						num_batteries++;
						battery_stats[num_batteries].name = calloc(sizeof(char), strlen(batdirs[i]->d_name));
						strcpy(battery_stats[num_batteries].name, batdirs[i]->d_name);
					}
				}
				if(strncmp(label, "last full capacity", 15)==0) {
					battery_stats[num_batteries].capacity = atoi(value);
					break;
				}
			}
			fclose(fp);
		}
	}
	num_batteries++;
}
#endif


#ifdef USE_NOTIFY
char get_messages(char *status) {
	int n=0;
	notify_stat.message = notify_get_message(&n);

	if(notify_stat.message!=NULL) {
		notify_format(status);
		return 1;
	}
	return 0;
}
#endif


int main(int argc, char **argv) {
	char stext[256];
	int i = 0;
	time_t now;
	Display *dpy;
	Window root;

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "dwmstatus: cannot open display\n");
		return 1;
	}
	root = DefaultRootWindow(dpy);

#ifdef USE_CPU
	normal_statuslist[num_normal_status++] = get_cpu;
#endif

#ifdef USE_WIFI
	normal_statuslist[num_normal_status++] = get_wifi;
#endif

#ifdef USE_BATTERIES
	check_batteries();
	normal_statuslist[num_normal_status++] = get_battery;
#endif

#ifdef USE_NOTIFY
	if(!notify_init(0)) {
		fprintf(stderr, "dwmstatus: cannot bind notification\n");
		return 1;
	}
	big_statuslist[num_big_status++] = get_messages;
#endif

	while ( 1 )
#ifdef USE_NOTIFY
		if( !notify_check() )
#endif
		{
			stext[0]=0;

			for(i=0; i<num_big_status; i++)
				if(big_statuslist[i](stext))
					break;

			// only add other info if no big messages
			// (status bar isnt that big)
			if(i==num_big_status)
				for(i=0; i<num_normal_status; i++) {
					normal_statuslist[ i ](stext);
					aprintf(stext, " ## ");
				}

			// add the date/time to the end
			now = time(NULL);
			strftime(stext+strlen(stext), 256-strlen(stext), DATE_STRING, localtime( &now ) );

			XChangeProperty(dpy, root, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (unsigned char*)stext, strlen(stext));
			XFlush(dpy);

			// this needs to be low enough to catch notifications quickly
			sleep(1);
		}

	return 0;
}









/*#ifdef USE_MPD
#include <libmpd/libmpd.h>

// checks the song playing on mpd
// if its new, displays a message
char get_mpdsong(char *status) {
	static MpdObj *mpd_connection = NULL;
	static char curSong[128];
	static int time_left=0, song_left=0;
	mpd_Song *temp = NULL;
	char tempstr[128];

	if( mpd_connection==NULL) {
		mpd_connection = mpd_new_default();
		mpd_connect(mpd_connection);
	}

	if( time_left<=0 ) {
		if( mpd_status_update(mpd_connection) ) {
			if( !mpd_connect(mpd_connection) )
				// wait a minute between retry attempts
				time_left=60;
		} else {
			// get current song
			temp = mpd_playlist_get_current_song(mpd_connection);
			if( temp ) {
				snprintf(tempstr,128,"%s - %s", temp->artist, temp->title);
				if( strncmp(curSong, tempstr, 128)!=0 ) {
					strncpy(curSong, tempstr, 128);
					// show new song for 5 seconds
					song_left=5;
				}
			}
			// wait 5 seconds before checking song again
			time_left=5;
		}
	}	else
		time_left--;
	
	if( song_left > 0 ) {
		song_left--;
		aprintf(status, "%s | ", curSong);
		return 1;
	}

	return 0;
}

#endif // USE_MPD*/
/*#ifdef USE_MPD
	big_statuslist[ num_big_status++ ] = get_mpdsong;
#endif*/
