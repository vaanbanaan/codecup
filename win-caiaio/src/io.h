#ifndef WIN_CAIAIO_IO_H
#define WIN_CAIAIO_IO_H

#define IO_NAME_TAG "I"

#define PIPE_SIZE 4096
#define BUF_SIZE 4096

#define PROCESS_INIT   		0
#define PROCESS_ACTIVE 		1
#define PROCESS_STOPPED		2
#define PROCESS_BROKEN_PIPE	3

#include <windows.h>

struct buffer_ctx;
struct stderr_async_ctx;

struct process_ctx {
    int state;
	unsigned long exit_code;
	unsigned int response_time_msec;
	PROCESS_INFORMATION process_info;
	HANDLE write_stdin,
		   read_stdout,
		   read_stderr;
	struct buffer_ctx *buffer_ctx_in;
	struct stderr_async_ctx *stderr_async_ctx; // player stderr read end -> logfile / debug / NULL */
};


struct buffer_ctx {
	char *buffer;
	unsigned long data_length;
	OVERLAPPED stOverlapped;
	unsigned long read_status;
};


struct stderr_async_ctx {
	/*
	 * To pass extra variables to the completion routines: use the Microsoft example trick
	 * https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-completion-routines
	 * It passes the struct with an OVERLAPPED variable as first member, and casts struct pointer to (LPOVERLAPPED)
	 */
   OVERLAPPED stOverlapped;
   char stderr_id;
   BOOL error;
   BOOL stderr_line;
   HANDLE //wait_event,
		  read_stderr,
		  logfile;
   char *pipe_buffer;
};


struct process_ctx *create_process_ctx();
void free_process_ctx(struct process_ctx **process_ctx);

BOOL create_stderr_async_ctx(struct process_ctx *process_ctx, char stderr_id);
BOOL create_write_file(char *logfile, LPHANDLE write_handle);
BOOL activate_async_read_callback(struct stderr_async_ctx *stderr_async_ctx);

BOOL spawn_suspended_process(struct process_ctx *process_ctx, char *command, char *argument, char stderr_redir);
BOOL get_firsterror(struct process_ctx *process_ctx, unsigned int wait_msec, unsigned long *length);

BOOL write_to_handle(HANDLE handle, char *message, unsigned long length);
BOOL read_from_timed_process(struct process_ctx *process_ctx, unsigned int wait_msec);

char check_alive(struct process_ctx *process_ctx);
VOID terminate_process(struct process_ctx *process_ctx);

#endif // WIN_CAIAIO_IO_H
