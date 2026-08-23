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

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/
#include "processinfo.h"

#include "../utils/osutils.h"
#include "../utils/except.h"
#include <windows.h>
#include <tlhelp32.h>
#include <wx/string.h>

ProcessInfo::ProcessInfo(DWORD id_, const std::wstring& name_)
	: name(name_),
	  id(id_)
{
	prevKernelTime.dwHighDateTime = prevKernelTime.dwLowDateTime = 0;
	prevUserTime.dwHighDateTime = prevUserTime.dwLowDateTime = 0;
	cpuUsage = -1;
	totalCpuTimeMs = -1;
	is64Bits = Is64BitProcess(id);
}

ProcessInfo::~ProcessInfo()
{
}

std::vector<ProcessInfo> ProcessInfo::enumProcesses()
{
	std::vector<ProcessInfo> processes_out;
	handle_ptr snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0));

	PROCESSENTRY32 processinfo = {0};
	processinfo.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot.get(), &processinfo))
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
			handle_ptr process_handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));

			// Skip processes we don't have permission to.
			if (process_handle == NULL)
			{
				continue;
			}

			if (!CanProfileProcess(process_handle.get()))
			{
				continue;
			}

			const std::wstring processname = processinfo.szExeFile;
			processes_out.push_back(ProcessInfo(process_id, processname));

			processinfo.dwSize = sizeof(PROCESSENTRY32);
		} while (Process32Next(snapshot.get(), &processinfo));
	}

	//------------------------------------------------------------------------
	//process threads
	//------------------------------------------------------------------------
	THREADENTRY32 threadinfo;
	threadinfo.dwSize = sizeof(THREADENTRY32);

	if (Thread32First(snapshot.get(), &threadinfo))
	{
		do
		{
			const DWORD owner_process_id = threadinfo.th32OwnerProcessID;

			for(unsigned int i=0; i<processes_out.size(); ++i)
			{
				if(processes_out[i].getID() == owner_process_id)
				{
					processes_out[i].threads.push_back(ThreadInfo(threadinfo.th32ThreadID));
					break;
				}
			}

			threadinfo.dwSize = sizeof(THREADENTRY32);
		} while (Thread32Next(snapshot.get(), &threadinfo));
	}

	return processes_out;
}

ProcessInfo ProcessInfo::FindProcessById(DWORD process_id)
{
	std::vector<ProcessInfo> allProcesses = enumProcesses();
	for (const auto& process : allProcesses)
	{
		if (process.getID() == process_id)
			return process;
	}
	throw SleepyException(
		wxString::Format("Could not found process with specified id: %lu", process_id));
}
