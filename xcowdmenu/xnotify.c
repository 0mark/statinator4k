/*
	cow-notify - 0.1
	
	Executes command on notification.
	Edit file XDG_CONFIG_HOME/cow-notify/setting
	
	Original code dwmstatus by Jeremy Jay
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>

#include "notify.h"

static char DEBUGGING = 0;

int main(int argc, char **argv)
{	
	if( argc==2 && argv[1][0]=='-' && argv[1][1]=='v' ) {
		DEBUGGING=1;
		fprintf(stderr, "debugging enabled.\n");
	}

	if( !notify_init(DEBUGGING) ) {
		fprintf(stderr, "cannot bind notifications\n");
		return 1;
	}

	while ( 1 )
	{
		if( !notify_check() )
		{
			sleep(1);
		}
	}

	return 0;
}
