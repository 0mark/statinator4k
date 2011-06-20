/*
 * dwm status program
 *   handles libnotify events
 *   gets cpu usage directly from /proc/stats
 *   gets wifi signal level directly from /proc/net/wireless
 *   gets battery info from /proc/acpi/battery/BAT*
 *   also shows mpd song title changes (updates every 5 seconds)
 *      (connection tries once a minute)
 *
 * Written by Jeremy Jay
 * December 2008
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

// appending printf macro
#define aprintf(STR, ...) snprintf(STR+strlen(STR), 256-strlen(STR), __VA_ARGS__)
#define DEBUG(...) if(DEBUGGING) fprintf(stderr, __VA_ARGS__)

#define DATE_STRING "%d %b %Y - %I:%M"

static char DEBUGGING = 1;

typedef char (*status_f)(char *);

int num_big_status=0, num_normal_status=0;
status_f big_statuslist[16];
status_f normal_statuslist[16];

/////////////////////////////////////////////////////////////////////////////

#ifdef USE_NOTIFY
#include <dbus/dbus.h>
#include "notify.h"

// length of notification marquee
#define MARQUEE_CHARS 30
// number of characters to shift marquee each second
#define MARQUEE_OFFSET 3

// prints the first current notification into status
char get_messages(char *status) {
	char fmt[10];
	int n=0;
	notification *m = notify_get_message(&n);

	// we have a message to show
	if( m != NULL ) {
		int rem = (m->started_at+m->expires_after)-time(NULL);
		if( rem>0 )	aprintf(status, "%d ", rem);
		aprintf(status, "%s: %s", m->appname, m->summary );
		if( m->body[0]!=0 ) {
			// marquee the body text, up to MARQUEE_CHARS size
			if( strlen(m->body) < MARQUEE_CHARS ) {
				aprintf(status, " [%s]", m->body);
			} else {
				int offset=(time(NULL) - m->started_at)-1;
				if( offset<0 ) offset=0;
				offset*=MARQUEE_OFFSET;
				if( offset > strlen(m->body)-MARQUEE_CHARS )
					offset = strlen(m->body)-MARQUEE_CHARS;
				if( offset<0 ) offset=0;

				snprintf(fmt, 10, " [%%.%ds]", MARQUEE_CHARS);
				aprintf(status, fmt, m->body + offset);
			}
		}
		aprintf(status, " | ");
		return 1;
	}
	return 0;
}

#endif // USE_NOTIFY

/////////////////////////////////////////////////////////////////////////////

#ifdef USE_MPD
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

#endif // USE_MPD

/////////////////////////////////////////////////////////////////////////////

#ifdef USE_BATTERIES

#define __USE_BSD
#include <dirent.h>

int num_batteries=-1;
char **batteries = NULL;
int *battery_caps = NULL;
int total_caps=0;

// aggregate battery info from all batteries
//   if discharging or low, append to status text
//   if really low, colorize
char get_battery(char *status) {
	int i=0;
	int trate=0, tremaining=0, time_remaining=0, percent=0;
	char bstate[2]={0,0};
	char label[128], value[128];
	char filename[256];
	FILE *fp;

	if( num_batteries==0 ) return 0;

	for(i=0; i<num_batteries; i++) {
		int rate=0, remaining=0;
		sprintf(filename, "/proc/acpi/battery/%s/state", batteries[i]);
		fp = fopen(filename, "r");

		while( !feof(fp) ) {
			if( fscanf(fp, " %[^:]: %[^\n]\n", label, value) != 2 )
				break;

			if( strncmp(label, "charging state", 14)==0 ) {
				if( strncmp(value, "charging", 8)==0 )
					bstate[0]='+';
				else if( strncmp(value, "discharging", 11)==0 )
					bstate[0]='-';
			}
			else if( strncmp(label, "present rate", 12)==0 ) {
				rate = atoi(value);
				if( rate <= 0 ) rate=1;
			}
			else if( strncmp(label, "remaining capacity", 18)==0 ) {
				remaining = atoi(value);
			}
		}
		fclose(fp);

		trate+=rate;
		tremaining+=remaining;
	}

	time_remaining = (60*tremaining) / trate;
	percent = (100*tremaining) / total_caps;
	if( percent > 100 ) percent=100;

	if( bstate[0]=='-' || percent < 50 ) {
		if( percent <= 15 ) {
			// if you have my colored status patch, this will highlight the battery indicator
			// otherwise put your own popup message/whatever here
			aprintf(status, "%cb=%d%%%s [%d:%02d] \x01| ", (percent<10?4:3), percent, bstate, time_remaining/60, time_remaining%60);
		} else
			aprintf(status, "b=%d%%%s [%d:%02d] | ", percent, bstate, time_remaining/60, time_remaining%60);
	}

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
	if( nentries<=2 ) {
		num_batteries=0;
		return;
	}

	batteries = calloc( sizeof(char *), nentries-2);
	battery_caps = calloc( sizeof(int), nentries-2);
	total_caps=0;

	for( i=0; i<nentries; i++) {
		if( strncmp("BAT", batdirs[i]->d_name, 3)==0 ) {
			sprintf(filename, "/proc/acpi/battery/%s/info", batdirs[i]->d_name);
			fp = fopen(filename, "r");

			while( fp && !feof(fp) ) {
				if( fscanf(fp, " %[^:]: %[^\n]\n", label, value) != 2 )
					break;

				if( strncmp(label, "present", 7)==0 ) {
					if( strncmp(value, "no", 2)==0 ) break;
					else {
						num_batteries++;
						batteries[num_batteries] = calloc(sizeof(char), strlen(batdirs[i]->d_name));
						strcpy( batteries[num_batteries], batdirs[i]->d_name );
					}
				}
				if( strncmp(label, "design capacity", 15)==0 ) {
					battery_caps[num_batteries] = atoi( value );
					total_caps+=battery_caps[num_batteries];
					break;
				}
			}
			fclose(fp);
		}
	}
	num_batteries++;
}

#endif // USE_BATTERIES

/////////////////////////////////////////////////////////////////////////////

#ifdef USE_WIFI

// get wifi quality from /proc/net/wireless
// append to status if weak signal
char get_wifi(char *status) {
	char devname[20];
	unsigned int ch=0;
	unsigned int wstatus=0;
	unsigned int perc=0;

	FILE *fp = fopen("/proc/net/wireless", "r");
	while( (ch=fgetc(fp)) != '\n' && ch!=EOF );  // skip 2 header lines
	while( (ch=fgetc(fp)) != '\n' && ch!=EOF );
	if( fscanf(fp, "%s %u %u", devname, &wstatus, &perc) != 3 ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	//if( perc < 33 ) 
	devname[strlen(devname)-1] = 0;
	aprintf(status, "%s=%d%% | ", devname, perc);

	//printf("%s, %d, %d\n", devname, wstatus, perc);

	return 1;
}

#endif // USE_WIFI

/////////////////////////////////////////////////////////////////////////////

// get cpu stats from /proc/stat
// appends to status if high usage
char get_cpu(char *status) {
	static unsigned int o_running=0, o_total=0;
	unsigned int user=0, nice=0, system=0, idle=0;
	unsigned int running=0, total=0;
	unsigned int perc=0;

	FILE *fp = fopen("/proc/stat", "r");
	// this is the average for multiple cpus, if you want per-cpu, use cpu0, cpu1 etc
	if( fscanf(fp, "cpu %u %u %u %u", &user, &nice, &system, &idle) != 4 ) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	running = user+nice+system;
	total = running+idle;
	perc = ((running-o_running)*100) / (total-o_total);

	if( perc>100 ) perc=100;

	//if( perc > 33 ) 
		aprintf(status, "c=%d%% | ", perc);

	o_running=running;
	o_total=total;
	return 1;
}

/////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
	char stext[256];
	int i=0;
	time_t now;
	Display *dpy;
	Window root;

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "dwmstatus: cannot open display\n");
		return 1;
	}
	root = DefaultRootWindow(dpy);

	if( argc==2 && argv[1][0]=='-' && argv[1][1]=='v' ) {
		DEBUGGING=1;
		fprintf(stderr, "debugging enabled.\n");
	}

#ifdef USE_NOTIFY
	if( !notify_init(DEBUGGING) ) {
		fprintf(stderr, "dwmstatus: cannot bind notification\n");
		return 1;
	}
	big_statuslist[ num_big_status++ ] = get_messages;
#endif

#ifdef USE_MPD
	big_statuslist[ num_big_status++ ] = get_mpdsong;
#endif

	normal_statuslist[ num_normal_status++] = get_cpu;

#ifdef USE_WIFI
	normal_statuslist[ num_normal_status++] = get_wifi;
#endif

#ifdef USE_BATTERIES
	check_batteries();
	normal_statuslist[ num_normal_status++ ] = get_battery;
#endif

	while ( 1 )
#ifdef USE_NOTIFY
		if( !notify_check() )
#endif
		{
			stext[0]=0;

			for(i=0; i<num_big_status; i++)
				if( big_statuslist[i](stext) )
					break;

			// only add other info if no big messages
			// (status bar isnt that big)
			if( i == num_big_status )
				for(i=0; i<num_normal_status; i++)
					normal_statuslist[ i ](stext);
			
			// add the date/time to the end
			now = time(NULL);
			strftime(stext+strlen(stext), 256-strlen(stext), DATE_STRING, localtime( &now ) );

			XChangeProperty(dpy, root, XA_WM_NAME, XA_STRING,
					8, PropModeReplace, (unsigned char*)stext, strlen(stext));
			XFlush(dpy);
			//printf("%s\n", stext);

			// this needs to be low enough to catch notifications quickly
			sleep(1);
		}

	return 0;
}
