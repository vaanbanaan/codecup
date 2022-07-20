#ifndef COMMAND_H
#define COMMAND_H

#define FAILURE 0
#define SUCCESS 1

#define BUF_ERROR_NO_DATA 2
#define BUF_ERROR_NO_EOL 3
#define BUF_ERROR_EMPTY_LINE 4

#define MAXPLAYERS 32

#include "io.h"

struct player { // ﻿manager: I player <number> <name> <time> [<log>]
	int number;
	struct process_ctx *process_ctx;
	char *executable; // <name>
	char *logfile; // [<log>]
	int total_time_msec; // <time>
	int firsterror_time_msec; // manager: ﻿I start <number> [<err_time>]
	int cpu_time_msec; //referee: ﻿I request_time <number>
};

struct manager {
	struct process_ctx *process_ctx;
	char *executable, // at startup: ﻿./caiaio [-d] [-f <information>] [-m <executable>]
         *information;
	int number_players; // ﻿I number_players <number>
};

struct referee {  // manager: ﻿I referee <name> [<log>]
	struct process_ctx *process_ctx;
	char *executable, // <name> [<log>]
         *logfile;
};

extern struct manager *manager;
extern struct referee *referee;
extern struct player **player;
extern int nplayers;

char create_manager(const char *executable, const char *information);
char start_manager();
void stop_manager();

char create_referee(const char *executable, const char *logfile);
char start_referee();
void stop_referee();

char create_player_array(unsigned int number_players);
char create_player(unsigned int number, char *executable, int total_time_msec, char *logfile);
char start_player(int player_id);
void stop_player(int player_id);
void stop_players();

char has_firsterror(struct process_ctx *process_ctx, int firsterror_time_msec);
char read_stderr_line(struct stderr_ctx *stderr_ctx, char *message, unsigned long *length);

char send_message(struct process_ctx *process_ctx, char *message);
char write_logfile(struct process_ctx *process_ctx, char *message, unsigned long length);

char receive_timed_process_message(char *message, unsigned long *length, struct process_ctx *process_ctx, int max_wait_msec);
char receive_player_message(char *message, unsigned long *length, int player_id);

#endif // COMMAND_H
