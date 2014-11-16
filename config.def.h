/* +++ CONFIG +++ */
static int max_big_messages    = 0;         // max num of messages (long texts) before shortening
static int refresh_wait        = 1;         // time between refresh in seconds
static int max_status_length   = 512;       // max length of status
static int auto_delimiter      = 0;         // automagically add delimiter on success
static char delimiter[]        = "^[f37C;|^[f;";    // delimiter ^[d;
static char *brightnes_names[] = { "acpi_video0" };
static char *clockfile         = "cpuinfo";
#ifdef USE_NOTIFY
static int marquee_chars       = 30;        // TODO: description!
static int marquee_offset      = 3;         // TODO: description!
static int message_length      = 10;        // TODO: description!
#endif
#ifdef USE_SOCKETS
static char mp_adress[]        = "127.0.0.1";
static int mp_port             = 6600;
static char (*mp_parse)()      = mpd_parse;
#endif
#ifdef USE_ALSAVOL
static const char ATTACH[]     = "default"; // device
static const char SELEM_NAME[] = "Master";  // mixer
#endif

static int status_funcs_order[] = {
    NET,
#ifdef USE_SOCKETS
	MP,
#endif
	/*CLOCK,*/ CPU, THERM, MEM, WIFI, BATTERY, BRIGHTNESS,
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
