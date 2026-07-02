/*=====================================================================
threadlist.h
-------------
File created by ClassTemplate on Sun Mar 20 17:33:43 2005

Copyright (C) Dan Engelbrecht

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

#include "../profiler/threadinfo.h"

#include <wx/listctrl.h>
#include <wx/timer.h>

class ProcessInfo;
class SymbolInfo;

/*=====================================================================
ThreadsList
-----------

=====================================================================*/
class ThreadList : public wxListView
{
public:
	/*=====================================================================
	ThreadList
	-----------

	=====================================================================*/
	ThreadList(wxWindow *parent, wxButton *ok_button, wxButton *all_button);

	virtual ~ThreadList();

	void OnSelected(wxListEvent& event);
	void OnDeSelected(wxListEvent& event);
	void OnTimer(wxTimerEvent& event);
	void OnSort(wxListEvent& event);

	void updateThreads(const ProcessInfo* processInfo, SymbolInfo *symInfo);
	void updateTimes();

	std::vector<const ThreadInfo*> getSelectedThreads(bool all=false);

	enum {
		COL_LOCATION,
		COL_CPUUSAGE,
		COL_TOTALCPU,
		COL_ID,
		COL_NAME,
		NUM_COLUMNS
	};

private:
	std::vector<ThreadInfo> threads;
	wxTimer timer;
	wxLongLong lastTime;
	DWORD pid;
	SymbolInfo *syminfo;
	wxButton *ok_button;
	wxButton *all_button;

	void fillList();
	int getNumDisplayedThreads();
	std::wstring getLocation(DWORD thread_id);

	DECLARE_EVENT_TABLE()
};


enum
{
	THREADS_LIST = 4000,
	THREADS_LIST_TIMER = 4001
};
