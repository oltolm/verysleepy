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
#include "../profiler/symbolinfo.h"
#include "../utils/except.h"
#include "../utils/osutils.h"
#include "processlist.h"
#include "threadlist.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <wx/itemattr.h>
#include <wx/listctrl.h>

BEGIN_EVENT_TABLE(ProcessList, wxListView)
EVT_LIST_ITEM_SELECTED(PROCESS_LIST, ProcessList::OnSelected)
EVT_LIST_COL_CLICK(wxID_ANY, ProcessList::OnSort)
EVT_TIMER(PROCESS_LIST_TIMER, ProcessList::OnTimer)
END_EVENT_TABLE()

ProcessList::ProcessList(wxWindow *parent, const wxPoint& pos,
						 const wxSize& size,
						ThreadList* threadList_)
						 : wxListView(parent, PROCESS_LIST, pos, size, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VIRTUAL),
						 timer(this, PROCESS_LIST_TIMER)
{
	threadList = threadList_;
	firstUpdate = true;

	syminfo.reset(new SymbolInfo());

	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT)/* | wxLIST_MASK_IMAGE*/;
	itemCol.SetText(_T("Process"));
	itemCol.SetImage(-1);
	InsertColumn(COL_NAME, itemCol);
#ifdef _WIN64
	itemCol.SetText(_T("Type"));
	InsertColumn(COL_TYPE, itemCol);
#endif
	itemCol.SetText(_T("CPU"));
	InsertColumn(COL_CPUUSAGE, itemCol);
	itemCol.SetText(_T("Total CPU"));
	InsertColumn(COL_TOTALCPU, itemCol);
	itemCol.SetText(_T("PID"));
	InsertColumn(COL_PID, itemCol);

	SetColumnWidth(COL_NAME, FromDIP(270));
#ifdef _WIN64
	SetColumnWidth(COL_TYPE, FromDIP(45));
#endif
	SetColumnWidth(COL_CPUUSAGE, FromDIP(50));
	SetColumnWidth(COL_TOTALCPU, FromDIP(70));
	SetColumnWidth(COL_PID, FromDIP(50));

	ShowSortIndicator(COL_CPUUSAGE, false);

	timer.Start(1000); // 1 second interval
}


void ProcessList::reloadSymbols(bool download)
{
	syminfo.reset(new SymbolInfo);

	//------------------------------------------------------------------------
	//load up the debug info for it
	//------------------------------------------------------------------------
	try
	{
		const ProcessInfo* process_info = getSelectedProcess();
		if (!process_info)
			throw SleepyException(L"No process selected");

		HANDLE process_handle = process_info->getProcessHandle();
		if (process_handle)
		{
			syminfo->loadSymbols(process_handle, download);
		}
	}
	catch (SleepyException &e)
	{
		::MessageBox(NULL, std::wstring(L"Error: " + e.wwhat()).c_str(), L"Profiler Error", MB_OK);
		syminfo = NULL;
	}

	updateThreadList();
}

void ProcessList::OnSelected(wxListEvent&)
{	
	if (!m_updating)
		reloadSymbols(false);
	m_updating = false;
}

const ProcessInfo* ProcessList::getSelectedProcess()
{
	if(GetFirstSelected() != wxNOT_FOUND)
	{
		return &processes.at(GetFirstSelected());
	}
	else
		return NULL;
}

std::unique_ptr<SymbolInfo> ProcessList::takeSymbolInfo()
{
	return std::move(syminfo);
}

void ProcessList::updateThreadList()
{
	if (syminfo && GetFirstSelected() != wxNOT_FOUND)
	{
		const auto processInfo = &processes.at(GetFirstSelected());
		threadList->updateThreads(processInfo, syminfo.get());
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
	Refresh();
}


struct ProcessInfoPred
{
	ProcessInfoPred(wxListView *listView)
		: m_listView(listView)
	{
	}

	bool operator()(const ProcessInfo& item1, const ProcessInfo& item2) const
	{
		auto sort_column = m_listView->GetSortIndicator();
		bool ascending = m_listView->IsAscendingSortIndicator();
		auto a = &item1;
		auto b = &item2;
		if (!ascending)
			std::swap(a, b);

		switch (sort_column)
		{
		case ProcessList::COL_NAME:
			return wcsicmp(a->getName().c_str(), b->getName().c_str()) < 0;
		case ProcessList::COL_CPUUSAGE:
			return a->cpuUsage < b->cpuUsage;
		case ProcessList::COL_TOTALCPU:
			return a->totalCpuTimeMs < b->totalCpuTimeMs;
		case ProcessList::COL_PID:
			return a->getID() < b->getID();
#ifdef _WIN64
		case ProcessList::COL_TYPE: {
			if (a->getIs64Bits() == b->getIs64Bits())
				return a->cpuUsage < b->cpuUsage;
			return a->getIs64Bits() < b->getIs64Bits();
		}
#endif
		case ProcessList::NUM_COLUMNS:
			break;
		}
		return 0;
	}

private:
	wxListView *m_listView;
};

void ProcessList::OnSort(wxListEvent& event)
{
	bool ascending;
	if (GetSortIndicator() == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	} else {
		// if switching columns, start with the default sort for that column type
		ascending = !(event.GetColumn() >= 1 && event.GetColumn() <= 4);
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	std::stable_sort(processes.begin(), processes.end(), ProcessInfoPred(this));
	Refresh();
}

wxString ProcessList::OnGetItemText(long item, long column) const
{
	const ProcessInfo *process = &processes.at(item);

	switch (column)
	{
	case COL_NAME:
		return process->getName();

	case COL_CPUUSAGE:
		return process->cpuUsage >= 0 ? wxString::Format("%i%%", process->cpuUsage) : "-";

	case COL_TOTALCPU:
		return process->totalCpuTimeMs >= 0
				   ? wxString::Format("%0.1f s", (double)process->totalCpuTimeMs / 1000)
				   : "-";

	case COL_PID:
		return wxString::Format("%li", process->getID());

#ifdef _WIN64
	case COL_TYPE:
		return Is64BitProcess(process->getProcessHandle()) ? "64-bit" : "32-bit";
#endif
	}
	return "";
}

void ProcessList::updateProcesses()
{
	DWORD selectedProcessId;
	const ProcessInfo* selectedProcess = getSelectedProcess();
	selectedProcessId = selectedProcess != nullptr ? selectedProcess->getID() : (DWORD)-1;

	for(auto & process : processes)
	{
		CloseHandle( process.getProcessHandle() );

		for(const auto & thread : process.threads)
			CloseHandle( thread.getThreadHandle() );
	}
	threadList->updateThreads(NULL, NULL);

	processes = ProcessInfo::enumProcesses();
	SetItemCount(processes.size());

	// Select the selected process last time the program was run..
	if(selectedProcessId == (DWORD)-1) {
		wxString prevProcess;
		config.Read("PrevProcess", &prevProcess, "");
		if(!prevProcess.IsEmpty()) {
			for(const auto & process : processes)
			{
				if(process.getName() == prevProcess)
				{
					selectedProcessId = process.getID();
					break;
				}
			}
		}
	}

	lastTime = wxGetLocalTimeMillis();

	// We need to wait a bit before we can get any useful CPU usage data to sort on.
	updateTimes();
	Sleep(200);
	updateTimes();
	std::stable_sort(processes.begin(), processes.end(), ProcessInfoPred(this));

	if(selectedProcessId != (DWORD)-1)
	{
		long i = 0;
		for (const auto& processInfo : processes)
		{
			if(processInfo.getID() == selectedProcessId) {
				SetFocus();
				EnsureVisible(i);
				Select(i);
				m_updating = true;
				break;
			}
			++i;
		}
	}
	updateThreadList();
	Refresh();
}

void ProcessList::updateTimes()
{
	wxLongLong now = wxGetLocalTimeMillis();
	int sampleTimeDiff = (now - lastTime).ToLong();
	lastTime = now;

	for(auto & process : processes)
	{
		process.recalcUsage(sampleTimeDiff);
	}
}
