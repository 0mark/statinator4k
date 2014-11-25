/**
 * simple status program
 *
 * Written by Jeremy Jay <jeremy@pbnjay.com> December 2008
 * Stefan Mark <0mark@unserver.de> June 2011
 *
 * Sensors:
 *   date/time (C std lib)
 *   cpu load (/proc)
 *   memory usage (/proc)
 *   cpu clock (/sys)
 *   cpu temperature (acpi, /sys)
 *   network stat (/proc) TODO: find a way to get reliable up/down status
 *   wifi signal strength (/proc)
 *   battery stats (/proc)
 *   cmus, mpd stats (socket [unix, inet])
 *   volume setting (alsa lib) // TODO: find a better way
 *   notify (dbus, notify.c)
 *
 * Planned:
 *   uptime (/proc)
 *   imap mail (inet socket [ssl lib])
 *   local mail (filesystem)
 *   fan (/proc [at least for ibm])
 *   killswitch (/proc [at least for ibm])
 *   display brightness (/proc [at least for ibm])
 *   watchdoc
 *
 * Would have but dont know how:
 *   hdaps
 *
 * Every Sensor has:
 *  a function that gets data, named get_NAME, returning 1 on success and 0 on failure
 *  a format function, named NAME_format
 *  a struct variable for its data, named NAME_stat
 *
 * If a sensor needs some initialisation, it should be made in main. A formater has to
 * be made at least for dwm, and copyed in every other formater.
 */
#define _POSIX_C_SOURCE 1 // needed for fdopen

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#include <dirent.h>

#ifdef USE_SOCKETS
#include <sys/un.h>
#include <fcntl.h>
#include <netdb.h>
#endif

#ifdef USE_ALSAVOL
#include "alsa/asoundlib.h"
#endif

#ifdef USE_NOTIFY
#include <dbus/dbus.h>
#include "notify.h"
#endif


/* macros */
#define aprintf(STR, ...)            snprintf(STR+strlen(STR), max_status_length-strlen(STR), __VA_ARGS__)
#define LENGTH(X)                    (sizeof X / sizeof X[0])
#define MAX(A, B)                    ((A) > (B) ? (A) : (B))
#define MIN(A, B)                    ((A) < (B) ? (A) : (B))
#define XALLOC(target, type, size)   if((target = calloc(sizeof(type), size)) == NULL) die("fatal: could not malloc() %u bytes (target)\n", size*sizeof(type))


/* statics */
#define BUF_SIZE            256


/* enmus */
enum { BatCharged, BatCharging, BatDischarging, BatUnknown };

enum {
	DATETIME, CPU, MEM, CLOCK, THERM, NET, WIFI, BATTERY, BRIGHTNESS,
#ifdef USE_SOCKETS
	MP,
#endif
#ifdef USE_ALSAVOL
	AVOL,
#endif
#ifdef USE_NOTIFY
	NOTIFY,
#endif
	NUMFUNCS, };


/* structs */
typedef struct dstat { // datetime
	time_t time;
} dstat;

typedef struct cstat { // cpu
	int num_cpus;
	unsigned int *user;
	unsigned int *nice;
	unsigned int *system;
	unsigned int *idle;
	unsigned int *running;
	unsigned int *total;
	unsigned int *perc;
} cstat;

typedef struct mstat { // memory
	unsigned int total;
	unsigned int free;
	unsigned int buffers;
	unsigned int cached;
} mstat;

typedef struct lstat { // clock
	int num_clocks;
	unsigned int clock_min;
	unsigned int clock_max;
	unsigned int *clocks;
} lstat;

typedef struct tstat { // temperature
	int num_therms;
	unsigned int *therms;
} tstat;

typedef struct nwstat { // network
	int count;
	unsigned int *rx;
	unsigned int *tx;
	unsigned int *lrx;
	unsigned int *ltx;
	char **devnames;
} nwstat;

typedef struct wstat { // wifi
	char devname[20];
	unsigned int wstatus;
	unsigned int perc;
} wstat;

typedef struct bstat { // battery
	int num_bats;
	int *state;
	unsigned int *rate;
	unsigned int *remaining;
	unsigned int *capacity;
	char **name;
} bstat;

typedef struct brstat { // brightness
	int num_brght;
	unsigned int *brghts;
	unsigned int *max_brghts;
	char **devnames;
} brstat;

#ifdef USE_SOCKETS
typedef struct con {
	struct hostent* host;
    struct sockaddr_in sain;
    struct sockaddr_un saun;
    int sock;
    int connected;
    int domain;
    FILE* fp;
} con;
typedef struct mpstat { // music player
	int status;
	int duration;
	int position;
	int repeat;
	int shuffle;
	int volume;
	char artist[128];
	char album[128];
	char title[128];
	con con;
	void (*parse_func)();
} mpstat;
#endif

#ifdef USE_ALSAVOL
typedef struct astat { // volume
	long vol;
	long vol_min;
	long vol_max;
} astat;
#endif

#ifdef USE_NOTIFY
typedef struct nstat { // notifications
	notification *message;
} nstat;
#endif

typedef char (*status_f)(char *);

/* function declarations */
#ifdef USE_ALSAVOL
static char get_alsavol(char *status);
#endif
static void check_batteries();
static char get_battery(char *status);
static void check_brightness();
static char get_brightness(char *status);
static void check_clocks();
static char get_clock(char *status);
static void check_cpus();
static char get_cpu(char *status);
static char get_datetime(char *status);
static char get_mem(char *status);
#ifdef USE_NOTIFY
static char get_messages(char *status);
#endif
#ifdef USE_SOCKETS
static void check_mp();
static char get_mp();
static void check_con(con *con);
static char mp_parse_mpd();
static char mp_parse_madasul();
#endif
static char get_net(char *status);
static void check_therms();
static char get_therm(char *status);
static char get_wifi(char *status);
static void die(const char *errstr, ...);
static int read_clock(int num, char type[3], unsigned int *target);


/* variables */
#ifdef USE_ALSAVOL
static astat alsavol_stat;
#endif
//int num_batteries;
static bstat battery_stats;
static brstat brightness_stat;
static lstat clock_stat;
static cstat cpu_stat;
static dstat datetime_stat;
static mstat mem_stat;
#ifdef USE_NOTIFY
static nstat notify_stat;
#endif
#ifdef USE_SOCKETS
static mpstat mp_stat;
#endif
static nwstat net_stat;
static tstat therm_stat;
static wstat wifi_stat;

static const status_f statusfuncs[] = {
	get_datetime,
	get_cpu,
	get_mem,
	get_clock,
	get_therm,
	get_net,
	get_wifi,
	get_battery,
    get_brightness,
#ifdef USE_SOCKETS
	get_mp,
#endif
#ifdef USE_ALSAVOL
	get_alsavol,
#endif
#ifdef USE_NOTIFY
	get_messages,
#endif
};


#include "config.h"


void check_batteries() {
	// TODO: the battery count might change on run time?
	struct dirent **batdirs;
	int i, nentries = scandir("/sys/class/power_supply/", &batdirs, NULL, alphasort);

	if(nentries<=2) {
		for(i=0; i<nentries; i++)
			free(batdirs[i]);
		free(batdirs);
		return;
	}

	FILE *fp;
	char label[32], value[64];
	char filename[BUF_SIZE];
	battery_stats.num_bats = -1;

	
	XALLOC(battery_stats.state, int, nentries - 2);
	XALLOC(battery_stats.rate, unsigned int, nentries - 2);
	XALLOC(battery_stats.remaining, unsigned int, nentries - 2);
	XALLOC(battery_stats.capacity, unsigned int, nentries - 2);
	XALLOC(battery_stats.name, char*, nentries - 2);

	for(i=0; i<nentries; i++) {
		if(strlen(batdirs[i]->d_name)>=3 && strncmp("BAT", batdirs[i]->d_name, 3)==0) {
			sprintf(filename, "/sys/class/power_supply/%s/uevent", batdirs[i]->d_name);
			fp = fopen(filename, "r");

			if(fp==NULL)
				continue;

			while(fp && !feof(fp)) {
				if(fscanf(fp, "%[^=]=%[^\n]\n", label, value) != 2)
					break;

				if(strncmp(label, "POWER_SUPPLY_PRESENT", 20)==0) {
					if(strncmp(value, "0", 1)==0) break;  // not present battery is not interesting
					else {                                // (might be wrong, when battery is added)
						battery_stats.num_bats++;              	  // but this loop looks a bit fishy anyway...
						XALLOC(battery_stats.name[battery_stats.num_bats], char, strlen(batdirs[i]->d_name) + 1);
						strcpy(battery_stats.name[battery_stats.num_bats], batdirs[i]->d_name);
					}
				}
				if(strncmp(label, "POWER_SUPPLY_ENERGY_FULL", 24)==0 && strlen(label)==24) {
					battery_stats.capacity[battery_stats.num_bats] = atoi(value);
					break;
				}
			}
			fclose(fp);
		}
		free(batdirs[i]);
	}
	free(batdirs);
	battery_stats.num_bats++;
}

void check_brightness() {
	FILE *fp;
	char b[10], filename[BUF_SIZE];
	struct dirent **brightdirs;
	int i, ii, val, len, len2, nentries = scandir("/sys/class/backlight/", &brightdirs, NULL, alphasort);

	if(nentries<=2) {
		for(i=0; i<nentries; i++)
			free(brightdirs[i]);
		free(brightdirs);
		return;
	}

	XALLOC(brightness_stat.brghts, int, nentries - 2);
	XALLOC(brightness_stat.max_brghts, int, nentries - 2);
	XALLOC(brightness_stat.devnames, char*, nentries - 2);
    // brightness_stat.brghts = calloc(sizeof(int), nentries - 2); // at least 2 directory entries are '.' and '..'
    // brightness_stat.max_brghts = calloc(sizeof(int), nentries - 2); // at least 2 directory entries are '.' and '..'
    // brightness_stat.devnames = calloc(sizeof(char*), nentries - 2); // at least 2 directory entries are '.' and '..'
    brightness_stat.num_brght = 0;

	for(i=0; i<nentries; i++) {
		if(strlen(brightdirs[i]->d_name)>=3) {
            *filename = 0;
            for(ii=0; ii<LENGTH(brightnes_names); ii++) {
                len = strlen(brightnes_names[ii]);
                len2 = strlen(brightdirs[i]->d_name);
                if(len2>=len && strncmp(brightnes_names[ii], brightdirs[i]->d_name, len)==0) {
                    sprintf(filename, "/sys/class/backlight/%s/max_brightness", brightdirs[i]->d_name);
                    break;
                }
            }

            if(*filename==0) continue;
            fp = fopen(filename, "r");
			if(fp==NULL) continue;
            fgets(b, 10, fp);
            if((val=atoi(b))<1) continue;
            brightness_stat.max_brghts[brightness_stat.num_brght] = val;
            brightness_stat.devnames[brightness_stat.num_brght] = calloc(sizeof(char), len);
            strncpy(brightness_stat.devnames[brightness_stat.num_brght++], brightdirs[i]->d_name, len2);

			fclose(fp);
		}
		free(brightdirs[i]);
	}

	free(brightdirs);
}

void check_cpus() {
	FILE *fp = fopen("/proc/stat", "r");
	unsigned int x;

	if(fp==NULL)
		return;

	while(!feof(fp)) {
		if(fscanf(fp, "cpu%*[0-9] %u %u %u %u", &x, &x, &x, &x) == 4) {
			cpu_stat.num_cpus++;
		}
		while(!feof(fp) && fgetc(fp)!='\n');
	}

	XALLOC(cpu_stat.user, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.nice, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.system, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.idle, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.running, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.total, unsigned int, cpu_stat.num_cpus);
	XALLOC(cpu_stat.perc, unsigned int, cpu_stat.num_cpus);

	fclose(fp);
}

void check_clocks() {
	struct dirent **clockdirs;
	int i, nentries = scandir("/sys/devices/system/cpu/", &clockdirs, NULL, alphasort);

	if(nentries<=2) {
		for(i=0; i<nentries; i++)
			free(clockdirs[i]);
		free(clockdirs);
		return;
	}

	int len;
	clock_stat.num_clocks = 0;

	for(i=0; i<nentries; i++) {
		len = strlen(clockdirs[i]->d_name);
		if(len>=4 && strncmp("cpu", clockdirs[i]->d_name, 3)==0 && clockdirs[i]->d_name[3]>='0' && clockdirs[i]->d_name[3]<='9') {			
			if(clock_stat.num_clocks==0) {
				if(read_clock(clock_stat.num_clocks, "min", &clock_stat.clock_min)==0 || read_clock(clock_stat.num_clocks, "max", &clock_stat.clock_max)==0)
					return;
			}
			clock_stat.num_clocks++;
		}
		free(clockdirs[i]);
	}
	free(clockdirs);
	clock_stat.clocks = calloc(sizeof(unsigned int), clock_stat.num_clocks);
}

void check_mp() {
	if(!mp_port) { // unix sockets dont have a port
		mp_stat.con.saun.sun_family = AF_UNIX;
		strcpy(mp_stat.con.saun.sun_path, mp_adress);
		mp_stat.con.domain = AF_UNIX;
	} else { // internet sockets do have a port
		mp_stat.con.host = gethostbyname(mp_adress);
		mp_stat.con.sain.sin_family = AF_INET;
		mp_stat.con.sain.sin_port = htons(mp_port);
		mp_stat.con.sain.sin_addr = *((struct in_addr *)mp_stat.con.host->h_addr);
		memset(&(mp_stat.con.sain.sin_zero), 0, 8);
		mp_stat.con.domain = AF_INET;
	}
}

void check_therms() {
	struct dirent **thermdirs;
	int i, nentries = scandir("/sys/devices/virtual/thermal", &thermdirs, NULL, alphasort);

	if(nentries<=2) {
		for(i=0; i<nentries; i++)
			free(thermdirs[i]);
		free(thermdirs);
		return;
	}

	therm_stat.num_therms = 0;

	for(i=0; i<nentries; i++) {
		if(strncmp("thermal_zone", thermdirs[i]->d_name, 12)==0 && thermdirs[i]->d_name[12]>='0' && thermdirs[i]->d_name[12]<='9')
			therm_stat.num_therms++;
		free(thermdirs[i]);
	}
	free(thermdirs);
	XALLOC(therm_stat.therms, unsigned int, therm_stat.num_therms);
}



#ifdef USE_ALSAVOL
char get_alsavol(char *status) {
	// Derived from: http://blog.yjl.im/2009/05/get-volumec.html
	// TODO: we shall read all channels, not only one
	static const snd_mixer_selem_channel_id_t CHANNEL = SND_MIXER_SCHN_FRONT_LEFT;
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
	//was: 'snd_mixer_selem_id_alloca(&sid);', a macro:
	//  #define __snd_alloca(ptr,type) do { *ptr = (type##_t *) alloca(type##_sizeof()); memset(*ptr, 0, type##_sizeof()); } while (0)
	//  #define snd_mixer_selem_id_alloca(ptr) __snd_alloca(ptr, snd_mixer_selem_id)
	//but for some reasons alloca is not available. TODO: check if this strange loop is necesary
	do {
		sid = (snd_mixer_selem_id_t *) calloc(sizeof(char), snd_mixer_selem_id_sizeof());
		memset(sid, 0, snd_mixer_selem_id_sizeof());
	} while (0);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, SELEM_NAME);

	if((elem = snd_mixer_find_selem(h_mixer, sid)) == NULL) {
		free(sid); // TODO: why is only 'sid' freed?
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

char get_battery(char *status) {
	int i = 0;
	static char label[32], value[64];
	static char filename[BUF_SIZE];
	FILE *fp;

	if(battery_stats.num_bats==0)
		return 0;

	for(i=0; i<battery_stats.num_bats; i++) {
		sprintf(filename, "/sys/class/power_supply/%s/uevent", battery_stats.name[i]);
		fp = fopen(filename, "r");
		if(fp==NULL)
			return 0;

		while(!feof(fp)) {
			if(fscanf(fp, "%[^=]=%[^\n]\n", label, value)!=2) {
				continue;
			}

			if(strncmp(label, "POWER_SUPPLY_STATUS", 19)==0) {
				if(strncmp(value, "Charging", 8)==0)
					battery_stats.state[i] = BatCharging;
				else if(strncmp(value, "Discharging", 11)==0)
					battery_stats.state[i] = BatDischarging;
				else if(strncmp(value, "Charged", 7)==0)
					battery_stats.state[i] = BatCharged;
				else
					battery_stats.state[i] = BatUnknown;
			}
			else if(strncmp(label, "POWER_SUPPLY_POWER_NOW", 23)==0) {
				battery_stats.rate[i] = atoi(value);
				if(battery_stats.rate[i] < 0) battery_stats.rate[i]=1;
			}
			else if(strncmp(label, "POWER_SUPPLY_ENERGY_NOW", 22)==0) {
				battery_stats.remaining[i] = atoi(value);
			}

		}

		fclose(fp);
	}

	battery_format(status);

	return 1;
}

char get_brightness(char *status) {
	int i, val;
	static char b[10];
	static char filename[BUF_SIZE];
	FILE *fp;

	if(brightness_stat.num_brght==0)
		return 0;

	for(i=0; i<brightness_stat.num_brght; i++) {
		sprintf(filename, "/sys/class/backlight/%s/actual_brightness", brightness_stat.devnames[i]);

		fp = fopen(filename, "r");
		if(fp==NULL)
			return 0;

        fgets(b, 10, fp);

        if((val=atoi(b))<0) continue;
        brightness_stat.brghts[i] = val;
		fclose(fp);
	}

	brightness_format(status);

	return 1;
}

char get_clock(char *status) {
	int i;

	for(i=0; i<clock_stat.num_clocks; i++) {
		if(read_clock(i, "cur", &clock_stat.clocks[i])==0)
			return 0;
	}

	clock_format(status);

	return 1;
}

char get_cpu(char *status) {
	FILE *fp = fopen("/proc/stat", "r");

	if(fp==NULL)
		return 0;

	unsigned int running, total;
	int i;

	// skip first line
	while(!feof(fp) && fgetc(fp)!='\n');

	for(i=0; i<cpu_stat.num_cpus; i++) {
		if(fscanf(fp, "cpu%*[0-9] %u %u %u %u", &cpu_stat.user[i], &cpu_stat.nice[i], &cpu_stat.system[i], &cpu_stat.idle[i]) == 4) {
			running = cpu_stat.user[i] + cpu_stat.nice[i] + cpu_stat.system[i];
			total = running + cpu_stat.idle[i];
			cpu_stat.perc[i] = (total - cpu_stat.total[i]) ? ((running - cpu_stat.running[i]) * 100) / (total - cpu_stat.total[i]) : 0;
			cpu_stat.running[i] = running;
			cpu_stat.total[i] = total;
		}
		while(!feof(fp) && fgetc(fp)!='\n');
	}
	fclose(fp);

	cpu_format(status);

	return 1;
}

char get_datetime(char *status) {
	datetime_stat.time = time(NULL);
	datetime_format(status);
	return 1;
}

char get_mem(char *status) {
	FILE *fp = fopen("/proc/meminfo", "r");

	if(fp==NULL)
		return 0;

	static char label[18];
	unsigned int value;

	while(!feof(fp)) {
		if(fscanf(fp, "%16[^:]: %u kB\n", label, &value)!=2) {
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

char get_mp(char *status) {
	if(mp_stat.con.connected!=1) {
		check_con(&mp_stat.con);
		if(mp_stat.con.connected!=1)
			return 0;
	}
	
	if(mp_stat.con.connected==1) {
		mp_parse();
		mp_format(status);
	}
	
	return 1;
}

char get_net(char *status) {
	FILE *fp = fopen("/proc/net/dev", "r");

	if(fp==NULL)
		return 0;

	unsigned int ch=0, ons = net_stat.count, i;

	net_stat.count = 0;
	while(!feof(fp)) {
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);
		net_stat.count++;
	}
	net_stat.count -= 3; // 2 header line and 1 line for lo device

	if(net_stat.count>0) {
		if(ons!=net_stat.count) {
			if(ons > 0) {
				for(i=0; i<net_stat.count; i++) {
					free(net_stat.devnames[i]);
				}
				free(net_stat.tx);
				free(net_stat.rx);
				free(net_stat.ltx);
				free(net_stat.lrx);
			}
			XALLOC(net_stat.devnames, char*, net_stat.count);
			XALLOC(net_stat.tx, unsigned int, net_stat.count);
			XALLOC(net_stat.rx, unsigned int, net_stat.count);
			XALLOC(net_stat.ltx, unsigned int, net_stat.count);
			XALLOC(net_stat.lrx, unsigned int, net_stat.count);
		}

		rewind(fp);

		while((ch=fgetc(fp)) != '\n' && ch!=EOF);
		while((ch=fgetc(fp)) != '\n' && ch!=EOF);  // skip 2 header lines

		i = 0;
		while(!feof(fp) && i<net_stat.count) {
			net_stat.ltx[i] = net_stat.tx[i];
			net_stat.lrx[i] = net_stat.rx[i];
			if(ons!=net_stat.count && net_stat.count) XALLOC(net_stat.devnames[i], char, 20);
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

char get_wifi(char *status) {
	FILE *fp = fopen("/proc/net/wireless", "r");

	if(fp==NULL)
		return 0;

	unsigned int ch = 0;

	while((ch=fgetc(fp)) != '\n' && ch!=EOF);  // skip 2 header lines
	while((ch=fgetc(fp)) != '\n' && ch!=EOF);
                               // might provoke a buffer overflow? TODO
	if(fscanf(fp, "%s %u %u", wifi_stat.devname, &wifi_stat.wstatus, &wifi_stat.perc) != 3) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	wifi_stat.devname[strlen(wifi_stat.devname)-1] = 0;
	wifi_format(status);

	return 1;
}




void check_con(con *con) {
    int flags, stat;

    if((con->sock = socket(con->domain, SOCK_STREAM, 0)) < 0) {
        printf("sock fail\n");
		con->connected = 0;
        return;
    }

    if(con->domain == AF_INET)
    	stat = connect(con->sock, (struct sockaddr *)&con->sain, sizeof(struct sockaddr));
    else
    	stat = connect(con->sock, (struct sockaddr *)&con->saun, sizeof(con->saun.sun_family) + strlen(con->saun.sun_path));
    if(stat < 0) {
        close(con->sock);
		con->connected = 0;
        return;
    }

	flags = fcntl(con->sock, F_GETFL, 0);
	fcntl(con->sock, F_SETFL, flags | O_NONBLOCK);

    con->connected = 1;

	con->fp = fdopen(con->sock, "r");
}

char mp_parse_mpd() {
    static const char cmd[] = "status\ncurrentsong\n";
	static char type[128], value[128];
	char *dp;

    if(send(mp_stat.con.sock, cmd, strlen(cmd), MSG_NOSIGNAL)<0) {
		mp_stat.con.connected = 0;
		close(mp_stat.con.sock);
		fclose(mp_stat.con.fp);
		return 0;
	}

	while(mp_stat.con.fp && !feof(mp_stat.con.fp)) {
		if(fscanf(mp_stat.con.fp, "%[^:]: %[^\n]\n", type, value) != 2)
			break;
		
		if(strncmp(type, "state", 5)==0) {
			if(strncmp(value, "play", 4)==0)
				mp_stat.status = 1;
			else if(strncmp(value, "stop", 4)==0)
				mp_stat.status = 2;
			else
				mp_stat.status = 0;
		} else if(strncmp(type, "time", 4)==0) {
			mp_stat.position = atoi(value);
			dp = strstr(value, ":");
			dp++;
			mp_stat.duration = atoi(dp);
		} else if(strncmp(type, "Artist", 6)==0) {
			strncpy(mp_stat.artist, value, 128);
		} else if(strncmp(type, "Album", 5)==0) {
			strncpy(mp_stat.album, value, 128);
		} else if(strncmp(type, "Title", 5)==0) {
			strncpy(mp_stat.title, value, 128);
		} else if(strncmp(type, "repeat", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mp_stat.repeat = 1;
			else
				mp_stat.repeat = 0;
		} else if(strncmp(type, "random", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mp_stat.shuffle = 1;
			else
				mp_stat.shuffle = 0;
		} else if(strncmp(type, "volume", 6)==0) {
			mp_stat.volume = atoi(value);
		}
	}

	return 1;
}

char mp_parse_madasul() {
    static const char cmd[] = "status #c\t##\t#s\t#r\t#n\t$a\t$l\t$t\n";
	unsigned int track, tracknum, atrack;

	mp_stat.duration = mp_stat.position = mp_stat.volume = -1;

    if(send(mp_stat.con.sock, cmd, strlen(cmd), MSG_NOSIGNAL)<0) {
		mp_stat.con.connected = 0;
		close(mp_stat.con.sock);
		fclose(mp_stat.con.fp);
		return 0;
	}

	while(mp_stat.con.fp && !feof(mp_stat.con.fp)) {
		if(fscanf(mp_stat.con.fp, "%u\t%u\t%i\t%i\t%u\t%128[^\t]\t%128[^\t]\t%128[^\n]\n", &track, &tracknum, &mp_stat.status, &mp_stat.shuffle, &atrack, mp_stat.artist, mp_stat.album, mp_stat.title)!=8)
			continue;

		mp_stat.status = mp_stat.status==3 ? 1 : (mp_stat.status==1 ? 2 : 0);		
	}

	mp_stat.con.connected = 0;
	close(mp_stat.con.sock);
	fclose(mp_stat.con.fp);

	return 1;
}

/*
void check_con(con *con) {
    int flags, stat;

    if((con->sock = socket(con->domain, SOCK_STREAM, 0)) < 0) {
        printf("sock fail\n");
		con->connected = 0;
        return;
    }

    if(con->domain == AF_INET)
    	stat = connect(con->sock, (struct sockaddr *)&con->sain, sizeof(struct sockaddr));
    else
    	stat = connect(con->sock, (struct sockaddr *)&con->saun, sizeof(con->saun.sun_family) + strlen(con->saun.sun_path));
    if(stat < 0) {
        close(con->sock);
		con->connected = 0;
        return;
    }

	flags = fcntl(con->sock, F_GETFL, 0);
	fcntl(con->sock, F_SETFL, flags | O_NONBLOCK);

    con->connected = 1;

	#ifndef USE_ALSAVOL
	con->fp = fdopen(con->sock, "r");
	#endif
}


char mpd_parse() {
    static const char cmd[] = "status\ncurrentsong\n";
	static char type[128], value[128];
	char *dp;

	#ifdef USE_ALSAVOL
	#define SOCKBUFZIZE 1024
	int n, bufp = 0;
	static char buf[SOCKBUFZIZE];
	#endif

    if(send(mp_stat.con.sock, cmd, strlen(cmd), MSG_NOSIGNAL)<0) {
		mp_stat.con.connected = 0;
		close(mp_stat.con.sock);
		return 0;
	}

	#ifdef USE_ALSAVOL
	n = read(mp_stat.con.sock, buf, SOCKBUFZIZE);
	buf[n] = 0;

	while(bufp<n) {
		if(sscanf(buf+bufp, "%[^:]: %[^\n]\n", type, value) != 2) {
			bufp+=strlen(type) + strlen(value) + 3;
			continue;
		}
		bufp+=strlen(type) + strlen(value) + 3;
	#else
	while(mpd_fp && !feof(mpd_fp)) {
		if(fscanf(mpd_fp, "%[^:]: %[^\n]\n", type, value) != 2)
			continue;
	#endif

		if(strncmp(type, "state", 5)==0) {
			if(strncmp(value, "play", 4)==0)
				mp_stat.status = 1;
			else if(strncmp(value, "stop", 4)==0)
				mp_stat.status = 2;
			else
				mp_stat.status = 0;
		} else if(strncmp(type, "time", 4)==0) {
			mp_stat.position = atoi(value);
			dp = strstr(value, ":");
			dp++;
			mp_stat.duration = atoi(dp);
		} else if(strncmp(type, "Artist", 6)==0) {
			strncpy(mp_stat.artist, value, 128);
		} else if(strncmp(type, "Album", 5)==0) {
			strncpy(mp_stat.album, value, 128);
		} else if(strncmp(type, "Title", 5)==0) {
			strncpy(mp_stat.title, value, 128);
		} else if(strncmp(type, "repeat", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mp_stat.repeat = 1;
			else
				mp_stat.repeat = 0;
		} else if(strncmp(type, "random", 6)==0) {
			if(strncmp(value, "1", 1)==0)
				mp_stat.shuffle = 1;
			else
				mp_stat.shuffle = 0;
		} else if(strncmp(type, "volume", 6)==0) {
			mp_stat.volume = atoi(value);
		}
	}

	return 1;
}

*/

int read_clock(int num, char type[3], unsigned int *target) {
	static char filename[BUF_SIZE];
	FILE *fp;

	snprintf(filename, BUF_SIZE, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_%s_freq", num, type);
	fp = fopen(filename, "r");
	if(fp==NULL)
		return 0;

	if(fscanf(fp, "%u", target) != 1) {
		fclose(fp);
		return 0;
	}

	fclose(fp);
	return 1;
}



void die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	char stext[max_status_length], ostext[max_status_length];
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

	check_cpus();
	check_batteries();
	check_clocks();
	check_therms();
    check_brightness();
#ifdef USE_SOCKETS
	check_mp();
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

           if(strcmp(stext, ostext)!=0) {
#ifdef USE_X11
				XChangeProperty(dpy, root, XA_WM_NAME, XA_STRING, 8, PropModeReplace, (unsigned char*)stext, strlen(stext));
				XFlush(dpy);
#else
				printf("%s\n", stext);
#endif
            }
                        strcpy(ostext, stext);
			sleep(refresh_wait);
		}

// #ifdef USE_SOCKETS
// 	if(cmus_sock) close(cmus_sock);
// #endif

	return 0;
}
