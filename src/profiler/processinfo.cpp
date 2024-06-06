/*=====================================================================
processinfo.cpp
---------------
File created by ClassTemplate on Sun Mar 20 03:22:27 2005

Copyright (C) Nicholas Chapman
Copyright (C) 2015 Ashod Nakashian

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

http://www.gnu.org/copyleft/gpl.html..
=====================================================================*/
#include "processinfo.h"

#include "../utils/osutils.h"
#include "../utils/except.h"

#include <cstddef>
#include <tlhelp32.h>
#include <windows.h>

ProcessInfo::ProcessInfo(DWORD id_, const std::wstring& name_, HANDLE process_handle_)
:	name(name_),
	id(id_),
	process_handle(process_handle_)
{
	prevKernelTime.dwHighDateTime = prevKernelTime.dwLowDateTime = 0;
	prevUserTime.dwHighDateTime = prevUserTime.dwLowDateTime = 0;
	cpuUsage = -1;
#ifdef _WIN64
	is64Bits = Is64BitProcess(process_handle);
#endif
}

static __int64 difference(FILETIME before, FILETIME after)
{
	__int64 i0 = ((__int64)(before.dwHighDateTime) << 32) + before.dwLowDateTime;
	__int64 i1 = ((__int64)( after.dwHighDateTime) << 32) +  after.dwLowDateTime;
	return i1 - i0;
}

static __int64 total(FILETIME time)
{
	return ((__int64)(time.dwHighDateTime) << 32) + time.dwLowDateTime;
}

bool ProcessInfo::recalcUsage(int sampleTimeDiff)
{
	cpuUsage = -1;
	totalCpuTimeMs = -1;

	HANDLE process_handle = getProcessHandle();
	if (process_handle == NULL)
		return false;

	FILETIME CreationTime, ExitTime, KernelTime, UserTime;

	if(!GetProcessTimes(
		process_handle,
		&CreationTime,
		&ExitTime,
		&KernelTime,
		&UserTime
	))
		return false;

	__int64 coreCount = GetCoresForProcess(process_handle);

	__int64 kernel_diff = difference(prevKernelTime, KernelTime);
	__int64 user_diff = difference(prevUserTime, UserTime);
	prevKernelTime = KernelTime;
	prevUserTime = UserTime;

	if (sampleTimeDiff > 0) {
		__int64 total_diff = ((kernel_diff + user_diff) / 10'000) * 100;
		cpuUsage = (total_diff / sampleTimeDiff) / coreCount;
	}

	totalCpuTimeMs = (total(KernelTime) + total(UserTime)) / 10'000;
	
	return true;
}

std::vector<ProcessInfo> ProcessInfo::enumProcesses()
{
	std::vector<ProcessInfo> processes;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);

	PROCESSENTRY32 processinfo = {0};
	processinfo.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &processinfo))
	{
		do
		{
			const DWORD process_id = processinfo.th32ProcessID;

			// Don't allow profiling our own process. Bad things happen.
			if (process_id == GetCurrentProcessId())
			{
				continue;
			}

			//------------------------------------------------------------------------
			//Get the actual handle of the process
			//------------------------------------------------------------------------
			const HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);

			// Skip processes we don't have permission to.
			if (process_handle == NULL)
			{
				continue;
			}

			if (!CanProfileProcess(process_handle))
			{
				CloseHandle(process_handle);
				continue;
			}

			const std::wstring processname = processinfo.szExeFile;
			processes.emplace_back(process_id, processname, process_handle);

			processinfo.dwSize = sizeof(PROCESSENTRY32);
		}
		while(Process32Next(snapshot, &processinfo));
	}

	//------------------------------------------------------------------------
	//process threads
	//------------------------------------------------------------------------
	THREADENTRY32 threadinfo;
	threadinfo.dwSize = sizeof(THREADENTRY32);

	if(Thread32First(snapshot, &threadinfo))
	{
		do
		{
			const DWORD owner_process_id = threadinfo.th32OwnerProcessID;

			for(auto & process : processes)
			{
				if(process.getID() == owner_process_id)
				{
					DWORD threadID = threadinfo.th32ThreadID;
					HANDLE threadHandle = OpenThread( THREAD_ALL_ACCESS, FALSE, threadID );

					process.threads.emplace_back(threadID, threadHandle);
					break;
				}
			}

			threadinfo.dwSize = sizeof(THREADENTRY32);
		}
		while(Thread32Next(snapshot, &threadinfo));
	}

	CloseHandle(snapshot);
	return processes;
}

ProcessInfo ProcessInfo::FindProcessById(DWORD process_id)
{
	std::vector<ProcessInfo> processes = enumProcesses();
	for (const auto& process : processes)
	{
			if(process.getID() == process_id)
				return process;
	}
	throw SleepyException("Could not find process with specified id: " + std::to_string(process_id));
}
