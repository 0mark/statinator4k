unsigned int o_running=0, o_total=0;
inline void cpu_format(char *status, cpu_stat *stat) {
	unsigned int running = stat->user + stat->nice + stat->system, total = running + stat->idle;
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

	o_running=running;
	o_total=total;
}


inline void wifi_format(char *status, wifi_stat *stat) {
	aprintf(status, "%s=%d%%", stat->devname, stat->perc);
}


inline void battery_format(char *status) {
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
#define MARQUEE_CHARS 30
#define MARQUEE_OFFSET 3
inline void notify_format(char *status) {
	char fmt[10];
	int offset;
	int remaining = (notify_stat.message->started_at + notify_stat.message->expires_after) - time(NULL);

	if(remaining>0)
		aprintf(status, "%d ", remaining);

	aprintf(status, "%s: %s", notify_stat.message->appname, notify_stat.message->summary);

	if(notify_stat.message->body[0]!=0) {
		if(strlen(notify_stat.message->body) < MARQUEE_CHARS) {
			aprintf(status, " [%s]", notify_stat.message->body);
		} else {
			offset = (time(NULL) - notify_stat.message->started_at) - 1;
			offset *= MARQUEE_OFFSET;
			if(offset > strlen(notify_stat.message->body) - MARQUEE_CHARS)
				offset = strlen(notify_stat.message->body) - MARQUEE_CHARS;
			if(offset<0)
				offset = 0;
			snprintf(fmt, 10, " [%%.%ds]", MARQUEE_CHARS);
			aprintf(status, fmt, notify_stat.message->body + offset);
		}
	}
}
#endif
