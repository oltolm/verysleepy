/*=====================================================================
threadList.cpp
---------------
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
#include "../profiler/processinfo.h"
#include "../profiler/profiler.h"
#include "../profiler/symbolinfo.h"
#include "database.h"
#include "threadlist.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <wx/button.h>
#include <wx/listbase.h>
#include <wx/listctrl.h>

#define UPDATE_DELAY 1000	 // 1 second interval
#define MAX_NUM_THREAD_LOCATIONS 100 // getting location of thread is very expensive, so only show it for the first X threads in the list
#define MAX_NUM_DISPLAYED_THREADS 1000 // creating very large tables is expensive, limit number of threads to first X in selected sort order

BEGIN_EVENT_TABLE(ThreadList, wxListView)
EVT_LIST_ITEM_SELECTED(THREADS_LIST, ThreadList::OnSelected)
EVT_LIST_ITEM_DESELECTED(THREADS_LIST, ThreadList::OnDeselected)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadList::OnSort)
EVT_TIMER(THREADS_LIST_TIMER, ThreadList::OnTimer)
END_EVENT_TABLE()

ThreadList::ThreadList(wxWindow *parent, const wxPoint& pos,
						 const wxSize& size, wxButton *_ok_button, wxButton *_all_button)
						 : wxListView(parent, THREADS_LIST, pos, size, wxLC_REPORT | wxLC_VIRTUAL),
						 timer(this, THREADS_LIST_TIMER),
						 ok_button(_ok_button), all_button(_all_button)
{
	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT)/* | wxLIST_MASK_IMAGE*/;
	itemCol.SetImage(-1);
	itemCol.SetText(_T("Location"));
	InsertColumn(COL_LOCATION, itemCol);
	itemCol.SetText(_T("CPU"));
	InsertColumn(COL_CPUUSAGE, itemCol);
	itemCol.SetText(_T("Total CPU"));
	InsertColumn(COL_TOTALCPU, itemCol);
	itemCol.SetText(_T("TID"));
	InsertColumn(COL_ID, itemCol);
	itemCol.SetText(_T("Thread Name"));
	InsertColumn(COL_NAME, itemCol);

	SetColumnWidth(COL_LOCATION, FromDIP(270));
	SetColumnWidth(COL_CPUUSAGE, FromDIP(50));
	SetColumnWidth(COL_TOTALCPU, FromDIP(70));
	SetColumnWidth(COL_ID, FromDIP(50));

	// We hide the thread name column if running it on an OS that doesn't
	// support the API, to avoid wasting screen space.
	if (hasThreadDescriptionAPI())
		SetColumnWidth(COL_NAME, FromDIP(150));
	else
		SetColumnWidth(COL_NAME, 0);

	ShowSortIndicator(COL_CPUUSAGE, false);

	process_handle = NULL;
	syminfo = NULL;

	lastTime = wxGetLocalTimeMillis();
	updateThreads(NULL, NULL);
	timer.Start(UPDATE_DELAY);
}


void ThreadList::OnSelected(wxListEvent&)
{
	ok_button->Enable(true);
}

void ThreadList::OnDeselected(wxListEvent&)
{
	if(GetSelectedItemCount() != wxNOT_FOUND)
		ok_button->Enable(false);
}

std::vector<const ThreadInfo*> ThreadList::getSelectedThreads(bool all)
{
	std::vector<const ThreadInfo*> selectedThreads;
	if(all) {
		selectedThreads.reserve(threads.size());
		for(const auto & thread : threads) {
			selectedThreads.push_back(&thread);
		}
	} else {
		for(long item = GetFirstSelected(); item != wxNOT_FOUND; item = GetNextSelected(item))
		{
			selectedThreads.push_back(&threads.at(item));
		}
	}
	return selectedThreads;
}

void ThreadList::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	updateTimes();
}

struct ThreadInfoPred
{
	ThreadInfoPred(wxListView *listView)
		: m_listView(listView)
	{
	}

	bool operator()(const ThreadInfo& item1, const ThreadInfo& item2) const
	{
		auto sort_column = m_listView->GetSortIndicator();
		auto ascending = m_listView->IsAscendingSortIndicator();
		auto a = &item1;
		auto b = &item2;
		if (!ascending)
			std::swap(a, b);

		switch (sort_column)
		{
		case ThreadList::COL_LOCATION:
			return a->getLocation() < b->getLocation();
		case ThreadList::COL_CPUUSAGE:
			return a->cpuUsage < b->cpuUsage;
		case ThreadList::COL_TOTALCPU:
			return a->totalCpuTimeMs < b->totalCpuTimeMs;
		case ThreadList::COL_ID:
			return a->getID() < b->getID();
		case ThreadList::COL_NAME:
			return a->getName() < b->getName();
		case ThreadList::NUM_COLUMNS:
			break;
		}
		return false;
	}

private:
	wxListView *m_listView;
};

void ThreadList::OnSort(wxListEvent& event)
{
	bool ascending;
	if (GetSortIndicator() == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	} else {
		// if switching columns, start with the default sort for that column type
		ascending = event.GetColumn() < 1 || event.GetColumn() > 4;
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	std::stable_sort(threads.begin(), threads.end(), ThreadInfoPred(this));
}

int ThreadList::getNumDisplayedThreads() {
	int numThreads = threads.size();
	return std::min(numThreads, MAX_NUM_DISPLAYED_THREADS);
}

wxString  ThreadList::OnGetItemText (long item, long column) const
{
	const ThreadInfo* thread = &threads.at(item);
	switch(column)
	{
		case COL_LOCATION: return thread->getLocation();

		case COL_CPUUSAGE:
		return thread->cpuUsage >= 0 ? wxString::Format("%i%%", thread->cpuUsage): "-";

		case COL_TOTALCPU:
		return thread->totalCpuTimeMs >= 0 ? wxString::Format("%0.1f s", (double) (thread->totalCpuTimeMs) / 1000) : "-";

		case COL_ID:
		return wxString::Format("%ld", thread->getID());

		case COL_NAME:
		return thread->getName();
	}
	return "";
}

void ThreadList::updateThreads(const ProcessInfo* processInfo, SymbolInfo *symInfo)
{
	threads.clear();
	SetItemCount(0);
	ok_button->Enable(false);
	all_button->Enable(false);

	if(processInfo != NULL)
	{
		process_handle = processInfo->getProcessHandle();
		syminfo = symInfo;

		threads = processInfo->threads;

		all_button->Enable(!threads.empty());

		lastTime = wxGetLocalTimeMillis();
		SetItemCount(threads.size());
		updateTimes();
	}
}

void ThreadList::updateTimes()
{
	wxLongLong now = wxGetLocalTimeMillis();
	int sampleTimeDiff = (now - lastTime).ToLong();
	lastTime = now;

	for(size_t i=0; i<threads.size(); ++i)
	{
		if (!threads[i].recalcUsage(sampleTimeDiff))
			continue;

		HANDLE thread_handle = threads[i].getThreadHandle();
		if (thread_handle == NULL)
			continue;

		DWORD thread_id = threads[i].getID();

		threads[i].setLocation(L"-");
		if (i < MAX_NUM_THREAD_LOCATIONS) {
			std::wstring loc = getLocation(thread_handle, thread_id);
			threads[i].setLocation(loc);
		}
	}
	Refresh();
}

std::wstring ThreadList::getLocation(HANDLE thread_handle, DWORD thread_id) {
	PROFILER_ADDR profaddr = 0;
	try {
		std::map<CallStack, SAMPLE_TYPE> callstacks;
		Profiler profiler(process_handle, thread_handle, thread_id, callstacks);
		bool ok = profiler.sampleTarget(0, syminfo);
		if (ok && !profiler.targetExited() && !callstacks.empty())
		{
			const CallStack &stack = callstacks.begin()->first;
			profaddr = stack.addr[0];

			// Collapse functions down
			if (syminfo && stack.depth > 0)
			{
				for (size_t n=0;n<stack.depth;n++)
				{
					PROFILER_ADDR addr = stack.addr[n];
					std::wstring mod = syminfo->getModuleNameForAddr(addr);
					if (IsOsModule(mod))
					{
						profaddr = addr;
					} else {
						break;
					}
				}

				for (int n=(int)stack.depth-1;n>=0;n--)
				{
					PROFILER_ADDR addr = stack.addr[n];
					ProcedureInfo proc = syminfo->getProcForAddr(addr);
					if (IsOsFunction(proc.name))
					{
						profaddr = addr;
						break;
					}
				}
			}
		}
	} catch( ProfilerExcep &)
	{
	}

	if (profaddr && syminfo)
	{
		// Grab the name of the current IP location.
		return syminfo->getProcForAddr(profaddr).name;
	}

	return L"-";
}
