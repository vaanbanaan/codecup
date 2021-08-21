#ifndef PIPE_EX_H
#define PIPE_EX_H

BOOL
APIENTRY
MyCreatePipeEx(
    OUT LPHANDLE lpReadPipe,
    OUT LPHANDLE lpWritePipe,
    IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
    IN DWORD nSize,
    DWORD dwReadMode,
    DWORD dwWriteMode
    );

extern ULONG PipeSerialNumber;

#endif // PIPE_EX_H
