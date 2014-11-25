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
	int amax = 0, bmax = 0, xmax = 0, i, x;

	val = val < 0 ? 0 : (val > 1 ? 1 : val);

	for(i = 0; i < 3; i++) {
		x = h2i(ca[i]);
		if(x>amax) amax = x;
		x = h2i(cb[i]);
		if(x>bmax) bmax = x;
	}

	for(i = 0; i < 3; i++) {
		a[i] = h2i(ca[i]) * val + h2i(cb[i]) * (1 - val);
		if(a[i]>xmax) xmax = a[i];
	}

	s = ((double)amax * val + (double)bmax * (1 - val)) / (double)xmax;

	for(i = 0; i < 3; i++) {
		x = a[i] * s;
		r[i] = x>9 ? 'a' + x - 10 : '0' + x;
	}
	r[3] = 0;

	return;
}


/* +++ FORMAT FUNCTIONS +++ */
static inline void datetime_format(char *status) {
	const struct tm* lt;
	lt = localtime(&datetime_stat.time);
	strftime(status + strlen(status), max_status_length - strlen(status), "^[f777;%d.%b %H:%M^[f;", lt);
}

static inline void cpu_format(char *status) {
    static char hv[4];
	unsigned int perc;
	int i, p;

	//aprintf(status, " ");

	for(i=0; i<cpu_stat.num_cpus; i++) {
		perc = cpu_stat.perc[i];

		if(perc>100) perc=100;

	    hexfade("f34", "3f4", perc / 100.0, hv);

		p = perc / 10;
		// ^[i15;	 : Show pixmap CPU Symbol
		// ^[f%x%x0; : Set foreground color (fading from green to red)
		// ^[f;		 : Set default color
		// ^[d;		 : Show delimiter
		aprintf(status, "^[f%s;^[v%d;^[f;", hv, p>9 ? 9 : p);
	}

	aprintf(status, " ");
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

	for(i=0; i<clock_stat.num_clocks; i++) {
		perc = clock_stat.clocks[i] - clock_stat.clock_min;
		perc = perc ? (perc * 100) / (clock_stat.clock_max - clock_stat.clock_min) : 0;
		if(i<clock_stat.num_clocks-1)
			aprintf(status, "^[fea0;^[G15,%d;", perc / 10);
		else
			aprintf(status, "^[fea0;^[G15,%d;^[f;", perc / 10);
	}
}

static inline void therm_format(char *status) {
	int i, perc;
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

static inline void calc_traf_sym(int traf, char *status, char *sym, char *col, char *col2) {
    static char hv[4];

    hexfade(col, col2, (double)traf/((double)500*(double)1024), hv);

    if(traf>1024*1024) {
        traf /= 1024*1024;
        aprintf(status, "^[f%s;%s%dM", hv, sym, traf);
    } else if(traf>1024) {
        traf /= 1024;
        aprintf(status, "^[f%s;%s%dK", hv, sym, traf);
    } else if(traf>20) {
        aprintf(status, "^[f%s;%s%d", hv, sym, traf);
    }
    aprintf(status, "^[f444;%s", sym);
}

static inline void net_format(char *status) {
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
			//aprintf(status, "^[f%s;^[i38;^[f%s;^[i35;", dtx<100 ? "444" : "845", drx<100 ? "444" : "584");
			calc_traf_sym(dtx, status, "^[i38;", "f45", "645");
			calc_traf_sym(drx, status, "^[i35;", "5f4", "564");
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
    static int last[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, pos=0, mean=0;

	for(i=0; i<battery_stats.num_bats; i++) {
		if(battery_stats.state[i]==BatUnknown) {
            if(!battery_stats.rate) battery_stats.state[i] = BatCharged;
        }
        if(battery_stats.state[i]!=BatCharged) cstate = 0;
		if(battery_stats.state[i]==BatDischarging) dstate = 1;
		if(battery_stats.state[i]==BatCharging) dstate = -1;
		if(battery_stats.rate[i]) rate = battery_stats.rate[i];
	}
	last[pos++] = rate;
	if(pos>=10) {
		for(int i=0; i<10; i++) {
			mean += last[i];
		}
		mean = mean / 10;
		pos = 0;
	} else if(mean==0) {
		mean = rate;
	}

	if(cstate) {
		aprintf(status, "^[f539;^[i0;^[f;");
	} else {
		for(i=0; i<battery_stats.num_bats; i++) {
			perc = battery_stats.capacity[i] ? battery_stats.remaining[i] / (battery_stats.capacity[i] / 100) : 0;
			if(battery_stats.state[i]==BatCharging/* || (dstate==-1 && battery_stats[i].state==BatCharged)*/) {
                hexfade("3f4", "f34", perc / 100.0, hv);
                aprintf(status, "^[f%s;^[g0,%d;^[f;", hv, perc / 10);
				totalremaining += battery_stats.rate[i] ? ((battery_stats.capacity[i]-battery_stats.remaining[i]) * 60) / battery_stats.rate[i] : 0;
			} else if(battery_stats.state[i]==BatDischarging || (dstate==1 && (battery_stats.state[i]==BatCharged || battery_stats.state[i]==BatUnknown))) {
                hexfade("3f4", "f34", perc / 100.0, hv);
				aprintf(status, "^[f%s;^[g9,%d;^[f;", hv, perc / 10);
				totalremaining += (mean ? (battery_stats.remaining[i] * 60) / mean : 0);
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
        aprintf(status, "^[f%s;^[i56;^[f;%s", hv, delimiter);
    }
}

static inline void shrtn(char *s) {
	char *n = s;
	s++;
	n++;
	while(*s) {
		switch(*s) {
			case 'a':
			case 'e':
			case 'i':
			case 'o':
			case 'u':
				s++;
				break;
			case ' ':
				s++;
				while(*s && !((*s>'a' && *s<'z') || (*s>'A' && *s<'Z') || (*s>'0' && *s<'9'))) s++;
				if(*s && *s>'a' && *s<'z') {
					*n = *s + 'A'-'a';
					s++;
					n++;
				}
				break;
			default:
				if((*s>'a' && *s<'z') || (*s>'A' && *s<'Z') || (*s>'0' && *s<'9')) {
					*n = *s;
					s++;
					n++;
				} else {
					s++;
				}
		}
	}
	*n = '\0';
}

#ifdef USE_SOCKETS
static inline void mp_format(char *status) {
	int p, v;
	if(mp_stat.status>0) {
		//shrtn(mp_stat.artist);
		//shrtn(mp_stat.title);
		aprintf(status, "^[feb2;%s^[f26c;-^[fe60;%s", mp_stat.artist, mp_stat.title);
		if(mp_stat.status==1) {
			if(mp_stat.duration>=0 && mp_stat.duration>=0) {
				p = mp_stat.duration ? (mp_stat.position * 10) / mp_stat.duration : 0;
				aprintf(status, "^[d1;^[f26c;^[h%d;", p);
			}
			aprintf(status, "^[d1;%s%s", mp_stat.repeat ? "^[f999;r" : "^[f555;1", mp_stat.shuffle ? "^[f999;s" : "^[f555; ");
			if(mp_stat.volume>=0) {
				v = mp_stat.volume * 10 / 100;
				if(p>9) p = 9;
				if(v>9) v = 9;
				aprintf(status, "^[d1;^[f845;^[g51,%d;", v);
			}
		}
		aprintf(status, "^[f0;%s", delimiter);
	}
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
