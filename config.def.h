/* +++ CONFIG +++ */
static int max_big_messages  = 0;   // max num of messages (long texts) before shortening
static int refresh_wait      = 1;   // time between refresh in seconds
static int max_status_length = 512; // max length of status
static int auto_delimiter    = 1;
static char delimiter[] = " | ";
#ifdef USE_NOTIFY
static int marquee_chars     = 30;
static int marquee_offset    = 3;
static int message_length    = 10;
#endif
#ifdef USE_SOCKETS
static char cmus_adress[] = "/home/USER/.cmus/socket";
#endif

static int status_funcs_order[] = { CPU, CLOCK, THERM, MEM, NET, WIFI, BATTERY, DATETIME, };
static int message_funcs_order[] = { NOTIFY, };

#include FORMAT_METHOD
