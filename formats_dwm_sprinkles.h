/* +++ FORMAT FUNCTIONS +++ */
static inline void datetime_format(char *status) {
	strftime(status + strlen(status), max_status_length - strlen(status), "^[f777;%d.%b %H:%M^[f;", localtime(&datetime_stat.time));
}

static inline void cpu_format(char *status) {
	static unsigned int o_running=0, o_total=0;
	unsigned int running = cpu_stat.user + cpu_stat.nice + cpu_stat.system, total = running + cpu_stat.idle;
	unsigned int perc = ((running - o_running) * 100) / (total - o_total);

	if(perc>100) perc=100;

	int col = perc * 15 / 100;
	// ^[i15;	: Show pixmap CPU Symbol
	// ^[f%x%x0;	: Set foreground color (fading from green to red)
	// ^[f;		: Set default color
	// ^[d;		: Show delimiter
	aprintf(status, " ^[f%x%x0;%d%%^[f;%s", col, 15-col, perc, delimiter);

	o_running = running;
	o_total = total;
}

static inline void mem_format(char *status) {
	int free = mem_stat.free + mem_stat.buffers + mem_stat.cached;
	int perc = mem_stat.total ? (free * 100) / mem_stat.total : 0;

	int col = perc * 15 / 100;
	aprintf(status, "^[f%x%x0;^[G31,%d;^[f;%s", 15-col, col, perc/10, delimiter);
}

static inline void clock_format(char *status) {
	int i, perc, col;
	// TODO: get max and min freqs from sys!

	for(i=0; i<clock_stat.num_clocks; i++) {
		perc = clock_stat.clocks[i] - 600000;
		perc = perc ? (perc * 100) / 1000000 : 0;
		col = perc * 15 / 100;
		if(i<clock_stat.num_clocks-1)
			aprintf(status, " ^[f%x%x0;^[G15,%d;^[f;", col, 15-col, perc/10);
		else
			aprintf(status, " ^[f%x%x0;^[G15,%d;^[f;", col, 15-col, perc/10);
	}
}

static inline void net_format(char *status) {
	int i;
	if(net_stat.count>0) {
		for(i=0; i<net_stat.count; i++)
			if(strncmp(net_stat.devnames[i], "lo", 2)) {
				if(i<net_stat.count-1)
					aprintf(status, "^[fe81;^[i37;^[f555;%s^[f0;, ", net_stat.devnames[i]);
				else
					aprintf(status, "^[fe81;^[i37;^[f555;%s^[f0;", net_stat.devnames[i]);
			}
	} else
		aprintf(status, "^[f555;^[i33;^[f;");
	aprintf(status, "%s", delimiter);
}

static inline void wifi_format(char *status) {
	int col = wifi_stat.perc * 15 / 100;
	aprintf(status, "^[f%x%x0;^[G60,%d;^[f;%s", 15-col, col, wifi_stat.perc/10, delimiter);
}

static inline void battery_format(char *status) {
	int i, col, perc;
	int totalremaining = 0;

	int cstate = 1;
	for(i=0; i<num_batteries; i++)
		if(battery_stats[i].state!=BatCharged) cstate = 0;
	if(cstate) {
		aprintf(status, "^[f539;^[i0;^[f;");
	} else {
		for(i=0; i<num_batteries; i++) {
			perc = battery_stats[i].capacity ? (100 * battery_stats[i].remaining) / battery_stats[i].capacity : 0;
			col = wifi_stat.perc * 15 / 100;
			if(battery_stats[i].state==BatCharging) {
				aprintf(status, "^[f%x0%x;^[g0,%d;^[f;", 11-col/2, 7+col/2, perc/10);
				totalremaining += battery_stats[i].rate ? ((battery_stats[i].capacity-battery_stats[i].remaining) * 60) / battery_stats[i].rate : 0;
			} else if(battery_stats[i].state==BatDischarging) {
				aprintf(status, "^[f%x%x0;^[g9,%d;^[f;", 15-col, col, perc/10);
				totalremaining += battery_stats[i].rate ? (battery_stats[i].remaining * 60) / battery_stats[i].rate : 0;
			}
		}
		if(totalremaining)
			aprintf(status, " ^[f444;[^[fe84;%d:%02d^[f;]^[f0;", totalremaining / 60, totalremaining%60);
	}
	aprintf(status, "%s", delimiter);
}

#ifdef USE_NOTIFY
static inline void notify_format(char *status) {
	char fmt[message_length+30];
	int offset;
	int remaining = (notify_stat.message->started_at + notify_stat.message->expires_after) - time(NULL);

	if(remaining>0)
		aprintf(status, " ^[fc82;^[g21,%d;^[f; ", remaining);

	aprintf(status, "^[f88e;%s^[f;: ^[f999;%s^[f;", notify_stat.message->appname, notify_stat.message->summary);

	if(notify_stat.message->body[0]!=0) {
		if(strlen(notify_stat.message->body) < marquee_chars) {
			aprintf(status, " ^[f444;[^[fe84;%s^[f;]^[f0;", notify_stat.message->body);
		} else {
			offset = (time(NULL) - notify_stat.message->started_at) - 1;
			offset *= marquee_offset;
			if(offset > strlen(notify_stat.message->body) - marquee_chars)
				offset = strlen(notify_stat.message->body) - marquee_chars;
			if(offset<0)
				offset = 0;
			snprintf(fmt, message_length+30, " ^[f444;[^[fe84;%%.%ds^[f;]^[f0;", marquee_chars);
			aprintf(status, fmt, notify_stat.message->body + offset);
		}
	}
}
#endif
