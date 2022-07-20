#include <stdio.h>
#include <windows.h>
#include <sys/time.h>
#include "commandline.h"
#include "command.h"
#include "mainloop.h"
#include "prflush.h"
#include "debug.h"

#define UNUSED(x) (void)(x)

BOOL WINAPI abort_program(DWORD fdwCtrlType);

HANDLE dup_main_thread_handle;

int main(int argc, char **argv)
{
	struct timeval start, stop;
	int sec, usec, i;
	char *executable = "manager.exe";
    DWORD_PTR ProcessAffinityMask, SystemAffinityMask, first_cpu;

	if (!DuplicateHandle(GetCurrentProcess(),
						GetCurrentThread(),
						GetCurrentProcess(),
						&dup_main_thread_handle,
						0,
						FALSE,
						DUPLICATE_SAME_ACCESS)) {
        printf("%s error\n", argv[0]);
		return 1;
	}
    if (!SetConsoleCtrlHandler(abort_program, TRUE)) {
        printf("%s error\n", argv[0]);
        return 1;
    }
    // Force caiaio and all processes it starts to run on first CPU only
    // (manager / competition, referee and players)
    if (!GetProcessAffinityMask(GetCurrentProcess(),
                &ProcessAffinityMask, &SystemAffinityMask)) {
        printf("%s error\n", argv[0]);
        return 1;
    }
    i = __builtin_ctzll(ProcessAffinityMask);
    first_cpu = ProcessAffinityMask & (1ULL << i);
    if (!ProcessAffinityMask ||
        !SetProcessAffinityMask(GetCurrentProcess(), first_cpu)) {
        printf("%s error\n", argv[0]);
        return 1;
    }
    // Disable the Windows "Program has stopped working" dialog
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);


	commandline(argc, argv);
	if (!cmd_manager) {
        cmd_manager = executable;
	}
	if (!create_manager(cmd_manager, cmd_manager_file)) {
        printf("%s error\n", argv[0]);
		return 1;
	}

	if (!start_manager()) {
        return 1;
	}

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

BOOL WINAPI abort_program(DWORD fdwCtrlType)
{
    UNUSED(fdwCtrlType);
    printf("\n** Abort **\n");

	SuspendThread(dup_main_thread_handle);
	stop_players();
	stop_referee();
	stop_manager();
	CloseHandle(dup_main_thread_handle);

    return FALSE;
}
