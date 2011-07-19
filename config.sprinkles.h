/* +++ CONFIG +++ */
/*
 * Example config for use with the dwm-sprinkles formater
 */
static int max_big_messages    = 0;         // max num of messages (long texts) before shortening
static int refresh_wait        = 1;         // time between refresh in seconds
static int max_status_length   = 512;       // max length of status
static int auto_delimiter      = 0;         // automagically add delimiter on success
static char delimiter[]        = "^[d;";    // delimiter
#ifdef USE_NOTIFY
static int marquee_chars       = 30;        // 
static int marquee_offset      = 3;         // 
static int message_length      = 10;        // 
#endif
#ifdef USE_SOCKETS
static char cmus_adress[]      = "/home/USER/.cmus/socket"; // socket adressfor cmus
#endif
#ifdef USE_ALSAVOL
static const char ATTACH[]     = "default"; // device
static const char SELEM_NAME[] = "Master";  // mixer
#endif

static int status_funcs_order[] = {
#ifdef USE_SOCKETS
	CMUS,
#endif
	CLOCK, CPU, THERM, MEM, NET, WIFI, BATTERY, 
#ifdef USE_ALSAVOL
	AVOL,
#endif
	DATETIME, };

#ifdef USE_NOTIFY
static int message_funcs_order[] = { NOTIFY, };
#else
#define NO_MSG_FUNCS
#endif

#include FORMAT_METHOD
