#pragma once

#include <windows.h>

typedef BOOL WINAPI Wow64GetThreadContext_t(HANDLE hThread, PWOW64_CONTEXT lpContext);
typedef DWORD WINAPI Wow64SuspendThread_t(HANDLE hThread);

extern Wow64GetThreadContext_t *fn_Wow64GetThreadContext;
extern Wow64SuspendThread_t *fn_Wow64SuspendThread;
