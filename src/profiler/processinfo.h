/*=====================================================================
processinfo.h
-------------
File created by ClassTemplate on Sun Mar 20 03:22:27 2005

Copyright (C) Nicholas Chapman

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
#pragma once

#include <windows.h>
#include "threadinfo.h"
#include <vector>

/*=====================================================================
ProcessInfo
-----------
Info about a process running on the system
=====================================================================*/
class ProcessInfo
{
public:
	/*=====================================================================
	ProcessInfo
	-----------

	=====================================================================*/
	ProcessInfo(DWORD id, const std::wstring& name);

	~ProcessInfo();

	static std::vector<ProcessInfo> enumProcesses();
	static ProcessInfo FindProcessById(DWORD process_id);

	std::vector<ThreadInfo> threads;

	const std::wstring& getName() const { return name; }
	DWORD getID() const { return id; }
	bool getIs64Bits() const { return is64Bits; }
	FILETIME prevKernelTime, prevUserTime;
	int cpuUsage;
	__int64 totalCpuTimeMs;

private:
	std::wstring name;
	DWORD id;
	bool is64Bits;
};
