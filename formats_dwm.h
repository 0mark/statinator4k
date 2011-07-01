/* +++ FORMAT FUNCTIONS +++ */
static inline void datetime_format(char *status) {
	strftime(status + strlen(status), max_status_length - strlen(status), "%d %b %Y - %I:%M", localtime(&datetime_stat.time));
}

static inline void cpu_format(char *status) {
	static unsigned int o_running=0, o_total=0;
	unsigned int running = cpu_stat.user + cpu_stat.nice + cpu_stat.system, total = running + cpu_stat.idle;
	unsigned int perc = ((running - o_running) * 100) / (total - o_total);

	if(perc>100) perc=100;

	aprintf(status, "C %d%%", perc);

	o_running = running;
	o_total = total;
}

static inline void mem_format(char *status) {
	int free = mem_stat.free + mem_stat.buffers + mem_stat.cached;
	int perc = mem_stat.total ? (free * 100) / mem_stat.total : 0;

	aprintf(status, "M %d%%", perc);
}

static inline void clock_format(char *status) {
	int i, clk;
	char *s, m[]="mHz", g[]="gHz";

	for(i=0; i<clock_stat.num_clocks; i++) {
		if(clock_stat.clocks[i]>1000000) {
			clk = clock_stat.clocks[i] / 1000000;
			s = g;
		} else {
			clk = clock_stat.clocks[i] / 1000;
			s = m;
		}
		if(i<clock_stat.num_clocks-1)
			aprintf(status, "%d%s, ", clk, s);
		else
			aprintf(status, "%d%s", clk, s);
	}
}

static inline void therm_format(char *status) {
	int i, t;
	// TODO: get WARNING temp from sys!

	for(i=0; i<therm_stat.num_therms; i++) {
		t = therm_stat.therms[i] / 1000;
		if(t>75)
			aprintf(status, "WARNING %d°", t);
		else
			aprintf(status, "%d°", t);
		if(i<therm_stat.num_therms-1)
			aprintf(status, ", ");
	}
	aprintf(status, "%s", delimiter);
}

static inline void net_format(char *status) {
	int i;
	if(net_stat.count>0) {
		for(i=0; i<net_stat.count; i++)
			if(strncmp(net_stat.devnames[i], "lo", 2)) {
				if(i<net_stat.count-1)
					aprintf(status, "%s UP, ", net_stat.devnames[i]);
				else
					aprintf(status, "%s UP", net_stat.devnames[i]);
			}
	} else
		aprintf(status, "net DOWN");
}

static inline void wifi_format(char *status) {
	aprintf(status, "%s %d%%", wifi_stat.devname, wifi_stat.perc);
}

static inline void battery_format(char *status) {
	int i;
	int totalremaining = 0;

	int cstate = 1;
	for(i=0; i<num_batteries; i++)
		if(battery_stats[i].state!=BatCharged) cstate = 0;
	if(cstate) {
		aprintf(status, "=D~");
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
