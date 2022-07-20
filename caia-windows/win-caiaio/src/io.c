#include "io.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "pipe_ex.h"
#include "prflush.h"

// Windows Error codes:
// https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes

// temp child pipe-handles ends, parent MUST close them if child process is created
struct child_pipe_ctx {
	HANDLE read_stdin,
		   write_stdout;
};


BOOL create_sync_pipe(LPHANDLE read_handle, LPHANDLE write_handle);
BOOL create_async_read_pipe(LPHANDLE read_handle, LPHANDLE write_handle);
BOOL create_write_file(char *filename, LPHANDLE write_handle);
struct buffer_ctx *create_buffer_ctx();


BOOL create_sync_pipe(LPHANDLE read_handle, LPHANDLE write_handle)
{
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE; /* NOT inheritable */

	if (!CreatePipe(read_handle, write_handle, &sa, PIPE_SIZE)) {
		return FALSE;
	}
	return TRUE;
}


BOOL create_async_read_pipe(LPHANDLE read_handle, LPHANDLE write_handle)
{
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = FALSE; /* NOT inheritable */

	if (!MyCreatePipeEx(
			read_handle,
			write_handle,
			&sa,
			PIPE_SIZE,
			FILE_FLAG_OVERLAPPED, // read side asynchronous
			0) // 0: write side synchronous
	) {
		return FALSE;
	}
	return TRUE;
}


BOOL write_to_handle(HANDLE handle, char *message, unsigned long length)
{
	BOOL bSuccess;
	DWORD dwWritten;

	if (handle == INVALID_HANDLE_VALUE || !message || !length) {
		return FALSE;
	}

    bSuccess = WriteFile(handle, message, length, &dwWritten, NULL); /* synchronous write */

	return (bSuccess && dwWritten == length);
}


BOOL read_from_timed_process(struct process_ctx *process_ctx, unsigned int wait_msec)
{
	// Resumes a process (external program) for a given time to receive a message, then suspends
	struct buffer_ctx *buffer_ctx = process_ctx->buffer_ctx_in;

	DWORD dwWait = WAIT_IO_COMPLETION;
	unsigned int wait_time = wait_msec;
	struct timeval tv_start, tv_response;
	BOOL bSuccess;

	buffer_ctx->data_length = 0;
	process_ctx->response_time_msec = 0;

	if (buffer_ctx->read_status == ERROR_SUCCESS) {
		// ReadFile can read remaining pipe data, even if the attached process is suspended
		bSuccess = ReadFile(process_ctx->read_stdout, buffer_ctx->buffer, 
								BUF_SIZE, &buffer_ctx->data_length, &buffer_ctx->stOverlapped);
		// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
		// If the function fails, or is completing asynchronously, the return value is zero (FALSE).
		// To get extended error information, call the GetLastError function.

		// WELL..... that's the short version
		// Sometimes, EVEN with an overlapped structure, ReadFile still returns TRUE, so the read is synchonous
		// if you would call GetLastError it returns an error value of some other/older function
		if (!bSuccess) {
            buffer_ctx->read_status = GetLastError();
		}
	}

    if (buffer_ctx->read_status == ERROR_IO_PENDING) {
        gettimeofday(&tv_start, NULL);
        if (ResumeThread(process_ctx->process_info.hThread) != 1) {
            process_ctx->exit_code = ERROR_FUNCTION_FAILED;

            return FALSE;
        }
		dwWait = WaitForSingleObject(buffer_ctx->stOverlapped.hEvent, wait_time); // NO need for APC anymore

        gettimeofday(&tv_response, NULL);
        SuspendThread(process_ctx->process_info.hThread);
        process_ctx->response_time_msec = ((tv_response.tv_sec - tv_start.tv_sec) * 1000000 +
                                            tv_response.tv_usec - tv_start.tv_usec) / 1000;
	}

	if (buffer_ctx->read_status == ERROR_BROKEN_PIPE || dwWait == WAIT_FAILED) {
        buffer_ctx->read_status = ERROR_BROKEN_PIPE;

        return FALSE;
	}

	if (dwWait == WAIT_OBJECT_0) {
		// wait is signaled, get response length
		bSuccess = GetOverlappedResult(process_ctx->read_stdout,
										&buffer_ctx->stOverlapped,
										&buffer_ctx->data_length,
										FALSE);
		// getoverlappedresult can fail, maybe use GetLastError()
		// ERROR_BROKEN_PIPE, client crashed or stopped while waiting for input
		// ERROR_MORE_DATA, buffer should be big enough, client probably sent too much data
		if (!bSuccess || BUF_SIZE <= buffer_ctx->data_length) {
			buffer_ctx->read_status = ERROR_BROKEN_PIPE;

			return FALSE;
		}
		ResetEvent(buffer_ctx->stOverlapped.hEvent);
        buffer_ctx->read_status = ERROR_SUCCESS;
	}

	if (buffer_ctx->read_status != ERROR_SUCCESS) {
		return FALSE;
	}

    buffer_ctx->buffer[buffer_ctx->data_length] = '\0';

	if (process_ctx->stderr_ctx) {
		read_stderr_screen(process_ctx->stderr_ctx);
	}

	return TRUE;
}


BOOL wait_firsterror(struct process_ctx *process_ctx, unsigned int wait_msec) {

	CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
	unsigned int wait;

	if (ResumeThread(process_ctx->process_info.hThread) != 1) {
		process_ctx->exit_code = ERROR_FUNCTION_FAILED;

		return FALSE;
	}
	wait = 100;
	if (wait > wait_msec) {
		wait = wait_msec;
	}
	while (wait_msec && !csbi.dwCursorPosition.Y) {
		Sleep(wait);
		GetConsoleScreenBufferInfo(process_ctx->stderr_ctx->screen_handle, &csbi);
		wait_msec -= wait;
		if (wait > wait_msec) {
			wait = wait_msec;
		}
	}
    SuspendThread(process_ctx->process_info.hThread);
	if (!csbi.dwCursorPosition.Y) {
		return FALSE;
	}

	return TRUE;
}


BOOL read_stderr_screen(struct stderr_ctx *stderr_ctx)
{
	DWORD dwDummy, char_read;
	unsigned long buffer_size, data_length;
    char *src, *dst;
    unsigned int cursor_index, skip;
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(stderr_ctx->screen_handle, &csbi);
	cursor_index = csbi.dwCursorPosition.Y * STDERR_SCREEN_WIDTH + csbi.dwCursorPosition.X;

	if (!cursor_index) {
		// No data written to screen
		return TRUE;
	}

    if (! ReadConsoleOutputCharacter(
				stderr_ctx->screen_handle,       // screen buffer to read from
				stderr_ctx->screen_copy_buffer,  // buffer to copy into
				cursor_index,              // number of characters to read
				(COORD){0,0},               // start cell
				&char_read)          // number of characters actually read
	) {
		return FALSE;
	}
	SetConsoleCursorPosition(stderr_ctx->screen_handle, (COORD){0, 0});
	data_length = stderr_ctx->data_length;
	buffer_size = stderr_ctx->buffer_size;
	src = stderr_ctx->screen_copy_buffer;
	dst = stderr_ctx->stderr_buffer + data_length;
	// LAZY CODE, SHOULD REALLOCATE BUFFER WHEN THERE'S MORE DATA THAN BUFFER CAN HOLD
	cursor_index = 0;
	while (cursor_index < char_read && data_length < buffer_size) {
        if (*src) {
            *dst++ = *src++;
            cursor_index++;
        } else {
            *dst++ = '\n';
            skip = STDERR_SCREEN_WIDTH - (cursor_index % STDERR_SCREEN_WIDTH);
            src += skip;
            cursor_index += skip;
        }
        data_length++;
	}
	*dst = '\0';
	stderr_ctx->data_length = data_length;

	FillConsoleOutputCharacter(stderr_ctx->screen_handle, '\0', char_read, (COORD){0, 0}, &dwDummy);

	return TRUE;
}


BOOL create_pipes(struct process_ctx *process_ctx, struct child_pipe_ctx *child_pipe_ctx)
{
	child_pipe_ctx->read_stdin = INVALID_HANDLE_VALUE;
	child_pipe_ctx->write_stdout = INVALID_HANDLE_VALUE;

	if (!create_sync_pipe(&(child_pipe_ctx->read_stdin), &(process_ctx->write_stdin))) {
		return FALSE;
	}
	if (!SetHandleInformation(child_pipe_ctx->read_stdin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
		return FALSE;
	}
	if (!create_async_read_pipe(&(process_ctx->read_stdout), &(child_pipe_ctx->write_stdout))) {
		return FALSE;
	}
	if (!SetHandleInformation(child_pipe_ctx->write_stdout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
		return FALSE;
	}

	return TRUE;
}


BOOL spawn_suspended_process(struct process_ctx *process_ctx, char *command, char *argument)
{
	STARTUPINFO siStartInfo = { 0 };
	BOOL bSuccess = FALSE;
	DWORD error = 0;
	struct child_pipe_ctx child_pipe_ctx;
	char cmd[BUF_SIZE] = {0};
	int length;

	length = strlen(command);
	strncpy(cmd, command, BUF_SIZE);

	if ((length > 4) && !strcmpi(".bat", command + length - 4)) {
		/*
		https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
		lpApplicationName
		To run a batch file, you must start the command interpreter; set lpApplicationName to cmd.exe
		and set lpCommandLine to the following arguments: /c plus the name of the batch file.

		The string can specify the full path and file name of the module to execute
		or it can specify a partial name. In the case of a partial name, the
		function uses the current drive and current directory to complete the specification.
		The function will not use the search path.
		This parameter must include the file name extension; no default extension is assumed.
		*/
		// NEVER GOT IT TO WORK THAT WAY, SO JUST PREPEND THE CMD COMMAND
		snprintf(cmd, BUF_SIZE - 1, "CMD.EXE /Q /C %s", command);
        length = strlen(cmd);
	}

	if (argument) {
		if ((length + 1 + strlen(argument)) >= BUF_SIZE) {
			return FALSE;
		}
		cmd[length] = ' ';
		strncpy(cmd + length + 1, argument, BUF_SIZE - length - 1);
	}

	if (!create_pipes(process_ctx, &child_pipe_ctx)) {
		CloseHandle(child_pipe_ctx.read_stdin);
		CloseHandle(child_pipe_ctx.write_stdout);
		CloseHandle(process_ctx->write_stdin);
		CloseHandle(process_ctx->read_stdout);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
		return FALSE;
	}
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdInput = child_pipe_ctx.read_stdin;
	siStartInfo.hStdOutput = child_pipe_ctx.write_stdout;
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	if (process_ctx->stderr_ctx) {
        siStartInfo.hStdError = process_ctx->stderr_ctx->screen_handle;
	}
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	// Start the new process in suspended state
	// Don't use CREATE_NO_WINDOW, child's stderr handle of file type FILE_TYPE_CHAR
	// always seems to need an console handle as well (intherited from parent)
	bSuccess = CreateProcess(NULL,	// lpApplicationName, TODO ?: Should be cmd.exe when starting batchfiles
                                cmd,	// command line
                                NULL,	// process security attributes, NULL: handle cannot be intherited
                                NULL,	// primary thread security attributes, NULL: handle cannot be intherited
                                TRUE,	// inherit handles, MUST be set for stdin, stdout, stderr redirection
                                CREATE_SUSPENDED,  // creation flags
                                NULL,			// use parent's environment
                                NULL,			// use parent's current directory
                                &siStartInfo,	// STARTUPINFO pointer
                                &(process_ctx->process_info) // receives PROCESS_INFORMATION
				);
	error = GetLastError();
	// Child standard I/O handlers are the inherited pipes ends, close the local ones
	CloseHandle(child_pipe_ctx.read_stdin);
	CloseHandle(child_pipe_ctx.write_stdout);
	if (!bSuccess) {
		CloseHandle(process_ctx->write_stdin);
		CloseHandle(process_ctx->read_stdout);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
        process_ctx->exit_code = error;
        fprintf(stderr, "Could not start '%s' (error: %ld)\n", command, process_ctx->exit_code);

		return FALSE;
	}

	if (process_ctx->stderr_ctx) {
		// The screen buffer handle cannot be set to a non inheritable handle, use an alternative method
		// Duplicate handle as a non inheritable handle, close inheritable source handle
        DuplicateHandle(GetCurrentProcess(),
                        siStartInfo.hStdError, // Source handle
                        GetCurrentProcess(),
                        &process_ctx->stderr_ctx->screen_handle, // Target handle
                        0,
                        FALSE, // bInheritHandle
                        DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS);
	}

    process_ctx->state = PROCESS_ACTIVE;

	return TRUE;
}


struct buffer_ctx *create_buffer_ctx()
{
	struct buffer_ctx *buffer_ctx = NULL;
	if (!(buffer_ctx = calloc(1, sizeof(*buffer_ctx)))) {
		return NULL;
	}
	if (!(buffer_ctx->buffer = malloc(BUF_SIZE * sizeof(*(buffer_ctx->buffer))))) {
		free(buffer_ctx);
		return NULL;
	}
	buffer_ctx->read_status = ERROR_SUCCESS;
    buffer_ctx->stOverlapped.hEvent =
            CreateEvent(NULL,    // default security attribute, NOT inherited
                        TRUE,    // manual reset event
                        FALSE,   // initial state: nonsignaled
                        NULL);   // unnamed event object

	return buffer_ctx;
}


struct process_ctx *create_process_ctx()
{
	struct process_ctx *process_ctx = NULL;

	if (!(process_ctx = calloc(1, sizeof(*process_ctx)))) {
		return NULL;
	}
	if (!(process_ctx->buffer_ctx_in = create_buffer_ctx())) {
		free(process_ctx);
		return NULL;
	}
	process_ctx->write_stdin = INVALID_HANDLE_VALUE;
	process_ctx->read_stdout = INVALID_HANDLE_VALUE;
	process_ctx->process_info.hProcess = INVALID_HANDLE_VALUE;
	process_ctx->process_info.hThread = INVALID_HANDLE_VALUE;
	process_ctx->state = PROCESS_INIT;

	return process_ctx;
}


void free_buffer_ctx(struct buffer_ctx **buffer_ctx)
{
	if (!buffer_ctx || !*buffer_ctx) {
		return;
	}
	if ((*buffer_ctx)->buffer) {
		free((*buffer_ctx)->buffer);
	}
	free(*buffer_ctx);
	*buffer_ctx = NULL;
}


void free_process_ctx(struct process_ctx **process_ctx)
{
	if (!process_ctx || !*process_ctx) {
		return;
	}
	free_buffer_ctx(&(*process_ctx)->buffer_ctx_in);
	free(*process_ctx);
	*process_ctx = NULL;
}


BOOL create_stderr_ctx(struct process_ctx *process_ctx)
{
	// Windows sets stderr buffering to full buffering when redirected to a non interactive handle
	// Even if a wrapper program sets it to NON buffering, creating the external process,
	// sets the mode to full buffering again....
	//
	// For stderr non buffering redirection a file has to be FILE_TYPE_CHAR like a console screen buffer
	struct stderr_ctx *stderr_ctx = NULL;
	DWORD dwDummy;
	SECURITY_ATTRIBUTES sa;
    CONSOLE_CURSOR_INFO cursor_invisible = {1, FALSE};

	if (!(stderr_ctx = calloc(1, sizeof(*stderr_ctx)))) {
		return FALSE;
	}
	process_ctx->stderr_ctx = stderr_ctx;

	stderr_ctx->screen_handle = INVALID_HANDLE_VALUE;
	stderr_ctx->log_handle = INVALID_HANDLE_VALUE;
	stderr_ctx->buffer_size = STDERR_SCREEN_WIDTH * STDERR_SCREEN_HEIGHT;
	if (!(stderr_ctx->screen_copy_buffer =
			malloc(stderr_ctx->buffer_size * sizeof(*stderr_ctx->screen_copy_buffer)))) {
		return FALSE;
	}
	if (!(stderr_ctx->stderr_buffer =
			malloc(stderr_ctx->buffer_size * sizeof(*stderr_ctx->stderr_buffer)))) {
		return FALSE;
	}
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	stderr_ctx->screen_handle = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ | FILE_SHARE_WRITE,
									&sa, CONSOLE_TEXTMODE_BUFFER, NULL);
	if (stderr_ctx->screen_handle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	// Although the screen buffer will NEVER be set as an active screen buffer
	// The cursor will blink as an extra cursor on parent's console screen (No idea why)
	// So clear the cursor visible flag and hope for the best...
	SetConsoleCursorInfo(stderr_ctx->screen_handle, &cursor_invisible);
	SetConsoleScreenBufferSize(stderr_ctx->screen_handle, (COORD){STDERR_SCREEN_WIDTH, STDERR_SCREEN_HEIGHT});
	FillConsoleOutputCharacter(stderr_ctx->screen_handle, '\0', MAXLONG, (COORD){0, 0}, &dwDummy);
	
	return TRUE;
}


void free_stderr_ctx(struct stderr_ctx **stderr_ctx)
{
	if (!stderr_ctx || !*stderr_ctx) {
		return;
	}

	if ((*stderr_ctx)->screen_handle != INVALID_HANDLE_VALUE) {
		CloseHandle((*stderr_ctx)->screen_handle);
	}
	if ((*stderr_ctx)->log_handle != INVALID_HANDLE_VALUE) {
		CloseHandle((*stderr_ctx)->log_handle);
	}
	if ((*stderr_ctx)->screen_copy_buffer) {
		free((*stderr_ctx)->screen_copy_buffer);
	}
	if ((*stderr_ctx)->stderr_buffer) {
		free((*stderr_ctx)->stderr_buffer);
	}
	free(*stderr_ctx);
	*stderr_ctx = NULL;

	return;
}


BOOL create_write_file(char *logfile, LPHANDLE write_handle)
{
	HANDLE h;

	SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    h = CreateFile(logfile,
        GENERIC_WRITE, // dwDesiredAccess
        FILE_SHARE_WRITE | FILE_SHARE_READ, //dwShareMode
        &sa,
        CREATE_ALWAYS, // dwCreationDisposition
        FILE_ATTRIBUTE_NORMAL,
        NULL );

	*write_handle = h;
	if (h == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	return TRUE;
}


VOID terminate_process(struct process_ctx *process_ctx)
{
	//DWORD dwWait;

	if (!process_ctx) {
		return;
	}

	ResumeThread(process_ctx->process_info.hThread);
	CloseHandle(process_ctx->process_info.hThread);
	process_ctx->process_info.hThread = INVALID_HANDLE_VALUE;
	TerminateProcess(process_ctx->process_info.hProcess, 0);
	CloseHandle(process_ctx->process_info.hProcess);
	process_ctx->process_info.hProcess = INVALID_HANDLE_VALUE;

	if (process_ctx->write_stdin != INVALID_HANDLE_VALUE) {
		CloseHandle(process_ctx->write_stdin);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
	}
	if (process_ctx->read_stdout != INVALID_HANDLE_VALUE) {
		CloseHandle(process_ctx->read_stdout);
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
	}
	free_stderr_ctx(&process_ctx->stderr_ctx);

	process_ctx->state = PROCESS_STOPPED;

	return;
}

BOOL check_alive(struct process_ctx *process_ctx)
{
	DWORD dwRet;
	HANDLE process = process_ctx->process_info.hProcess;

	if (process == INVALID_HANDLE_VALUE) {
		process_ctx->exit_code = ERROR_FUNCTION_FAILED; // 1627 (0x65B) Function failed during execution.
		process_ctx->state = PROCESS_STOPPED;
		return FALSE;
	}
    // https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess
    // This function returns immediately.
    // If the process has not terminated and the function succeeds, the status returned is STILL_ACTIVE.
	if (!GetExitCodeProcess(process, &(process_ctx->exit_code))) {
		process_ctx->exit_code = ERROR_FUNCTION_FAILED;
		process_ctx->state = PROCESS_STOPPED;
		return FALSE;
	}
	dwRet = WaitForSingleObject(process, 0);
	if (dwRet != WAIT_TIMEOUT || process_ctx->exit_code != STILL_ACTIVE) {
		process_ctx->state = PROCESS_STOPPED;
		return FALSE;
	}
	process_ctx->state = PROCESS_ACTIVE;

	return TRUE;
}
