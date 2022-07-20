#ifndef WIN_CAIAIO_IO_H
#define WIN_CAIAIO_IO_H

#define IO_NAME_TAG "I"

#define PIPE_SIZE 4096
#define BUF_SIZE 4096

// stderr screen reader dimensions: width * heigth
#define STDERR_SCREEN_WIDTH 128
#define STDERR_SCREEN_HEIGHT 32

#define PROCESS_INIT   		0
#define PROCESS_ACTIVE 		1
#define PROCESS_STOPPED		2
#define PROCESS_BROKEN_PIPE	3

// stderr console mode
#define STDERR_NO_REDIR     0
#define STDERR_SCREEN	    1
#define STDERR_NULL         2

#include <windows.h>

struct buffer_ctx;
struct stderr_ctx;

struct process_ctx {
    int state;
	unsigned long exit_code;
	unsigned int response_time_msec;
	PROCESS_INFORMATION process_info;
	HANDLE write_stdin,
		   read_stdout;
	struct buffer_ctx *buffer_ctx_in;
	struct stderr_ctx *stderr_ctx; // player stderr screen buffer -> logfile / debug / NULL */
};


struct buffer_ctx {
	char *buffer;
	unsigned long data_length;
	OVERLAPPED stOverlapped;
	unsigned long read_status;
};


struct stderr_ctx {
	HANDLE screen_handle,
		   log_handle;
	char *screen_copy_buffer;
	char *stderr_buffer;
	unsigned long buffer_size,
				  data_length;
};


struct process_ctx *create_process_ctx();
void free_process_ctx(struct process_ctx **process_ctx);

BOOL create_stderr_ctx(struct process_ctx *process_ctx);
BOOL create_write_file(char *logfile, LPHANDLE write_handle);

BOOL spawn_suspended_process(struct process_ctx *process_ctx, char *command, char *argument);

BOOL wait_firsterror(struct process_ctx *process_ctx, unsigned int wait_msec);
BOOL read_stderr_screen(struct stderr_ctx *stderr_ctx);

BOOL write_to_handle(HANDLE handle, char *message, unsigned long length);
BOOL read_from_timed_process(struct process_ctx *process_ctx, unsigned int wait_msec);

BOOL check_alive(struct process_ctx *process_ctx);
VOID terminate_process(struct process_ctx *process_ctx);

#endif // WIN_CAIAIO_IO_H
