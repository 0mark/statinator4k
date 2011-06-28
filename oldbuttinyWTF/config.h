/* +++ CONFIG +++ */
static int max_big_messages  = 0;   // max num of messages (long texts) before shortening
static int refresh_wait      = 1;   // time between refresh in seconds
static int max_status_length = 512; // max length of status
static char delimiter[] = " ## ";
#ifdef USE_NOTIFY
static int marquee_chars     = 30;
static int marquee_offset    = 3;
static int message_length    = 10;
#endif

static int status_funcs_order[] = { CPU, CLOCK, MEM, WIFI, BATTERY, DATETIME, };
static int message_funcs_order[] = { NOTIFY, };


/* +++ FORMAT FUNCTIONS +++ */
static inline void datetime_format(char *status) {
	strftime(status + strlen(status), max_status_length - strlen(status), "%d %b %Y - %I:%M", localtime(&datetime_stat.time));
}

static inline void cpu_format(char *status) {
	static unsigned int o_running=0, o_total=0;
	unsigned int running = cpu_stat.user + cpu_stat.nice + cpu_stat.system, total = running + cpu_stat.idle;
	unsigned int perc = ((running - o_running) * 100) / (total - o_total);

	if(perc>100) perc=100;

	aprintf(status, "c=%d%%", perc);

/* Example for http://dwm.suckless.org/patches/statuscolors patch
 * (not tested)
	if(perc>85)
		aprintf(status, "c=\1%d\0%% | ", perc);
	else
		aprintf(status, "c=\2%d\0%% | ", perc);
*/

/* Example for http://0mark.unserver.de/projects/dwm-sprinkles/
 * (not tested)
	int col = (16 / 100) * perc;
	// ^[i15;	: Show pixmap CPU Symbol
	// ^[f%x%x0;	: Set foreground color (fading from green to red)
	// ^[f;		: Set default color
	// ^[d;		: Show delimiter
	aprintf(status, "^[i15; ^[f%x%x0;%d^[f; ^[d;", col, 15-col, perc);
*/

	o_running = running;
	o_total = total;
}

static inline void mem_format(char *status) {
	int free = mem_stat.free + mem_stat.buffers + mem_stat.cached;
	int perc = mem_stat.total ? (free * 100) / mem_stat.total : 0;

	aprintf(status, "m=%d%%", perc);
}

static inline void clock_format(char *status) {
	int i;

	for(i=0; i<clock_stat.num_clocks; i++) {
		if(i<clock_stat.num_clocks-1)
			aprintf(status, "%dMhz, ", clock_stat.clocks[i] / 1000);
		else
			aprintf(status, "%dMhz", clock_stat.clocks[i] / 1000);
	}
}

static inline void wifi_format(char *status) {
	aprintf(status, "%s=%d%%", wifi_stat.devname, wifi_stat.perc);
}

static inline void battery_format(char *status) {
	int i;
	int totalremaining = 0;

	int cstate = 1;
	for(i=0; i<num_batteries; i++)
		if(battery_stats[i].state!=BatCharged) cstate = 0;
	if(cstate) {
		aprintf(status, "=|");
	} else {
		aprintf(status, "||");
		for(i=0; i<num_batteries; i++) {
			if(battery_stats[i].state==BatCharging) {
				aprintf(status, " >%d%%", (100 * battery_stats[i].remaining) / battery_stats[i].capacity);
				totalremaining += (battery_stats[i].remaining * 60) / battery_stats[i].rate;
			} else if(battery_stats[i].state==BatDischarging) {
				aprintf(status, " <%d%%", (100 * battery_stats[i].remaining) / battery_stats[i].capacity);
				totalremaining += (battery_stats[i].remaining * 60) / battery_stats[i].rate;
			}
		}
		if(totalremaining)
			aprintf(status, " [%d:%02d], %d", totalremaining / 60, totalremaining%60, totalremaining);
	}
}

#ifdef USE_NOTIFY
static inline void notify_format(char *status) {
	char fmt[message_length];
	int offset;
	int remaining = (notify_stat.message->started_at + notify_stat.message->expires_after) - time(NULL);

	if(remaining>0)
		aprintf(status, "%d ", remaining);

	aprintf(status, "%s: %s", notify_stat.message->appname, notify_stat.message->summary);

	if(notify_stat.message->body[0]!=0) {
		if(strlen(notify_stat.message->body) < marquee_chars) {
			aprintf(status, " [%s]", notify_stat.message->body);
		} else {
			offset = (time(NULL) - notify_stat.message->started_at) - 1;
			offset *= marquee_offset;
			if(offset > strlen(notify_stat.message->body) - marquee_chars)
				offset = strlen(notify_stat.message->body) - marquee_chars;
			if(offset<0)
				offset = 0;
			snprintf(fmt, message_length, " [%%.%ds]", marquee_chars);
			aprintf(status, fmt, notify_stat.message->body + offset);
		}
	}
}
#endif
