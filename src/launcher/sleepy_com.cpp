/*=====================================================================
sleepy_com.cpp
------------

Console entry point for Very Sleepy.

sleepy.exe is a GUI subsystem binary, so it has no console of its own and
anything it writes to stderr goes nowhere unless it is redirected. That makes
the command line interface awkward to use and impossible to see.

This is the usual answer on Windows: a console subsystem stub beside it,
carrying the same base name. cmd searches PATHEXT in order and .com comes
before .exe, so typing "sleepy" at a prompt runs this, which starts the real
program with our standard handles and a private option telling it that they are
intentional, then waits for it. Output and redirection behave the way they
would for any console program, and the exit code is passed back.

Copyright (C) Oleg Tolmatcev

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Everything after the program name, which is what we pass on unchanged. The
// program name itself is quoted if it contains spaces, so skip to the closing
// quote in that case rather than to the first space.
static const wchar_t *arguments(const wchar_t *command_line)
{
	const wchar_t *p = command_line;

	if (*p == L'"')
	{
		p++;
		while (*p && *p != L'"')
			p++;
		if (*p == L'"')
			p++;
	}
	else
	{
		while (*p && *p != L' ' && *p != L'\t')
			p++;
	}

	return p;
}

static void report(const wchar_t *what)
{
	fwprintf(stderr, L"sleepy: %ls failed (error %lu)\n", what, GetLastError());
}

int main(void)
{
	wchar_t path[MAX_PATH];
	DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);

	if (!length || length >= MAX_PATH)
	{
		report(L"GetModuleFileName");
		return 1;
	}

	// We are sleepy.com; the program we exist to start is sleepy.exe.
	if (length < 4 || _wcsicmp(path + length - 4, L".com") != 0)
	{
		fwprintf(stderr, L"sleepy: expected to be named .com, not %ls\n", path);
		return 1;
	}
	wcscpy(path + length - 4, L".exe");

	const wchar_t *args = arguments(GetCommandLineW());
	const wchar_t *console_option = L" --console-proxy";

	// CreateProcess wants a mutable command line, and the first token has to be
	// the program. Add a private option so sleepy.exe does not have to guess why
	// it received standard handles.
	size_t size = wcslen(path) + wcslen(console_option) + wcslen(args) + 4;
	wchar_t *command_line = (wchar_t *)malloc(size * sizeof(wchar_t));

	if (!command_line)
	{
		fwprintf(stderr, L"sleepy: out of memory\n");
		return 1;
	}
	_snwprintf(command_line, size, L"\"%ls\"%ls%ls", path, console_option, args);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	PROCESS_INFORMATION pi = {};

	if (!CreateProcessW(path, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
	{
		report(L"CreateProcess");
		free(command_line);
		return 1;
	}
	free(command_line);

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exit_code = 1;
	GetExitCodeProcess(pi.hProcess, &exit_code);

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return (int)exit_code;
}
