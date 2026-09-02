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

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/
#include "threadlist.h"
#include "database.h"
#include "persistentlistctrl.h"
#include "../profiler/processinfo.h"
#include "../profiler/profiler.h"
#include "../profiler/symbolinfo.h"
#include <algorithm>
#include <wx/button.h>
#include <wx/log.h>

#define UPDATE_DELAY 1000	 // 1 second interval
#define MAX_NUM_THREAD_LOCATIONS 100 // getting location of thread is very expensive, so only show it for the first X threads in the list
#define MAX_NUM_DISPLAYED_THREADS 1000 // creating very large tables is expensive, limit number of threads to first X in selected sort order

BEGIN_EVENT_TABLE(ThreadList, wxListCtrl)
EVT_LIST_ITEM_SELECTED(THREADS_LIST, ThreadList::OnSelected)
EVT_LIST_ITEM_DESELECTED(THREADS_LIST, ThreadList::OnDeSelected)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadList::OnSort)
EVT_TIMER(THREADS_LIST_TIMER, ThreadList::OnTimer)
END_EVENT_TABLE()

ThreadList::ThreadList(wxWindow *parent, wxButton *_ok_button, wxButton *_all_button)
	: wxListView(parent, THREADS_LIST),
	  timer(this, THREADS_LIST_TIMER),
	  ok_button(_ok_button),
	  all_button(_all_button)
{
	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT /* | wxLIST_MASK_IMAGE*/);
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

	RegisterListCtrlPersistence(this, "ThreadPickerList");
	ShowSortIndicator(COL_CPUUSAGE, false);

	syminfo = NULL;

	lastTime = wxGetLocalTimeMillis();
	updateThreads(NULL, NULL);
	timer.Start(UPDATE_DELAY);
}

ThreadList::~ThreadList()
{

}

void ThreadList::OnSelected(wxListEvent&)
{
	ok_button->Enable(true);
}

void ThreadList::OnDeSelected(wxListEvent&)
{
	if (GetFirstSelected() == wxNOT_FOUND)
		ok_button->Enable(false);
}

std::vector<const ThreadInfo*> ThreadList::getSelectedThreads(bool all)
{
	std::vector<const ThreadInfo*> selectedThreads;
	if(all) {
		selectedThreads.reserve(this->threads.size());
		for (auto& thread : threads)
		{
			selectedThreads.push_back(&thread);
		}
	}
	else
	{
		for (auto i = GetFirstSelected(); i != wxNOT_FOUND; i = GetNextSelected(i))
			selectedThreads.push_back((ThreadInfo *)GetItemData(i));
	}
	return selectedThreads;
}

static int ThreadComparator(wxIntPtr item1, wxIntPtr item2, wxIntPtr data)
{
	auto sort_column = ((ThreadList *)data)->GetSortIndicator();
	auto ascending = ((ThreadList *)data)->IsAscendingSortIndicator();
	auto a = (ThreadInfo *)item1;
	auto b = (ThreadInfo *)item2;
	if (!ascending)
		std::swap(a, b);

	switch (sort_column)
	{
	case ThreadList::COL_LOCATION:
		return a->getLocation().compare(b->getLocation());
	case ThreadList::COL_CPUUSAGE:
		return a->cpuUsage < b->cpuUsage ? -1 : a->cpuUsage > b->cpuUsage ? 1 : 0;
	case ThreadList::COL_TOTALCPU:
		return a->totalCpuTimeMs < b->totalCpuTimeMs   ? -1
			   : a->totalCpuTimeMs > b->totalCpuTimeMs ? 1
													   : 0;
	case ThreadList::COL_ID:
		return a->getID() < b->getID() ? -1 : a->getID() > b->getID() ? 1 : 0;
	case ThreadList::COL_NAME:
		return a->getName().compare(b->getName());
	case ThreadList::NUM_COLUMNS:
		break;
	}
	return 0;
}

void ThreadList::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	updateTimes();
	SortItems(ThreadComparator, (wxIntPtr)this);
}

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
	SortItems(ThreadComparator, wxIntPtr(this));
}

int ThreadList::getNumDisplayedThreads() {
	return std::min<int>(threads.size(), MAX_NUM_DISPLAYED_THREADS);
}

void ThreadList::fillList()
{
	Freeze();

	for (int i = 0; i < GetItemCount(); ++i)
	{
		ThreadInfo *thread = (ThreadInfo *)GetItemData(i);
		this->SetItem(i, COL_LOCATION, thread->getLocation());

		wxString str;
		if (thread->cpuUsage >= 0)
			str = wxString::Format("%i%%", thread->cpuUsage);
		else
			str = "-";
		this->SetItem(i, COL_CPUUSAGE, str);

		if (thread->totalCpuTimeMs >= 0)
			str = wxString::Format("%0.1f s", (double)(thread->totalCpuTimeMs) / 1000);
		else
			str = "-";
		this->SetItem(i, COL_TOTALCPU, str);

		str = wxString::Format("%ld", thread->getID());
		this->SetItem(i, COL_ID, str);

		this->SetItem(i, COL_NAME, thread->getName());
	}
	Thaw();
}

void ThreadList::updateThreads(const ProcessInfo* processInfo, SymbolInfo *symInfo)
{
	DeleteAllItems();
	this->threads.clear();
	ok_button->Enable(false);
	all_button->Enable(false);

	if(processInfo != NULL)
	{
		this->pid = processInfo->getID();
		this->syminfo = symInfo;

		this->threads = processInfo->threads;

		int numDisplayedThreads = getNumDisplayedThreads();
		for(int i=0; i<numDisplayedThreads; ++i)
		{
			this->InsertItem(i, "", -1);
			SetItemPtrData(i, (wxUIntPtr)&threads[i]);
		}

		all_button->Enable(GetItemCount() != 0);

		lastTime = wxGetLocalTimeMillis();
		updateTimes();
		SortItems(ThreadComparator, (wxIntPtr)this);
		fillList();
	}
}

void ThreadList::updateTimes()
{
	wxLongLong now = wxGetLocalTimeMillis();
	int sampleTimeDiff = (now - lastTime).ToLong();
	lastTime = now;

	for (int i = 0; i < GetItemCount(); ++i)
	{
		ThreadInfo *thread = (ThreadInfo *)GetItemData(i);
		if (!thread->recalcUsage(sampleTimeDiff))
			continue;

		DWORD thread_id = thread->getID();

		thread->setLocation(L"-");
		if (i < MAX_NUM_THREAD_LOCATIONS) {
			std::wstring loc = getLocation(thread_id);
			thread->setLocation(loc);
		}
	}

	fillList();
}

std::wstring ThreadList::getLocation(DWORD thread_id)
{
	PROFILER_ADDR profaddr = 0;
	try {
		std::map<CallStack, SAMPLE_TYPE> callstacks;
		Profiler profiler(pid, thread_id, callstacks);
		bool ok = profiler.sampleTarget(0, syminfo);
		if (ok && !profiler.targetExited() && callstacks.size() > 0)
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
					std::wstring file;
					int line;

					PROFILER_ADDR addr = stack.addr[n];
					std::wstring loc = syminfo->getProcForAddr(addr, file, line);
					if (IsOsFunction(loc))
					{
						profaddr = addr;
						break;
					}
				}
			}
		}
	} catch (ProfilerExcep &e)
	{
		// sampleTarget only throws when it could not resume the thread it suspended,
		// which leaves the target frozen for good. That is worth saying, but this runs
		// on a one second timer, so say it once rather than queueing a dialog per tick.
		static bool reported = false;
		if (!reported)
		{
			reported = true;
			wxLogWarning(L"Thread %lu is left suspended: %s", thread_id, e.what().c_str());
		}
	}

	if (profaddr && syminfo)
	{
		std::wstring file;
		int line;

		// Grab the name of the current IP location.
		return syminfo->getProcForAddr(profaddr, file, line);
	}

	return L"-";
}
