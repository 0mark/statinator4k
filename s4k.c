/**
 * simple status program
 *
 * Written by Jeremy Jay <jeremy@pbnjay.com> December 2008
 * Stefan Mark <0mark@unserver.de> June 2011
 */

#ifndef USE_ALSAVOL
#define _POSIX_C_SOURCE 1
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#ifdef USE_SOCKETS
#include <sys/socket.h>
#include <sys/types.h>
//#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
// backward dingsi stuff
#define __USE_GNU
#include <netdb.h>
#include <string.h>
#include <errno.h>
#endif

#define __USE_BSD
#include <dirent.h>

#ifdef USE_ALSAVOL
#include "alsa/asoundlib.h"
#endif

#ifdef USE_NOTIFY
#include <dbus/dbus.h>
#include "notify.h"
#endif

/* macros */
#define aprintf(STR, ...)   snprintf(STR+strlen(STR), max_status_length-strlen(STR), __VA_ARGS__)
#define LENGTH(X)           (sizeof X / sizeof X[0])
#define MAX(A, B)           ((A) > (B) ? (A) : (B))
#define MIN(A, B)           ((A) < (B) ? (A) : (B))

#define BUF_SIZE   256

/* enmus */
enum { BatCharged, BatCharging, BatDischarging };

enum { DATETIME, CPU, MEM, CLOCK, THERM, NET, WIFI, BATTERY,
#ifdef USE_SOCKETS
	CMUS,
	MPD,
#endif
#ifdef USE_ALSAVOL
	AVOL,
#endif
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

#ifdef USE_SOCKETS
typedef struct cmstat {
	int status;
	int duration;
	int position;
	int repeat;
	int shuffle;
	int volume;
	char artist[128];
	char album[128];
	char title[128];
} cmstat;
#endif

#ifdef USE_ALSAVOL
typedef struct astat {
	long vol;
	long vol_min;
	long vol_max;
} astat;
#endif

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
#ifdef USE_SOCKETS
static char get_cmus(char *status);
static void check_cmus();
static char get_mpd(char *status);
static void check_mpd();
#endif
#ifdef USE_ALSAVOL
static char get_alsavol(char *status);
#endif
#ifdef USE_NOTIFY
static char get_messages(char *status);
#endif


/* variables */
static dstat datetime_stat;
static cstat cpu_stat;
static mstat mem_stat;
static lstat clock_stat;
static tstat therm_stat;
static nwstat net_stat;
static wstat wifi_stat;
bstat *battery_stats;
int num_batteries;
#ifdef USE_SOCKETS
static cmstat cmus_stat;
int cmus_sock;
static cmstat mpd_stat;
int mpd_sock;
#ifndef USE_ALSAVOL
FILE *cmus_fp;
FILE *mpd_fp;
#endif
int cmus_connect = 0;
int mpd_connect = 0;
#endif
#ifdef USE_ALSAVOL
static astat alsavol_stat;
#endif
#ifdef USE_NOTIFY
static nstat notify_stat;
#endif
static const status_f statusfuncs[] = {
	get_datetime,
	get_cpu,
	get_mem,
	get_clock,
	get_therm,
	get_net,
	get_wifi,
	get_battery,
#ifdef USE_SOCKETS
	get_cmus,
	get_mpd,
#endif
#ifdef USE_ALSAVOL
	get_alsavol,
#endif
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
	if(fp==NULL)
		return 0;

	if(fscanf(fp, "cpu %u %u %u %u", &cpu_stat.user, &cpu_stat.nice, &cpu_stat.system, &cpu_stat.idle) != 4) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	cpu_format(status);

	return 1;
}

char get_mem(char *status) {
	static char label[16];
	unsigned int value;
	FILE *fp = fopen("/proc/meminfo", "r");
	if(fp==NULL)
		return 0;

	while(!feof(fp)) {
		if(fscanf(fp, "%[^:]: %u kB\n", label, &value)!=2) {
			fclose(fp);
			return 0;
		}

		if(strncmp(label, "MemTotal", 8)==0)
			mem_stat.total = value;
		else if(strncmp(label, "MemFree", 7)==0)
			mem_stat.free = value;
		else if(strncmp(label, "Buffers", 7)==0)
			mem_stat.buffers = value;
		else if(strncmp(label, "Cached", 67)==0) {
			mem_stat.cached = value;
			break;
		}
	}

	fclose(fp);

	mem_format(status);

	return 1;
}

char get_clock(char *status) {
	static char filename[BUF_SIZE];
	int i;
	FILE *fp;

	for(i=0; i<clock_stat.num_clocks; i++) {
		snprintf(filename, BUF_SIZE, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		fp = fopen(filename, "r");
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
	int i, len;

	clock_stat.num_clocks = 0;
	int nentries = scandir("/sys/devices/system/cpu/", &clockdirs, NULL, alphasort);
	if(nentries<=2)
		return;

	for(i=0; i<nentries; i++) {
		len = strlen(clockdirs[i]->d_name);
		if(len>=4 && strncmp("cpu", clockdirs[i]->d_name, 3)==0 && clockdirs[i]->d_name[3]>='0' && clockdirs[i]->d_name[3]<='9')
			clock_stat.num_clocks++;
	}
	clock_stat.clocks = calloc(sizeof(unsigned int), clock_stat.num_clocks);
}

char get_therm(char *status) {
	static char filename[BUF_SIZE];
	int i;

	for(i=0; i<therm_stat.num_therms; i++) {
		snprintf(filename, BUF_SIZE, "/sys/devices/virtual/thermal/thermal_zone%d/temp", i);

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
	unsigned int ch=0, ons = net_stat.count, i=0;
	FILE *fp = fopen("/proc/net/dev", "r");

	if(fp==NULL)
		return 0;

	net_stat.count = 0;

	while(!feof(fp)) {
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);
		net_stat.count++;
	}
	net_stat.count -= 3; // 2 header line and 1 line for lo device

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
		while(!feof(fp) && i<net_stat.count) {
			net_stat.ltx[i] = net_stat.tx[i];
			net_stat.lrx[i] = net_stat.rx[i];
			if(ons!=net_stat.count && net_stat.count) net_stat.devnames[i] = calloc(sizeof(char), 20);
			if(fscanf(fp, " %[^:]: %u %*u %*u %*u %*u %*u %*u %*u %u %*u %*u %*u %*u %*u %*u %*u\n", net_stat.devnames[i], &net_stat.rx[i], &net_stat.tx[i]) != 3) {
				fclose(fp);
				return 0;
			}
			i++;
		}
	} else {
		fclose(fp);
		return 0;
	}

	fclose(fp);

	net_format(status);

	return 1;
}


char get_wifi(char *status) {
	unsigned int ch = 0;
	FILE *fp = fopen("/proc/net/wireless", "r");

	if(fp==NULL)
		return 0;

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
	static char label[32], value[64];
	static char filename[BUF_SIZE];
	FILE *fp;

	if(num_batteries==0)
		return 0;

	for(i=0; i<num_batteries; i++) {
		sprintf(filename, "/proc/acpi/battery/%s/state", battery_stats[i].name);
		fp = fopen(filename, "r");
		if(fp==NULL)
			return 0;

		while(!feof(fp)) {
			if(fscanf(fp, " %[^:]: %[^\n]\n", label, value)!=2) {
				fclose(fp);
				return 0;
			}

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
	char label[32], value[64];
	char filename[BUF_SIZE];
	struct dirent **batdirs;
	int i;

	int nentries = scandir("/proc/acpi/battery/", &batdirs, NULL, alphasort);
	num_batteries = -1;
	if(nentries<=2) {
		return;
	}

	battery_stats = calloc(sizeof(bstat), nentries - 2); // at least 2 directory entries are '.' and '..'

	for(i=0; i<nentries; i++) {
		if(strlen(batdirs[i]->d_name)>=3 && strncmp("BAT", batdirs[i]->d_name, 3)==0) {
			sprintf(filename, "/proc/acpi/battery/%s/info", batdirs[i]->d_name);
			fp = fopen(filename, "r");
			if(fp==NULL) {
				continue;
			}

			while(fp && !feof(fp)) {
				if(fscanf(fp, " %[^:]: %[^\n]\n", label, value) != 2)
					break;

				if(strncmp(label, "present", 7)==0) {
					if(strncmp(value, "no", 2)==0) break; // not present battery is not interesting
					else {                                // (might be wrong, when battery is added)
						num_batteries++;                  // but this loop looks a bit suspect anyway...
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

#ifdef USE_SOCKETS
char get_cmus(char*status) {
    static const char cmd[] = "status\n";
	static char type[128], value[128], tag[128], value2[128];
	int tvol = 0;
#ifdef USE_ALSAVOL
#define SOCKBUFZIZE 1024
	int n, bufp = 0;
	static char buf[SOCKBUFZIZE];
#endif

	if(cmus_connect!=1) {
		check_cmus();
		return 0;
	}

    if(send(cmus_sock, cmd, strlen(cmd), MSG_NOSIGNAL)<0) {
		close(cmus_sock);
		return 0;
	}

#ifdef USE_ALSAVOL
	n = read(cmus_sock, buf, SOCKBUFZIZE);
	buf[n] = 0;

	while(bufp<n) {
	    //        this is awfull hack, i think. There have to be a sober solution...
		if(sscanf(buf+bufp, "%s %[^\n]\n", type, value) != 2) {
			break;
		}
		// TODO: find a sensible replacement for this ugly hack.
		// Before i used fdopen, but that need #define _POSIX_C_SOURCE 1, which conflicts with alsa...
		bufp+=strlen(type) + strlen(value) + 2;
#else
	while(cmus_fp && !feof(cmus_fp)) {
		if(fscanf(cmus_fp, "%s %[^\n]\n", type, value) != 2)
			break;
#endif

		if(strncmp(type, "status", 6)==0) {
			if(strncmp(value, "playing", 7)==0)
				cmus_stat.status = 1;
			else if(strncmp(value, "stopped", 7)==0)
				cmus_stat.status = 2;
			else
				cmus_stat.status = 0;
		} else if(strncmp(type, "duration", 8)==0) {
			cmus_stat.duration = atoi(value);
		} else if(strncmp(type, "position", 8)==0) {
			cmus_stat.position = atoi(value);
		} else if(strncmp(type, "tag", 3)==0) {
			if(sscanf(value, "%s %[^\n]\n", tag, value2) == 2) {
				if(strncmp(tag, "artist", 6)==0) {
					strncpy(cmus_stat.artist, value2, 128);
				} else if(strncmp(tag, "album", 5)==0) {
					strncpy(cmus_stat.album, value2, 128);
				} else if(strncmp(tag, "title", 5)==0) {
					strncpy(cmus_stat.title, value2, 128);
				}
			}
		} else if(strncmp(type, "set", 3)==0) {
			if(sscanf(value, "%s %[^\n]\n", tag, value2) == 2) {
				if(strncmp(tag, "repeat", 6)==0) {
					if(strncmp(value2, "true", 4)==0)
						cmus_stat.repeat = 1;
					else
						cmus_stat.repeat = 0;
				} else if(strncmp(tag, "shuffle", 7)==0) {
					if(strncmp(value2, "true", 4)==0)
						cmus_stat.shuffle = 1;
					else
						cmus_stat.shuffle = 0;
				} else if(strncmp(tag, "vol_", 4)==0) {
					tvol += atoi(value2);
				}
			}
		}
	}
	if(tvol) cmus_stat.volume = tvol / 2;

	cmus_format(status);

	return 1;
}

void check_cmus() {
    int len, flags;
    struct sockaddr_un saun;

    if((cmus_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        cmus_connect = 0;
        return;
    }

	flags = fcntl(cmus_sock, F_GETFL, 0);
	fcntl(cmus_sock, F_SETFL, flags | O_NONBLOCK);

    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, cmus_adress);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if(connect(cmus_sock, (struct sockaddr *)&saun, len) < 0) {
        close(cmus_sock);
		cmus_connect = 0;
        return;
    }

    cmus_connect = 1;

#ifndef USE_ALSAVOL
	cmus_fp = fdopen(cmus_sock, "r");
#endif
	cmus_stat.volume = 0;
}

char get_mpd(char*status) {
    static const char cmd[] = "status\ncurrentsong\n";
	static char type[128], value[128];
	char *dp;
#ifdef USE_ALSAVOL
#define SOCKBUFZIZE 1024
	int n, bufp = 0;
	static char buf[SOCKBUFZIZE];
#endif

	if(mpd_connect!=1) {
		check_mpd();
		return 0;
	}

    if(send(mpd_sock, cmd, strlen(cmd), MSG_NOSIGNAL)<0) {
		close(mpd_sock);
		return 0;
	}

#ifdef USE_ALSAVOL
	n = read(mpd_sock, buf, SOCKBUFZIZE);
	buf[n] = 0;

	while(bufp<n) {
	    //        this is awfull hack, i think. There have to be a sober solution...
		if(sscanf(buf+bufp, "%[^:]: %[^\n]\n", type, value) != 2) {
			bufp+=strlen(type) + strlen(value) + 3;
			continue;
		}
		// TODO: find a sensible replacement for this ugly hack.
		// Before i used fdopen, but that need #define _POSIX_C_SOURCE 1, which conflicts with alsa...
		bufp+=strlen(type) + strlen(value) + 3;
#else
	while(mpd_fp && !feof(mpd_fp)) {
		if(fscanf(mpd_fp, "%[^:]: %[^\n]\n", type, value) != 2)
			continue;
#endif

		if(strncmp(type, "state", 5)==0) {
			if(strncmp(value, "play", 4)==0)
				mpd_stat.status = 1;
			else if(strncmp(value, "stop", 4)==0)
				mpd_stat.status = 2;
			else
				mpd_stat.status = 0;
		} else if(strncmp(type, "time", 4)==0) {
			mpd_stat.position = atoi(value);
			dp = strstr(value, ":");
			dp++;
			mpd_stat.duration = atoi(dp);
		} else if(strncmp(type, "Artist", 6)==0) {
			strncpy(mpd_stat.artist, value, 128);
		} else if(strncmp(type, "Album", 5)==0) {
			strncpy(mpd_stat.album, value, 128);
		} else if(strncmp(type, "Title", 5)==0) {
			strncpy(mpd_stat.title, value, 128);
		} else if(strncmp(type, "repeat", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mpd_stat.repeat = 1;
			else
				mpd_stat.repeat = 0;
		} else if(strncmp(type, "random", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mpd_stat.shuffle = 1;
			else
				mpd_stat.shuffle = 0;
		} else if(strncmp(type, "volume", 6)==0) {
			mpd_stat.volume = atoi(value);
		}
	}
	// TODO: Bad!
	cmus_stat = mpd_stat;
	cmus_format(status);

	return 1;
}

void check_mpd() {
    int flags;
	struct hostent *host;
    struct sockaddr_in sain;

	host = gethostbyname(mpd_adress);

    if((mpd_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("sock fail\n");
		mpd_connect = 0;
        return;
    }

	sain.sin_family = AF_INET;
	sain.sin_port = htons(mpd_port);
	sain.sin_addr = *((struct in_addr *)host->h_addr);
	memset(&(sain.sin_zero), 0, 8);

    if(connect(mpd_sock, (struct sockaddr *)&sain, sizeof(struct sockaddr)) < 0) {
	perror("sock");
        printf("con fail\n");
        close(mpd_sock);
		mpd_connect = 0;
        return;
    }

	flags = fcntl(mpd_sock, F_GETFL, 0);
	fcntl(mpd_sock, F_SETFL, flags | O_NONBLOCK);

    mpd_connect = 1;

#ifndef USE_ALSAVOL
	mpd_fp = fdopen(mpd_sock, "r");
#endif
	mpd_stat.volume = 0;
}

#endif

#ifdef USE_ALSAVOL
// Derived from: http://blog.yjl.im/2009/05/get-volumec.html
// TODO: we shall read all channels, not only one
static const snd_mixer_selem_channel_id_t CHANNEL = SND_MIXER_SCHN_FRONT_LEFT;
char get_alsavol(char *status) {
	snd_mixer_t *h_mixer;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem ;

	if(snd_mixer_open(&h_mixer, 1) < 0)
		return 0;

	if(snd_mixer_attach(h_mixer, ATTACH) < 0)
		return 0;

	if(snd_mixer_selem_register(h_mixer, NULL, NULL) < 0)
		return 0;

	if(snd_mixer_load(h_mixer) < 0)
		return 0;
//#define __snd_alloca(ptr,type) do { *ptr = (type##_t *) alloca(type##_sizeof()); memset(*ptr, 0, type##_sizeof()); } while (0)
//#define snd_mixer_selem_id_alloca(ptr) __snd_alloca(ptr, snd_mixer_selem_id)
	//snd_mixer_selem_id_alloca(&sid);
	do {
		sid = (snd_mixer_selem_id_t *) calloc(sizeof(char), snd_mixer_selem_id_sizeof());
		memset(sid, 0, snd_mixer_selem_id_sizeof());
	} while (0);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, SELEM_NAME);

	if((elem = snd_mixer_find_selem(h_mixer, sid)) == NULL) {
		free(sid);
		return 0;
	}

	snd_mixer_selem_get_playback_volume(elem, CHANNEL, &alsavol_stat.vol);
	snd_mixer_selem_get_playback_volume_range(elem, &alsavol_stat.vol_min, &alsavol_stat.vol_max);

	snd_mixer_close(h_mixer);

	alsavol_format(status);

	free(sid);
	return 1;
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
		fprintf(stderr, "statinator4k: cannot open display\n");
		return 1;
	}
	root = DefaultRootWindow(dpy);
#endif

	check_batteries();
	check_clocks();
	check_therms();
#ifdef USE_SOCKETS
	check_cmus();
#endif
	net_stat.count = 0;

#ifdef USE_NOTIFY
	if(!notify_init(0)) {
		fprintf(stderr, "statinator4k: cannot bind notification\n");
		return 1;
	}
#endif

	while ( 1 )
#ifdef USE_NOTIFY
		if( !notify_check() )
#endif
		{
			stext[0] = 0;
			aprintf(stext, " ");
			mc = 0;

#ifndef NO_MSG_FUNCS
			for(i=0; i<LENGTH(message_funcs_order); i++)
				if(statusfuncs[message_funcs_order[i]](stext)) {
					mc++;
					if(auto_delimiter) aprintf(stext, "%s", delimiter);
				}
#endif

			if(mc<=max_big_messages)
				for(i=0; i<LENGTH(status_funcs_order); i++) {
					if(statusfuncs[status_funcs_order[i]](stext) && i<LENGTH(status_funcs_order)-1)
						if(auto_delimiter) aprintf(stext, "%s", delimiter);
				}

#ifdef USE_X11
			XChangeProperty(dpy, root, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (unsigned char*)stext, strlen(stext));
			XFlush(dpy);
#else
			printf("%s\n", stext);
#endif
			sleep(refresh_wait);
		}

#ifdef USE_SOCKETS
	if(cmus_sock) close(cmus_sock);
#endif

	return 0;
}
