/*=====================================================================
processlist.h
-------------
File created by ClassTemplate on Sun Mar 20 17:33:43 2005

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

http://www.gnu.org/copyleft/gpl.html.
=====================================================================*/
#pragma once

#include "profilergui.h"
#include "../profiler/processinfo.h"

#include <memory>
#include <wx/listctrl.h>
#include <wx/timer.h>

// DE: 20090325 ProcessList knows about threadlist and updates it based on process selection
class ThreadList;
class SymbolInfo;

/*=====================================================================
ProcessList
-----------

=====================================================================*/
class ProcessList : public wxListView
{
public:
	/*=====================================================================
	ProcessList
	-----------

	=====================================================================*/
	// DE: 20090325 ProcessList knows about threadlist and updates it based on process selection
	ProcessList(wxWindow *parent, const wxPoint& pos,
			const wxSize& size,
			ThreadList* threadList);

	virtual ~ProcessList() = default;

	void OnSelected(wxListEvent& event);
	void OnTimer(wxTimerEvent& event);
	void OnSort(wxListEvent& event);

	void updateProcesses();
	void reloadSymbols(bool download);

	const ProcessInfo* getSelectedProcess();
	std::unique_ptr<SymbolInfo> takeSymbolInfo();

	enum ColumnType {
		COL_NAME,
#ifdef _WIN64
		COL_TYPE,
#endif
		COL_CPUUSAGE,
		COL_TOTALCPU,
		COL_PID,
		NUM_COLUMNS
	};
	wxString  OnGetItemText (long item, long column) const override;

private:
	std::vector<ProcessInfo> processes;
	// DE: 20090325 ProcessList knows about threadlist and updates it based on process selection
	ThreadList* threadList;
	std::unique_ptr<SymbolInfo> syminfo;

	wxTimer timer;
	wxLongLong lastTime;
	bool firstUpdate;
	bool m_updating = false;

	void updateThreadList();
	void updateTimes();

	DECLARE_EVENT_TABLE()
};


enum
{
	PROCESS_LIST = 4000,
	PROCESS_LIST_TIMER = 4001
};
