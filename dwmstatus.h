
inline void CPU_format(char *status, int perc) {
	aprintf(status, "c=%d%% | ", perc);
}

/* Example for http://dwm.suckless.org/patches/statuscolors patch
 * (not tested)
inline void CPU_format(char *status, int perc) {
	if(perc>85)
		aprintf(status, "c=\1%d\0%% | ", perc);
	else
		aprintf(status, "c=\2%d\0%% | ", perc);
}
*/

/* Example for http://0mark.unserver.de/projects/dwm-sprinkles/
 * (not tested)
inline void CPU_format(char *status, int perc) {
	int col = (16 / 100) * perc;
	// ^[i15;	: Show pixmap CPU Symbol
	// ^[f%x%x0;	: Set foreground color (fading from green to red)
	// ^[f;		: Set default color
	// ^[d;		: Show delimiter
	aprintf(status, "^[i15; ^[f%x%x0;%d^[f; ^[d;", col, 15-col, perc);
}
*/