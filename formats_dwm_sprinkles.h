/*
 * This is an example for a fully personalized output.
 * This file is optimized for my needs. For more generic
 * examples look at formats_dwm.h and formats_dwm_colorbar.h
 */

int h2i(char c) {
    if(c>'0' && c<='9') return 9 - ('9' - c);
    if(c>='a' && c<='f') return 15 - ('f' - c);
    return 0;
}

void hexfade(char *ca, char *cb, double val, char r[4]) {
  char a[4];
  double s;
  int max = 0, i, x, amax = 0;

  val = val < 0 ? 0 : (val > 1 ? 1 : val);

  for(i = 0; i < 3; i++) {
    x = h2i(ca[i]);
    if(x>max) max = x;
    x = h2i(cb[i]);
    if(x>max) max = x;
  }

  for(i = 0; i < 3; i++) {
    a[i] = h2i(ca[i]) * val + h2i(cb[i]) * (1 - val);
    if(a[i]>amax) amax = a[i];
  }

  s = (double)max / (double)amax;
  for(i = 0; i < 3; i++) {
    x = a[i] * s;
    r[i] = x>9 ? 'a' + x - 10 : '0' + x;
  }
  r[3] = 0;

  return;
}


/* +++ FORMAT FUNCTIONS +++ */
static inline void datetime_format(char *status) {
	strftime(status + strlen(status), max_status_length - strlen(status), "^[f777;%d.%b %H:%M^[f;", localtime(&datetime_stat.time));
}

static inline void cpu_format(char *status) {
	static unsigned int o_running=0, o_total=0;
    static char hv[4];
	unsigned int running = cpu_stat.user + cpu_stat.nice + cpu_stat.system, total = running + cpu_stat.idle;
	unsigned int perc = (total - o_total) ? ((running - o_running) * 100) / (total - o_total) : 0;

	if(perc>100) perc=100;

    hexfade("f34", "3f4", perc / 100.0, hv);

	//int col = perc * 11 / 100;
	int p = perc / 10;
	// ^[i15;	: Show pixmap CPU Symbol
	// ^[f%x%x0;	: Set foreground color (fading from green to red)
	// ^[f;		: Set default color
	// ^[d;		: Show delimiter
	//aprintf(status, " ^[f%x%x0;^[v%d;^[f; ", col, 12-col, p>9 ? 9 : p);
	aprintf(status, " ^[f%s;^[v%d;^[f; ", hv, p>9 ? 9 : p);

	o_running = running;
	o_total = total;
}

static inline void mem_format(char *status) {
    static char hv[4];

	int free = mem_stat.free + mem_stat.buffers + mem_stat.cached;
	int perc = mem_stat.total ? (free * 100) / mem_stat.total : 0;

    hexfade("3f4", "f34", perc / 100.0, hv);

	aprintf(status, "^[f%s;^[g31,%d;^[f;%s", hv, perc / 10, delimiter);
}

static inline void clock_format(char *status) {
	int i, perc;
	// TODO: get max and min freqs from sys!

	for(i=0; i<clock_stat.num_clocks; i++) {
		perc = clock_stat.clocks[i] - 800000;
		perc = perc ? (perc * 100) / 530000 : 0;
		//col = perc * 10 / 100;
		if(i<clock_stat.num_clocks-1)
			aprintf(status, "^[fea0;^[G15,%d;", perc / 10);
		else
			aprintf(status, "^[fea0;^[G15,%d;^[f;", perc / 10);
	}
}

static inline void therm_format(char *status) {
	int i, perc;//, col;
    static char hv[4];
	// TODO: get WARNING temp from sys!

	for(i=0; i<therm_stat.num_therms; i++) {
		perc = therm_stat.therms[i] / 1000 - 40;
		perc = perc ? (perc * 100) / 80 : 0;
		if(perc<0) perc = 0;
		if(perc<=100) {
            hexfade("f34", "3f4", perc / 100.0, hv);
			aprintf(status, "^[f%s;%d^[f999;°", hv, therm_stat.therms[i] / 1000);
		} else if(perc>100)
			aprintf(status, "^[bf00;^[i27;^[b; ^[ff00;%d^[f;°", therm_stat.therms[i] / 1000);
		if(i<therm_stat.num_therms-1)
			aprintf(status, " ");
	}
	aprintf(status, "%s", delimiter);
}

static inline void net_format(char *status) {
	/*int i;
	if(net_stat.count>0) {
		for(i=0; i<net_stat.count; i++)
			if(strncmp(net_stat.devnames[i], "lo", 2) && net_stat.rx[i]) {
				aprintf(status, "^[f%s;^[i38;^[f%s;^[i35;", (net_stat.tx[i]-net_stat.ltx[i])>100 ? "444" : "845", (net_stat.rx[i]-net_stat.lrx[i])>100 ? "444" : "584");
				if(i<net_stat.count-1)
					aprintf(status, "^[f555;%s^[f0; ", net_stat.devnames[i]);
				else
					aprintf(status, "^[f555;%s^[f0;", net_stat.devnames[i]);
			}
	} else
		aprintf(status, "^[f555;^[i33;^[f;");*/
	int i, pdev=-1, drx, dtx, dsym = 28;
	if(net_stat.count>0) {
		for(i=0; i<net_stat.count; i++) {
			if(!strncmp(net_stat.devnames[i], "wlan", 3) && net_stat.rx[i] && pdev==-1) {
				pdev = i;
				dsym = 60;
			}
			if(!strncmp(net_stat.devnames[i], "eth", 3) && net_stat.rx[i] && pdev==-1) {
				pdev = i;
				dsym = 39;
			}
			if((!strncmp(net_stat.devnames[i], "tun", 3) || !strncmp(net_stat.devnames[i], "tap", 3)) && net_stat.rx[i]) {
				pdev = i;
				dsym = 50;
			}
			if(!strncmp(net_stat.devnames[i], "usb", 3) && net_stat.rx[i] && pdev==-1) {
				pdev = i;
				dsym = 57;
			}
			if(!strncmp(net_stat.devnames[i], "ppp", 3) && net_stat.rx[i] && pdev==-1) {
				pdev = i;
				dsym = 53;
			}
		}
		if(pdev>=0) {
			dtx = net_stat.tx[pdev]-net_stat.ltx[pdev];
			drx = net_stat.rx[pdev]-net_stat.lrx[pdev];
			aprintf(status, "^[f%s;^[i38;^[f%s;^[i35;", dtx<100 ? "444" : "845", drx<100 ? "444" : "584");
			aprintf(status, "^[f555;^[i%d;^[f0;", dsym);
		} else
			aprintf(status, "^[f555;^[i33;^[f;");
	} else
		aprintf(status, "^[f555;^[i33;^[f;");
	aprintf(status, "%s", delimiter);
}

static inline void wifi_format(char *status) {
    static char hv[4];

    hexfade("3f4", "f34", wifi_stat.perc / 70.0, hv);
	aprintf(status, "^[f%s;^[g60,%d;^[f;%s", hv, wifi_stat.perc / 7, delimiter);
}

static inline void battery_format(char *status) {
	int i, perc;
	int totalremaining = 0;
	int cstate = 1, dstate=0, rate = 0;
    static char hv[4];

	for(i=0; i<num_batteries; i++) {
		if(battery_stats[i].state==BatUnknown) {
            if(!battery_stats[i].rate) battery_stats[i].state = BatCharged;
        }
        if(battery_stats[i].state!=BatCharged) cstate = 0;
		if(battery_stats[i].state==BatDischarging) dstate = 1;
		if(battery_stats[i].state==BatCharging) dstate = -1;
		if(battery_stats[i].rate) rate = battery_stats[i].rate;
	}

	if(cstate) {
		aprintf(status, "^[f539;^[i0;^[f;");
	} else {
		for(i=0; i<num_batteries; i++) {
			perc = battery_stats[i].capacity ? battery_stats[i].remaining / (battery_stats[i].capacity / 100) : 0;
			if(battery_stats[i].state==BatCharging/* || (dstate==-1 && battery_stats[i].state==BatCharged)*/) {
                hexfade("3f4", "f34", perc / 100.0, hv);
                aprintf(status, "^[f%s;^[g0,%d;^[f;", hv, perc / 10);
				totalremaining += battery_stats[i].rate ? ((battery_stats[i].capacity-battery_stats[i].remaining) * 60) / battery_stats[i].rate : 0;
			} else if(battery_stats[i].state==BatDischarging || (dstate==1 && (battery_stats[i].state==BatCharged || battery_stats[i].state==BatUnknown))) {
                hexfade("3f4", "f34", perc / 100.0, hv);
				aprintf(status, "^[f%s;^[g9,%d;^[f;", hv, perc / 10);
				totalremaining += battery_stats[i].rate ? (battery_stats[i].remaining * 60) / battery_stats[i].rate : (rate ? (battery_stats[i].remaining * 60) / rate : 0);
			}
		}
		if(totalremaining>=0)
			aprintf(status, " ^[f444;[^[fe84;%d:%02d^[f;]^[f0;", totalremaining / 60, totalremaining % 60);
	}
	aprintf(status, "%s", delimiter);
}

static inline void brightness_format(char *status) {
    int i;
    static char hv[4];

    for(i=0; i<brightness_stat.num_brght; i++) {
        hexfade("3f4", "f34", 1.0 * brightness_stat.brghts[i] / brightness_stat.max_brghts[i], hv);
        //printf("%s\n", hv);
        aprintf(status, "^[f%s;^[i56;^[f;%s", hv, delimiter);
    }
}

#ifdef USE_SOCKETS
static inline void cmus_format(char *status) {
	int p, v;
	if(mp_stat->status>0) {
		aprintf(status, " ^[feb2;%s^[f26c;-^[fe60;%s", mp_stat->artist, mp_stat->title);
		if(mp_stat->status==1) {
			p = mp_stat->duration ? (mp_stat->position * 10) / mp_stat->duration : 0;
			v = mp_stat->volume * 10 / 100;
			if(p>9) p = 9;
			if(v>9) v = 9;
			aprintf(status, "^[d1;^[f26c;^[h%d;^[d1;%s%s^[d1;^[f845;^[g51,%d;", p, mp_stat->repeat ? "^[f999;r" : "^[f555;1", mp_stat->shuffle ? "^[f999;s" : "^[f555; ", v);
		}
		aprintf(status, "^[f0;%s", delimiter);
	} //else
		//aprintf(status, " ");
		//aprintf(status, "^[f555;^[i54;^[f;%s", delimiter);
}
#endif

#ifdef USE_ALSAVOL
static inline void alsavol_format(char *status) {
    static char hv[4];
	int tvol = alsavol_stat.vol_max - alsavol_stat.vol_min;
	int perc = tvol ? ((alsavol_stat.vol - alsavol_stat.vol_min) * 10) / tvol : 0;

    hexfade("39d", "343", perc / 10.0, hv);

	aprintf(status, "^[f%s;^[g51,%d;%s", hv, perc, delimiter);
}
#endif

#ifdef USE_NOTIFY
static inline void notify_format(char *status) {
	char fmt[message_length+30];
	int offset;
	int remaining = (notify_stat.message->started_at + notify_stat.message->expires_after) - time(NULL);
	//if(remaining>9) remaining = 9;
	//aprintf(status, "(%d, %d) ", remaining, notify_stat.message->expires_after);
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
