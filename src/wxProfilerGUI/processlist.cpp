/*=====================================================================
processlist.cpp
---------------
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
#include "processlist.h"
#include "profilergui.h"
#include "threadlist.h"
#include "../profiler/symbolinfo.h"
#include "../utils/osutils.h"
#include <algorithm>
#include <memory>
#include "../utils/except.h"

BEGIN_EVENT_TABLE(ProcessList, wxListCtrl)
EVT_LIST_ITEM_SELECTED(PROCESS_LIST, ProcessList::OnSelected)
EVT_LIST_COL_CLICK(wxID_ANY, ProcessList::OnSort)
EVT_TIMER(PROCESS_LIST_TIMER, ProcessList::OnTimer)
END_EVENT_TABLE()

ProcessList::ProcessList(wxWindow *parent, ThreadList *threadList_)
	: wxListView(parent, PROCESS_LIST, wxDefaultPosition, wxDefaultSize,
				 wxLC_REPORT | wxLC_SINGLE_SEL),
	  timer(this, PROCESS_LIST_TIMER)
{
	threadList = threadList_;
	selectionChanged = false;
	firstUpdate = true;

	syminfo = std::make_unique<SymbolInfo>();

	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT /* | wxLIST_MASK_IMAGE*/);
	itemCol.SetText(_T("Process"));
	itemCol.SetImage(-1);
	InsertColumn(COL_NAME, itemCol);
	itemCol.SetText(_T("Type"));
	InsertColumn(COL_TYPE, itemCol);
	itemCol.SetText(_T("CPU"));
	InsertColumn(COL_CPUUSAGE, itemCol);
	itemCol.SetText(_T("Total CPU"));
	InsertColumn(COL_TOTALCPU, itemCol);
	itemCol.SetText(_T("PID"));
	InsertColumn(COL_PID, itemCol);

	SetColumnWidth(COL_NAME, FromDIP(270));
	SetColumnWidth(COL_TYPE, FromDIP(45));
	SetColumnWidth(COL_CPUUSAGE, FromDIP(50));
	SetColumnWidth(COL_TOTALCPU, FromDIP(70));
	SetColumnWidth(COL_PID, FromDIP(50));

	ShowSortIndicator(COL_CPUUSAGE, false);

	timer.Start(1000); // 1 second interval
}

ProcessList::~ProcessList() {}

void ProcessList::reloadSymbols(bool download)
{
	syminfo = std::make_unique<SymbolInfo>();

	//------------------------------------------------------------------------
	//load up the debug info for it
	//------------------------------------------------------------------------
	try
	{
		const ProcessInfo *process_info = getSelectedProcessInfo();
		if (!process_info)
			throw SleepyException(L"No process selected");

		if (process_info->getID())
		{
			syminfo->loadSymbols(process_info->getID(), download);
		}
	}
	catch (SleepyException &e)
	{
		::MessageBox(NULL, std::wstring(L"Error: " + e.wwhat()).c_str(), L"Profiler Error", MB_OK);
		syminfo = nullptr;
	}

	updateThreadList();
}

void ProcessList::OnSelected(wxListEvent& event)
{
	if (this->selected_process != event.GetIndex())
	{
		this->selected_process = event.GetIndex();
		selectionChanged = true;

		reloadSymbols(false);
	}
}

DWORD ProcessList::getSelectedProcessId()
{
	auto pi = getSelectedProcessInfo();
	return pi == nullptr ? 0 : pi->getID();
}

const ProcessInfo *ProcessList::getSelectedProcessInfo()
{
	if (GetFirstSelected() != wxNOT_FOUND)
	{
		return (ProcessInfo *)GetItemData(GetFirstSelected());
	}
	else
		return NULL;
}

std::unique_ptr<SymbolInfo> ProcessList::takeSymbolInfo()
{
	return std::move(syminfo);
}

static __int64 getDiff(FILETIME before, FILETIME after)
{
	__int64 i0 = ((__int64)(before.dwHighDateTime) << 32) + before.dwLowDateTime;
	__int64 i1 = ((__int64)( after.dwHighDateTime) << 32) +  after.dwLowDateTime;
	return i1 - i0;
}

static __int64 getTotal(FILETIME time)
{
	return ((__int64)(time.dwHighDateTime) << 32) + time.dwLowDateTime;
}

void ProcessList::updateThreadList()
{
	if (syminfo && getSelectedProcessInfo())
	{
		threadList->updateThreads(getSelectedProcessInfo(), syminfo.get());
	} else {
		threadList->updateThreads(NULL, NULL);
	}
}

void ProcessList::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	if (firstUpdate)
	{
		firstUpdate = false;

		updateProcesses();
	}

	updateTimes();
	if(selectionChanged){
		selectionChanged = false;
		updateThreadList();
	}
}

static int ProcessComparator(wxIntPtr item1, wxIntPtr item2, wxIntPtr data)
{
	auto sort_column = ((wxListView *)data)->GetSortIndicator();
	bool ascending = ((wxListView *)data)->IsAscendingSortIndicator();
	auto a = (ProcessInfo *)item1;
	auto b = (ProcessInfo *)item2;
	if (!ascending)
		std::swap(a, b);

	switch (sort_column)
	{
	case ProcessList::COL_NAME:
		return wcsicmp(a->getName().c_str(), b->getName().c_str());
	case ProcessList::COL_TYPE: {
		if (a->getIs64Bits() == b->getIs64Bits())
			return a->cpuUsage < b->cpuUsage ? -1 : a->cpuUsage > b->cpuUsage ? 1 : 0;
		return a->getIs64Bits() < b->getIs64Bits()	 ? -1
			   : a->getIs64Bits() > b->getIs64Bits() ? 1
													 : 0;
	}
	case ProcessList::COL_CPUUSAGE:
		return a->cpuUsage < b->cpuUsage ? -1 : a->cpuUsage > b->cpuUsage ? 1 : 0;
	case ProcessList::COL_TOTALCPU:
		return a->totalCpuTimeMs < b->totalCpuTimeMs   ? -1
			   : a->totalCpuTimeMs > b->totalCpuTimeMs ? 1
													   : 0;
	case ProcessList::COL_PID:
		return a->getID() < b->getID() ? -1 : a->getID() > b->getID() ? 1 : 0;
	case ProcessList::NUM_COLUMNS:
		break;
	}
	return 0;
}

void ProcessList::OnSort(wxListEvent& event)
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
	SortItems(ProcessComparator, (wxIntPtr)this);
}

void ProcessList::fillList()
{
	Freeze();
	for (int i = 0; i < GetItemCount(); ++i)
	{
		ProcessInfo *process = (ProcessInfo *)GetItemData(i);

		this->SetItem(i, COL_NAME, process->getName());

		wxString str;
		if (process->cpuUsage >= 0)
			str = wxString::Format("%i%%", process->cpuUsage);
		else
			str = "-";
		this->SetItem(i, COL_CPUUSAGE, str);

		if (process->totalCpuTimeMs >= 0)
			str = wxString::Format("%0.1f s", (double)(process->totalCpuTimeMs) / 1000);
		else
			str = "-";
		this->SetItem(i, COL_TOTALCPU, str);

		str = wxString::Format("%li", process->getID());
		this->SetItem(i, COL_PID, str);
		if (Is64BitProcess(process->getID()))
		{
			SetItem(i,COL_TYPE,"64-bit");
		}
		else
		{
			SetItem(i,COL_TYPE,"32-bit");
		}
	}
	Thaw();
}

void ProcessList::updateProcesses()
{
	const ProcessInfo *selectedProcess = this->getSelectedProcessInfo();
	DeleteAllItems();

	processes.clear();
	threadList->updateThreads(NULL, NULL);

	this->processes = ProcessInfo::enumProcesses();
	for(int i=0; i<(int)processes.size(); ++i)
	{
		this->InsertItem(i, "", -1);
		SetItemPtrData(i, (wxUIntPtr)&processes[i]);
	}

	lastTime = wxGetLocalTimeMillis();

	// We need to wait a bit before we can get any useful CPU usage data to sort on.
	updateTimes();
	Sleep(200);
	updateTimes();

	fillList();
	SortItems(ProcessComparator, (wxIntPtr)this);

	// Select the selected process last time the program was run..
	if (!selectedProcess)
	{
		wxString prevProcess;
		config.Read("PrevProcess", &prevProcess, "");
		if (!prevProcess.IsEmpty())
		{
			for (int i = 0; i < GetItemCount(); ++i)
			{
				auto *process = (ProcessInfo *)GetItemData(i);
				if (process->getName() == prevProcess)
				{
					this->SetFocus();
					this->EnsureVisible(i);
					this->Select(i);
					break;
				}
			}
		}
	}
}

void ProcessList::updateTimes()
{
	wxLongLong now = wxGetLocalTimeMillis();
	int sampleTimeDiff = (now - lastTime).ToLong();
	lastTime = now;

	for (int i = 0; i < GetItemCount(); ++i)
	{
		ProcessInfo *process = (ProcessInfo *)GetItemData(i);
		process->cpuUsage = -1;
		process->totalCpuTimeMs = -1;

		__int64 coreCount = 0;

		DWORD pid = process->getID();
		handle_ptr process_handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
		if (!process_handle)
			continue;

		FILETIME CreationTime, ExitTime, KernelTime, UserTime;
		BOOL result =
			GetProcessTimes(process_handle.get(), &CreationTime, &ExitTime, &KernelTime, &UserTime);
		if (!result)
			continue;

		coreCount = GetCoresForProcess(process_handle.get());

		__int64 kernel_diff = getDiff(process->prevKernelTime, KernelTime);
		__int64 user_diff = getDiff(process->prevUserTime, UserTime);
		process->prevKernelTime = KernelTime;
		process->prevUserTime = UserTime;

		if (sampleTimeDiff > 0)
			process->cpuUsage =
				(((kernel_diff + user_diff) / 10000) * 100 / sampleTimeDiff) / coreCount;

		process->totalCpuTimeMs = (getTotal(KernelTime) + getTotal(UserTime)) / 10000;
	}

	fillList();
}
