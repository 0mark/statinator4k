/* +++ CONFIG +++ */
static int max_big_messages    = 0;         // max num of messages (long texts) before shortening
static int refresh_wait        = 1;         // time between refresh in seconds
static int max_status_length   = 512;       // max length of status
static int auto_delimiter      = 1;         // automagically add delimiter on success
static char delimiter[]        = " | ";     // delimiter
static char *brightnes_names[] = { "acpi" };
#ifdef USE_NOTIFY
static int marquee_chars       = 30;        // TODO: description!
static int marquee_offset      = 3;         // TODO: description!
static int message_length      = 10;        // TODO: description!
#endif
#ifdef USE_SOCKETS
static char cmus_adress[]      = "/home/USER/.cmus/socket"; // socket adressfor cmus
static char mpd_adress[]       = "127.0.0.1"; // socket adressfor cmus
static int mpd_port            = 6600;      // socket adressfor cmus
#endif
#ifdef USE_ALSAVOL
static const char ATTACH[]     = "default"; // device
static const char SELEM_NAME[] = "Master";  // mixer
#endif

// TODO: description
static int status_funcs_order[] = {
#ifdef USE_SOCKETS
	MPD,
#endif
	CPU, CLOCK, THERM, MEM, NET, WIFI, BATTERY,
#ifdef USE_ALSAVOL
	AVOL,
#endif
	DATETIME, };

// TODO: description
#ifdef USE_NOTIFY
static int message_funcs_order[] = { NOTIFY, };
#else
#define NO_MSG_FUNCS
#endif

#include FORMAT_METHOD
