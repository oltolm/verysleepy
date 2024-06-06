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

http://www.gnu.org/copyleft/gpl.html.
=====================================================================*/
#pragma once

#include "profilergui.h"
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
	ThreadList(wxWindow *parent, const wxPoint& pos,
			const wxSize& size, wxButton *ok_button, wxButton *all_button);

	virtual ~ThreadList() = default;

	void OnSelected(wxListEvent& event);
	void OnDeselected(wxListEvent& event);
	void OnTimer(wxTimerEvent& event);
	void OnSort(wxListEvent& event);

	void updateThreads(const ProcessInfo* processInfo, SymbolInfo *symInfo);

	std::vector<const ThreadInfo*> getSelectedThreads(bool all=false);
	wxString  OnGetItemText (long item, long column) const override;

	enum ColumnType {
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
	HANDLE process_handle;
	SymbolInfo *syminfo;
	wxButton *ok_button;
	wxButton *all_button;

	void fillList();
	int getNumDisplayedThreads();
	std::wstring getLocation(HANDLE thread_handle, DWORD thread_id);
	void updateTimes();

	DECLARE_EVENT_TABLE()
};


enum
{
	THREADS_LIST = 4000,
	THREADS_LIST_TIMER = 4001
};
