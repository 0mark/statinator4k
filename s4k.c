/**
 * simple status program
 *
 * Written by Jeremy Jay <jeremy@pbnjay.com> December 2008
 * Stefan Mark <0mark@unserver.de> June 2011
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifdef USE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif
#ifdef USE_MPD
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
// backward dingsi stuff
#define __USE_GNU
#include <netdb.h>
#include <string.h>
#include <errno.h>
#endif
#define __USE_BSD
#include <dirent.h>
#ifdef USE_NOTIFY
#include <dbus/dbus.h>
#include "notify.h"
#endif


/* macros */
#define aprintf(STR, ...)   snprintf(STR+strlen(STR), max_status_length-strlen(STR), __VA_ARGS__)
#define LENGTH(X)           (sizeof X / sizeof X[0])

/* enmus */
enum { BatCharged, BatCharging, BatDischarging };

enum { DATETIME, CPU, MEM, CLOCK, THERM, NET, WIFI, BATTERY,
#ifdef USE_NOTIFY
	NOTIFY,
#endif
	NUMFUNCS, };

typedef struct dstat {
	time_t time;
} dstat;

typedef struct cstat {
	unsigned int user;
	unsigned int nice;
	unsigned int system;
	unsigned int idle;
} cstat;

typedef struct mstat {
	unsigned int total;
	unsigned int free;
	unsigned int buffers;
	unsigned int cached;
} mstat;

typedef struct lstat {
	int num_clocks;
	unsigned int *clocks;
} lstat;

typedef struct tstat {
	int num_therms;
	unsigned int *therms;
} tstat;

typedef struct nwstat {
	int count;
	unsigned int *rx;
	unsigned int *tx;
	unsigned int *lrx;
	unsigned int *ltx;
	char **devnames;
} nwstat;

typedef struct wstat {
	char devname[20];
	unsigned int wstatus;
	unsigned int perc;
} wstat;

typedef struct bstat {
	int state;
	int rate;
	int remaining;
	int capacity;
	char *name;
} bstat;

#ifdef USE_NOTIFY
typedef struct nstat {
	notification *message;
} nstat;
#endif

typedef char (*status_f)(char *);


/* function declarations */
static char get_datetime(char *status);
static char get_cpu(char *status);
static char get_mem(char *status);
static char get_clock(char *status);
static void check_clocks();
static char get_therm(char *status);
static void check_therms();
static char get_net(char *status);
static char get_wifi(char *status);
static char get_battery(char *status);
static void check_batteries();
#ifdef USE_NOTIFY
static char get_messages(char *status);
#endif


/* variables */
dstat datetime_stat;
cstat cpu_stat;
mstat mem_stat;
lstat clock_stat;
tstat therm_stat;
nwstat net_stat;
wstat wifi_stat;
bstat *battery_stats;
int num_batteries = -1;
#ifdef USE_NOTIFY
nstat notify_stat;
#endif
status_f statusfuncs[] = {
	get_datetime,
	get_cpu,
	get_mem,
	get_clock,
	get_therm,
	get_net,
	get_wifi,
	get_battery,
#ifdef USE_NOTIFY
	get_messages,
#endif
};


#include "config.h"


char get_datetime(char *status) {
	datetime_stat.time = time(NULL);
	datetime_format(status);
	return 1;
}

char get_cpu(char *status) {
	// TODO: multiple CPUs!
	FILE *fp = fopen("/proc/stat", "r");

	if(fscanf(fp, "cpu %u %u %u %u", &cpu_stat.user, &cpu_stat.nice, &cpu_stat.system, &cpu_stat.idle) != 4) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	cpu_format(status);

	return 1;
}

char get_mem(char *status) {
	char label[128], value[128];
	FILE *fp = fopen("/proc/meminfo", "r");

	while(!feof(fp)) {
		if(fscanf(fp, "%[^:]: %[^\n] kB\n", label, value)!=2)
			break;

		if(strncmp(label, "MemTotal", 8)==0) {
			mem_stat.total = atoi(value);
		}
		else if(strncmp(label, "MemFree", 7)==0) {
			mem_stat.free = atoi(value);
		}
		else if(strncmp(label, "Buffers", 7)==0) {
			mem_stat.buffers = atoi(value);
		}
		else if(strncmp(label, "Cached", 67)==0) {
			mem_stat.cached = atoi(value);
			break;
		}
	}

	fclose(fp);

	mem_format(status);

	return 1;
}

char get_clock(char *status) {
	char filename[256];
	int i;

	for(i=0; i<clock_stat.num_clocks; i++) {
		snprintf(filename, 256, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		FILE *fp = fopen(filename, "r");
		if(fp==NULL)
			return 0;

		if(fscanf(fp, "%u", &clock_stat.clocks[i]) != 1) {
			fclose(fp);
			return 0;
		}
		fclose(fp);
	}

	clock_format(status);

	return 1;
}

void check_clocks() {
	struct dirent **clockdirs;
	int i;

	clock_stat.num_clocks = 0;
	int nentries = scandir("/sys/devices/system/cpu/", &clockdirs, NULL, alphasort);
	if(nentries<=2)
		return;

	for(i=0; i<nentries; i++) {
		if(strncmp("cpu", clockdirs[i]->d_name, 3)==0 && clockdirs[i]->d_name[3]>='0' && clockdirs[i]->d_name[3]<='9')
			clock_stat.num_clocks++;
	}
	clock_stat.clocks = calloc(sizeof(unsigned int), clock_stat.num_clocks);
}

char get_therm(char *status) {
	char filename[256];
	int i;

	for(i=0; i<therm_stat.num_therms; i++) {
		snprintf(filename, 256, "/sys/devices/virtual/thermal/thermal_zone%d/temp", i);

		FILE *fp = fopen(filename, "r");
		if(fp==NULL)
			return 0;

		if(fscanf(fp, "%u", &therm_stat.therms[i]) != 1) {
			fclose(fp);
			return 0;
		}
		fclose(fp);
	}

	therm_format(status);

	return 1;
}

void check_therms() {
	struct dirent **thermdirs;
	int i;

	therm_stat.num_therms = 0;
	int nentries = scandir("/sys/devices/virtual/thermal", &thermdirs, NULL, alphasort);
	if(nentries<=2)
		return;

	for(i=0; i<nentries; i++) {
		if(strncmp("thermal_zone", thermdirs[i]->d_name, 12)==0 && thermdirs[i]->d_name[12]>='0' && thermdirs[i]->d_name[12]<='9')
			therm_stat.num_therms++;
	}
	therm_stat.therms = calloc(sizeof(unsigned int), therm_stat.num_therms);
}

char get_net(char *status) {
	unsigned int ch=0, ons, i=0;

	ons = net_stat.count;
	net_stat.count = 0;
	FILE *fp = fopen("/proc/net/dev", "r");
	while(!feof(fp)) {
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);
		net_stat.count++;
	}
	net_stat.count -= 3;

	if(net_stat.count>0) {
		if(ons!=net_stat.count) {
			net_stat.devnames = calloc(sizeof(char*), net_stat.count);
			net_stat.tx = calloc(sizeof(unsigned int), net_stat.count);
			net_stat.rx = calloc(sizeof(unsigned int), net_stat.count);
			net_stat.ltx = calloc(sizeof(unsigned int), net_stat.count);
			net_stat.lrx = calloc(sizeof(unsigned int), net_stat.count);
		}

		rewind(fp);
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);  // skip 2 header lines
		i = 0;
		while(!feof(fp)) {
			net_stat.ltx[i] = net_stat.tx[i];
			net_stat.lrx[i] = net_stat.rx[i];
			if(ons!=net_stat.count && net_stat.count) net_stat.devnames[i] = calloc(sizeof(char), 20);
			if(fscanf(fp, " %[^:]: %u %*u %*u %*u %*u %*u %*u %*u %u %*u %*u %*u %*u %*u %*u %*u\n", net_stat.devnames[i], &net_stat.rx[i], &net_stat.tx[i]) != 3) {
				i++;
				break;
			}
			i++;
		}
	}

	net_format(status);

	fclose(fp);

	return 1;
}


char get_wifi(char *status) {
	unsigned int ch=0;

	FILE *fp = fopen("/proc/net/wireless", "r");
	while((ch=fgetc(fp)) != '\n' && ch!=EOF);  // skip 2 header lines
	while((ch=fgetc(fp)) != '\n' && ch!=EOF);
	if(fscanf(fp, "%s %u %u", wifi_stat.devname, &wifi_stat.wstatus, &wifi_stat.perc) != 3) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	wifi_stat.devname[strlen(wifi_stat.devname)-1] = 0;
	wifi_format(status);

	return 1;
}

char get_battery(char *status) {
	int i = 0;
	char label[128], value[128];
	char filename[256];
	FILE *fp;

	if(num_batteries==0) return 0;

	for(i=0; i<num_batteries; i++) {
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

#ifdef USE_MPD
int sock;  
char get_mpd(char *status) {
	int bytes_recieved;  
	char recv_data[1024];

	if(sock>=0) {
		bytes_recieved = recv(sock, recv_data, 1024, 0);
		recv_data[bytes_recieved] = '\0';
		return 0;
	}
	// Now parse data!
	return 1;
}

void check_mpd() {
	struct hostent *host;
	struct sockaddr_in server_addr;  

	host = gethostbyname("127.0.0.1");

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Error opening socket\n");
		sock = -1;
		return;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(6600);
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	memset(&(server_addr.sin_zero), 0, 8); 

	if(connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		printf("Error connecting to \n");
		sock = -1;
		return;
	}
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
	char stext[max_status_length];
	int mc =0, i = 0;
#ifdef USE_X11
	Display *dpy;
	Window root;

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "dwmstatus: cannot open display\n");
		return 1;
	}
	root = DefaultRootWindow(dpy);
#endif

	check_batteries();
	check_clocks();
	check_therms();
	net_stat.count = 0;

#ifdef USE_NOTIFY
	if(!notify_init(0)) {
		fprintf(stderr, "dwmstatus: cannot bind notification\n");
		return 1;
	}
#endif

	while ( 1 )
#ifdef USE_NOTIFY
		if( !notify_check() )
#endif
		{
			stext[0] = 0;
			mc = 0;

			for(i=0; i<LENGTH(message_funcs_order); i++)
				if(statusfuncs[message_funcs_order[i]](stext)) {
					mc++;
					if(auto_delimiter) aprintf(stext, delimiter);
				}

			if(mc<=max_big_messages)
				for(i=0; i<LENGTH(status_funcs_order); i++) {
					if(statusfuncs[status_funcs_order[i]](stext) && i<LENGTH(status_funcs_order)-1)
						if(auto_delimiter) aprintf(stext, delimiter);
				}

#ifdef USE_X11
			XChangeProperty(dpy, root, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (unsigned char*)stext, strlen(stext));
			XFlush(dpy);
			//printf("--%s--\n", stext);
			//printf("%d\n", strlen(stext));
#else
			printf("%s\n", stext);
#endif
			sleep(refresh_wait);
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
