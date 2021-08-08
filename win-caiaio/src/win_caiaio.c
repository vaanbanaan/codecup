#include <stdio.h>
#include <sys/time.h>
#include "commandline.h"
#include "io.h"
#include "command.h"
#include "mainloop.h"
#include "prflush.h"
#include "debug.h"

int main(int argc, char **argv)
{
	struct timeval start, stop;
	int sec, usec;
	char *executable = "manager.exe";

	commandline(argc, argv);
	if (!cmd_manager) {
        cmd_manager = executable;
	}
	if (!create_manager(cmd_manager, cmd_manager_file)) {
        printf("win-caiaio error\n");
		return 1;
	}
	if (!start_manager()) {
        return 1;
	}

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

	gettimeofday(&start, NULL);
	mainloop();
	gettimeofday(&stop, NULL);

	stop_players();
	stop_referee();
	stop_manager();

	sec = (stop.tv_sec - start.tv_sec);
	usec = (stop.tv_usec - start.tv_usec);
	if (usec < 0)
	{
		sec--;
		usec += 1000000;
	}
	debug(DEBUG_GOODBYE_STRING, sec + (usec + 500000) / 1000000);
	return 0;
}
