#include <stdio.h>
#include "io.h"
#include <string.h>
#include <sys/time.h>
#include "pipe_ex.h"
#include "prflush.h"

// Windows Error codes: https://docs.microsoft.com/en-us/windows/win32/debug/system-error-codes

// temp child pipe-handles ends, parent MUST close them if child process is created
struct child_pipe_ctx {
	HANDLE read_stdin,
		   write_stdout,
		   write_stderr;
};

BOOL create_sync_pipe(LPHANDLE read_handle, LPHANDLE write_handle);
BOOL create_async_read_pipe(LPHANDLE read_handle, LPHANDLE write_handle);
BOOL create_write_file(char *filename, LPHANDLE write_handle);
struct buffer_ctx *create_buffer_ctx();


VOID WINAPI async_read_callback(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap);


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
			0 // write side synchronous
			)) {
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
	/*
	 * Resumes a process (external program) for a given time to receive a message, then suspends
	 * If that process also writes to its stderr, waiting can be interrupted by an asynchronous completion routine
	 * The completion routine writes the stderr pipe data to a logfile (or discarded if no log)
	 */
	struct buffer_ctx *buffer_ctx = process_ctx->buffer_ctx_in;

	DWORD dwWait = WAIT_IO_COMPLETION;
	unsigned int wait_time = wait_msec;
	struct timeval tv_start, tv_response;
	BOOL bSuccess;

	buffer_ctx->data_length = 0;
	process_ctx->response_time_msec = 0;

	if (process_ctx->stderr_async_ctx) {
		process_ctx->stderr_async_ctx->stderr_line = FALSE; // No async stderr messages (yet)
	}
	if (buffer_ctx->read_status == ERROR_SUCCESS) {
		// ReadFile can read remaining pipe data, even if the attached process is suspended
		bSuccess = ReadFile(process_ctx->read_stdout, buffer_ctx->buffer, BUF_SIZE, &buffer_ctx->data_length, &buffer_ctx->stOverlapped);
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
		while (TRUE) {
			dwWait = WaitForSingleObjectEx(buffer_ctx->stOverlapped.hEvent, wait_time, TRUE);
			// GetOverlappedResultEx is not supported on Windows 7 (EOL, but I'm still using it), use an alternative
			if (dwWait != WAIT_IO_COMPLETION) {
				break;
			}
			// Wait was interrupted by async stderr alert, adjust wait time and try again
			gettimeofday(&tv_response, NULL);
			wait_time = wait_msec - ((tv_response.tv_sec - tv_start.tv_sec) * 1000000 +
														tv_response.tv_usec - tv_start.tv_usec) / 1000;
		}
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

	if (process_ctx->stderr_async_ctx) {
		if (process_ctx->stderr_async_ctx->error) {
			process_ctx->exit_code = ERROR_FUNCTION_FAILED;

			return FALSE; // Async stderr read -> write logfile error
		}
		if (process_ctx->stderr_async_ctx->stderr_line) {
			debug("\n"); // TODO check for already existing \n
		}
	}

	if (buffer_ctx->read_status != ERROR_SUCCESS) {
        return FALSE;
	}

    buffer_ctx->buffer[buffer_ctx->data_length] = '\0';

	return TRUE;
}


BOOL get_firsterror(struct process_ctx *process_ctx, unsigned int wait_msec, unsigned long *length)
{
    struct stderr_async_ctx *stderr_async_ctx = process_ctx->stderr_async_ctx;
	BOOL bSuccess = TRUE;
	DWORD dwWait;
	*length = 0;

	stderr_async_ctx->stOverlapped.hEvent =
            CreateEvent(NULL,    // default security attribute, NOT inherited
                        TRUE,    // manual reset event
                        FALSE,   // initial state: nonsignaled
                        NULL);   // unnamed event object

	if (stderr_async_ctx->stOverlapped.hEvent == INVALID_HANDLE_VALUE) {
        return FALSE;
	}
	if (ResumeThread(process_ctx->process_info.hThread) != 1) {
		return FALSE;
	}
	ReadFile(process_ctx->read_stderr, stderr_async_ctx->pipe_buffer, PIPE_SIZE, NULL, &stderr_async_ctx->stOverlapped);
	dwWait = WaitForSingleObject(stderr_async_ctx->stOverlapped.hEvent, wait_msec);
	SuspendThread(process_ctx->process_info.hThread);
	CancelIo(stderr_async_ctx->stOverlapped.hEvent);
	if (dwWait == WAIT_OBJECT_0) {
		ResetEvent(stderr_async_ctx->stOverlapped.hEvent);
		bSuccess = GetOverlappedResult(process_ctx->read_stderr,
										&stderr_async_ctx->stOverlapped,
										length,
										FALSE);
	}

    CloseHandle(stderr_async_ctx->stOverlapped.hEvent);
    stderr_async_ctx->stOverlapped.hEvent = INVALID_HANDLE_VALUE;
	// getoverlappedresult can fail with ERROR_MORE_DATA
	// buffer should be big enough, client probably sent too much data
	if (!bSuccess || PIPE_SIZE <= *length) {
		return FALSE;
	}
	process_ctx->stderr_async_ctx->pipe_buffer[*length] = '\0';

	return TRUE;
}


BOOL create_pipes(struct process_ctx *process_ctx, struct child_pipe_ctx *child_pipe_ctx, BOOL stderr_redir)
{
	child_pipe_ctx->read_stdin = INVALID_HANDLE_VALUE;
	child_pipe_ctx->write_stdout = INVALID_HANDLE_VALUE;
	child_pipe_ctx->write_stderr = INVALID_HANDLE_VALUE;

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
	if (stderr_redir) {
		if (!create_async_read_pipe(&(process_ctx->read_stderr), &(child_pipe_ctx->write_stderr))) {
			return FALSE;
		}
		if (!SetHandleInformation(child_pipe_ctx->write_stderr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
			return FALSE;
		}
	}
	return TRUE;
}


BOOL spawn_suspended_process(struct process_ctx *process_ctx, char *command, char *argument, char stderr_redir)
{
	STARTUPINFO siStartInfo;
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

	if (!create_pipes(process_ctx, &child_pipe_ctx, stderr_redir)) {
		CloseHandle(child_pipe_ctx.read_stdin);
		CloseHandle(child_pipe_ctx.write_stdout);
		CloseHandle(child_pipe_ctx.write_stderr);
		CloseHandle(process_ctx->write_stdin);
		CloseHandle(process_ctx->read_stdout);
		CloseHandle(process_ctx->read_stderr);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
		process_ctx->read_stderr = INVALID_HANDLE_VALUE;
		return FALSE;
	}

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdInput = child_pipe_ctx.read_stdin;
	siStartInfo.hStdOutput = child_pipe_ctx.write_stdout;
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	if (stderr_redir) {
		siStartInfo.hStdError = child_pipe_ctx.write_stderr;
	}
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	bSuccess = CreateProcess(NULL, // lpApplicationName, TODO: Used For starting batchfiles
							  cmd,     	// command line
							  NULL,          	// process security attributes
							  NULL,          	// primary thread security attributes
							  TRUE, // inherit handles, MUST be set for stdin, stdout, stderr redirection
                              CREATE_SUSPENDED, // creation flags CREATE_SUSPENDED
							  NULL,          	// use parent's environment
							  NULL,          	// use parent's current directory
							  &siStartInfo,  	// STARTUPINFO pointer
							  &(process_ctx->process_info)); // receives PROCESS_INFORMATION

	// Child is connected to the child pipes ends, close the local ones
	error = GetLastError();
	CloseHandle(child_pipe_ctx.read_stdin);
	CloseHandle(child_pipe_ctx.write_stdout);
	if (stderr_redir) {
		CloseHandle(child_pipe_ctx.write_stderr);
	}
	if (!bSuccess) {
		CloseHandle(process_ctx->write_stdin);
		CloseHandle(process_ctx->read_stdout);
		CloseHandle(process_ctx->read_stderr);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
		process_ctx->read_stderr = INVALID_HANDLE_VALUE;
        process_ctx->exit_code = error;
        fprintf(stderr, "Could not start '%s' (error: %ld)\n", command, process_ctx->exit_code);

		return FALSE;
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
	process_ctx->read_stderr = INVALID_HANDLE_VALUE;
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


BOOL create_stderr_async_ctx(struct process_ctx *process_ctx, char stderr_id)
{
	// Asynchronous stderr pipe read end, if logfile != NULL: writes contents te file
	struct stderr_async_ctx *stderr_async_ctx = NULL;

	if (!(stderr_async_ctx = calloc(1, sizeof(*stderr_async_ctx)))) {
		return FALSE;
	}
	if (!(stderr_async_ctx->pipe_buffer = malloc(PIPE_SIZE * sizeof(TCHAR)))) {
		free(stderr_async_ctx);
		return FALSE;
	}
	stderr_async_ctx->read_stderr = process_ctx->read_stderr;
	stderr_async_ctx->stderr_id = stderr_id;
	stderr_async_ctx->logfile = INVALID_HANDLE_VALUE;
	process_ctx->stderr_async_ctx = stderr_async_ctx;

	return TRUE;
}


BOOL activate_async_read_callback(struct stderr_async_ctx *stderr_async_ctx)
{
	BOOL bReadSuccess;

	bReadSuccess = ReadFileEx(stderr_async_ctx->read_stderr,
						stderr_async_ctx->pipe_buffer,
						PIPE_SIZE,
						&(stderr_async_ctx->stOverlapped), // == lpOverLap
						(LPOVERLAPPED_COMPLETION_ROUTINE) async_read_callback);

	return bReadSuccess;
}


VOID WINAPI async_read_callback(DWORD dwErr, DWORD cbBytesRead, LPOVERLAPPED lpOverLap)
{
	struct stderr_async_ctx *stderr_async_ctx = (struct stderr_async_ctx*)lpOverLap; // struct trick, see io.h
	BOOL bReadSuccess = FALSE,
		 bWriteSuccess = TRUE;
	DWORD cbRead, dwWritten;

	if (stderr_async_ctx->error == TRUE) {
		return;
	}
	if (stderr_async_ctx->logfile != INVALID_HANDLE_VALUE) {
		bWriteSuccess = FALSE;
	}
	if (!dwErr && cbBytesRead) {
		do {
			bReadSuccess = GetOverlappedResult(stderr_async_ctx->read_stderr,
											&(stderr_async_ctx->stOverlapped), // == lpOverLap
											&cbRead,
											FALSE);
			if (!bReadSuccess && GetLastError() != ERROR_MORE_DATA) {
				break; // Read error
			}
			stderr_async_ctx->pipe_buffer[cbRead] = '\0';
			if (!stderr_async_ctx->stderr_line) {
				debug("%d (log): %s", stderr_async_ctx->stderr_id, stderr_async_ctx->pipe_buffer);
				stderr_async_ctx->stderr_line = TRUE;
			} else {
				debug("%s", stderr_async_ctx->pipe_buffer);
			}
			if (stderr_async_ctx->logfile != INVALID_HANDLE_VALUE) {
				bWriteSuccess = WriteFile(stderr_async_ctx->logfile,
								  stderr_async_ctx->pipe_buffer,
								  cbRead,
								  &dwWritten,
								  NULL);
			}
			if (!bReadSuccess) { // ERROR_MORE_DATA, so read more
				ReadFile(stderr_async_ctx->read_stderr,
						 stderr_async_ctx->pipe_buffer,
						 PIPE_SIZE,
						 &cbRead,
						 &(stderr_async_ctx->stOverlapped));
			}
		} while (!bReadSuccess && bWriteSuccess);
	}

	if (bReadSuccess && bWriteSuccess) {
		bReadSuccess = activate_async_read_callback(stderr_async_ctx);
	}
	if (!bReadSuccess || !bWriteSuccess) {
		stderr_async_ctx->error = TRUE;
	}

	return;
}


BOOL create_write_file(char *logfile, LPHANDLE write_handle)
{
	HANDLE h;

	SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

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


VOID stop_stderr_async_ctx(struct stderr_async_ctx *stderr_async_ctx)
{
	if (!stderr_async_ctx) {
        return;
	}

	if (stderr_async_ctx->logfile != INVALID_HANDLE_VALUE) {
        stderr_async_ctx->error = TRUE;
        CloseHandle(stderr_async_ctx->logfile);
		stderr_async_ctx->logfile = INVALID_HANDLE_VALUE;
	}
	return;
}


void free_stderr_async_ctx(struct stderr_async_ctx **stderr_async_ctx)
{
	if (!stderr_async_ctx || !*stderr_async_ctx) {
		return;
	}
	if ((*stderr_async_ctx)->pipe_buffer) {
		free((*stderr_async_ctx)->pipe_buffer);
	}
	free(*stderr_async_ctx);
	*stderr_async_ctx = NULL;

	return;
}


VOID terminate_process(struct process_ctx *process_ctx)
{
	DWORD dwWait;

	if (!process_ctx) {
		return;
	}
	// give process some time to write to stderr (player programs only)
	if (process_ctx->stderr_async_ctx && check_alive(process_ctx)) {
		ResumeThread(process_ctx->process_info.hThread);
		while (TRUE) {
            dwWait = WaitForSingleObjectEx(process_ctx->stderr_async_ctx->stOverlapped.hEvent, 50, TRUE);
			if (dwWait != WAIT_IO_COMPLETION) {
				break;
			}
		}
		SuspendThread(process_ctx->process_info.hThread);
        ResetEvent(process_ctx->stderr_async_ctx->stOverlapped.hEvent);
	}
	if (process_ctx->write_stdin != INVALID_HANDLE_VALUE) {
		CloseHandle(process_ctx->write_stdin);
		process_ctx->write_stdin = INVALID_HANDLE_VALUE;
	}
	if (process_ctx->read_stdout != INVALID_HANDLE_VALUE) {
		CloseHandle(process_ctx->read_stdout);
		process_ctx->read_stdout = INVALID_HANDLE_VALUE;
	}
	if (process_ctx->read_stderr != INVALID_HANDLE_VALUE) {
		CloseHandle(process_ctx->read_stderr);
		process_ctx->read_stderr = INVALID_HANDLE_VALUE;
	}
	if (process_ctx->stderr_async_ctx) {
		stop_stderr_async_ctx(process_ctx->stderr_async_ctx);
		WaitForSingleObjectEx(process_ctx->stderr_async_ctx->stOverlapped.hEvent, 0, TRUE);
        free_stderr_async_ctx(&process_ctx->stderr_async_ctx);
	}

	if (process_ctx->process_info.hThread != INVALID_HANDLE_VALUE ) {
		CloseHandle(process_ctx->process_info.hThread);
		process_ctx->process_info.hThread = INVALID_HANDLE_VALUE;
		TerminateProcess(process_ctx->process_info.hProcess, 0);
		CloseHandle(process_ctx->process_info.hProcess);
		process_ctx->process_info.hProcess = INVALID_HANDLE_VALUE;
		process_ctx->state = PROCESS_STOPPED;
	}

	return;
}

char check_alive(struct process_ctx *process_ctx)
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


void reset_console_mode()
{
    // WIP, not implemented yet
    // Somehow win-caiaio + Cygwin frankenstein referee / players configuration
    // ends with wrong console mode (command console only)
	HANDLE hstdout;
	DWORD fdwMode;

	hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hstdout == INVALID_HANDLE_VALUE) {
		return;
	}
    if (!GetConsoleMode(hstdout, &fdwMode)) {
		return;
	}

    fdwMode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hstdout, fdwMode);

	return;
}
