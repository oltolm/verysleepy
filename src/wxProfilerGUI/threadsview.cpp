/*=====================================================================
threadsview.cpp
------------

Copyright (C) Very Sleepy authors and contributors

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
#include "threadsview.h"
#include "guiutils.h"
#include "mainwin.h"
#include <wx/types.h>

BEGIN_EVENT_TABLE(ThreadsView, wxListCtrl)
EVT_LIST_ITEM_SELECTED(THREADS_VIEW, ThreadsView::OnSelected)
EVT_LIST_ITEM_DESELECTED(THREADS_VIEW, ThreadsView::OnDeSelected)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadsView::OnSort)
EVT_TIMER(THREADS_VIEW_TIMER, ThreadsView::OnTimer)
END_EVENT_TABLE()

ThreadsView::ThreadsView(wxWindow *parent, Database *database_)
	: wxListView(parent, THREADS_VIEW),
	  selectionTimer(this, THREADS_VIEW_TIMER)
{
	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT);
	itemCol.SetImage(-1);
	itemCol.SetText(_T("TID"));
	InsertColumn(COL_TID, itemCol);
	itemCol.SetText(_T("Thread Name"));
	InsertColumn(COL_NAME, itemCol);

	SetColumnWidth(COL_TID, FromDIP(80));
	SetColumnWidth(COL_NAME, FromDIP(200));

	database = database_;
	ShowSortIndicator(COL_TID, true);
}

ThreadsView::~ThreadsView()
{
}

void ThreadsView::OnSelected(wxListEvent &WXUNUSED(event))
{
	startSelectionTimer();
}

void ThreadsView::OnDeSelected(wxListEvent &WXUNUSED(event))
{
	startSelectionTimer();
}

void ThreadsView::startSelectionTimer()
{
	// user interactions typically generate a series of selection change events
	// so we only update the main window 100ms after selection events have stopped coming
	selectionTimer.Stop();
	selectionTimer.StartOnce(100);
}

void ThreadsView::OnTimer(wxTimerEvent &WXUNUSED(event))
{
	theMainWin->refreshSelectedThreads();
}

static int ThreadComparator(wxIntPtr item1, wxIntPtr item2, wxIntPtr data)
{
	auto sort_column = ((ThreadsView *)data)->GetSortIndicator();
	bool ascending = ((ThreadsView *)data)->IsAscendingSortIndicator();
	auto a = (ThreadsView::ThreadRow *)item1;
	auto b = (ThreadsView::ThreadRow *)item2;
	if (!ascending)
		std::swap(a, b);

	switch (sort_column)
	{
	case ThreadsView::COL_TID:
		return a->tid < b->tid ? -1 : a->tid > b->tid ? 1 : 0;
	case ThreadsView::COL_NAME:
		return wcsicmp(a->name.c_str(), b->name.c_str());
	case ThreadsView::MAX_COLUMNS:
		break;
	}
	return 0;
}

void ThreadsView::OnSort(wxListEvent& event)
{
	bool ascending;
	if (GetSortIndicator() == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	}
	else
	{
		// if switching columns, start with the default sort for that column type
		ascending = true;
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	SortItems(ThreadComparator, (wxIntPtr)this);
}

void ThreadsView::updateList()
{
	getThreadsFromDatabase();
	fillList();
	SortItems(ThreadComparator, (wxIntPtr)this);
}

std::vector<Database::ThreadID> ThreadsView::getSelectedThreads()
{
	std::vector<Database::ThreadID> selected;
	for (long i = GetFirstSelected(); i != wxNOT_FOUND; i = GetNextSelected(i))
	{
		auto thread = (ThreadRow *)GetItemData(i);
		selected.push_back(thread->tid);
	}
	return selected;
}

void ThreadsView::clearSelectedThreads()
{
	std::vector<Database::ThreadID> selected;
	for (long i = GetFirstSelected(); i != wxNOT_FOUND; i = GetNextSelected(i))
	{
		Select(i, false);
	}
}

void ThreadsView::focusThread(Database::ThreadID tid)
{
	auto it = std::find_if(threads.begin(), threads.end(), [=](ThreadRow &r) { return r.tid == tid; });
	if (it == threads.end())
		return;
	long i = FindItem(-1, (wxUIntPtr) & *it);
	if (i != wxNOT_FOUND)
	{
		Focus(i);

		for (int j = 0; j < GetItemCount(); ++j)
		{
			SetItemTextColour(j, i == j ? lightOrDark(wxTheColourDatabase->Find("green"))
										: GetTextColour());
		}
	}
}

void ThreadsView::getThreadsFromDatabase()
{
	threads.clear();
	for (auto &tn : database->getThreadNames())
	{
		ThreadRow row;
		row.tid = tn.first;
		row.name = tn.second;
		threads.push_back(row);
	}
}

void ThreadsView::fillList()
{
	Freeze();
	DeleteAllItems();

	for (int i = 0; i < (int)threads.size(); ++i)
	{
		InsertItem(i, "", -1);
		SetItemPtrData(i, (wxUIntPtr)&threads[i]);

		wxString tid = wxString::Format("%d", threads[i].tid);
		SetItem(i, COL_TID, tid);

		SetItem(i, COL_NAME, threads[i].name);
	}

	Thaw();
}


BEGIN_EVENT_TABLE(ThreadSamplesView, wxListCtrl)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadSamplesView::OnSort)
EVT_LIST_ITEM_ACTIVATED(THREAD_SAMPLES_VIEW, ThreadSamplesView::OnActivated)
END_EVENT_TABLE()

ThreadSamplesView::ThreadSamplesView(wxWindow *parent, Database *database_)
	: wxListView(parent, THREAD_SAMPLES_VIEW)
{
	wxListItem itemCol;
	itemCol.SetMask(wxLIST_MASK_TEXT);
	itemCol.SetImage(-1);
	itemCol.SetText(_T("TID"));
	InsertColumn(COL_TID, itemCol);
	itemCol.SetText(_T("Thread Name"));
	InsertColumn(COL_NAME, itemCol);
	itemCol.SetText(_T("Exclusive"));
	InsertColumn(COL_EXCLUSIVE, itemCol);
	itemCol.SetText(_T("Inclusive"));
	InsertColumn(COL_INCLUSIVE, itemCol);
	itemCol.SetText(_T("% Exclusive"));
	InsertColumn(COL_EXCLUSIVEPCT, itemCol);
	itemCol.SetText(_T("% Inclusive"));
	InsertColumn(COL_INCLUSIVEPCT, itemCol);

	SetColumnWidth(COL_TID, FromDIP(80));
	SetColumnWidth(COL_NAME, FromDIP(200));
	SetColumnWidth(COL_EXCLUSIVE, FromDIP(70));
	SetColumnWidth(COL_INCLUSIVE, FromDIP(70));
	SetColumnWidth(COL_EXCLUSIVEPCT, FromDIP(70));
	SetColumnWidth(COL_INCLUSIVEPCT, FromDIP(70));

	database = database_;
	ShowSortIndicator(COL_EXCLUSIVE, false);
}

ThreadSamplesView::~ThreadSamplesView()
{
}

static int ThreadSampleComparator(wxIntPtr item1, wxIntPtr item2, wxIntPtr data)
{
	auto sort_column = ((ThreadSamplesView *)data)->GetSortIndicator();
	auto ascending = ((ThreadSamplesView *)data)->IsAscendingSortIndicator();
	auto a = (ThreadSamplesView::ThreadRow *)item1;
	auto b = (ThreadSamplesView::ThreadRow *)item2;
	if (!ascending)
		std::swap(a, b);

	switch (sort_column)
	{
	case ThreadSamplesView::COL_TID:
		return a->tid < b->tid ? -1 : a->tid > b->tid ? 1 : 0;
	case ThreadSamplesView::COL_NAME:
		return wcsicmp(a->name.c_str(), b->name.c_str());
	case ThreadSamplesView::COL_EXCLUSIVE:
	case ThreadSamplesView::COL_EXCLUSIVEPCT:
		if (a->exclusive != b->exclusive)
			return a->exclusive < b->exclusive ? -1 : a->exclusive > b->exclusive ? 1 : 0;
		return a->inclusive < b->inclusive ? -1 : a->inclusive > b->inclusive ? 1 : 0;
	case ThreadSamplesView::COL_INCLUSIVE:
	case ThreadSamplesView::COL_INCLUSIVEPCT:
		if (a->inclusive != b->inclusive)
			return a->inclusive < b->inclusive ? -1 : a->inclusive > b->inclusive ? 1 : 0;
		return a->exclusive < b->exclusive ? -1 : a->exclusive > b->exclusive ? 1 : 0;
	case ThreadSamplesView::MAX_COLUMNS:
		break;
	}
	return 0;
}

void ThreadSamplesView::OnSort(wxListEvent &event)
{
	bool ascending;
	if (GetSortIndicator() == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	} else {
		// if switching columns, start with the default sort for that column type
		ascending = true;
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	SortItems(ThreadSampleComparator, (wxIntPtr)this);
}

void ThreadSamplesView::OnActivated(wxListEvent &event)
{
	Database::ThreadID tid = ((ThreadRow *)event.GetData())->tid;
	theMainWin->focusThread(tid);
}

void ThreadSamplesView::showList(Database::SymbolSamples const &symbolSamples)
{
	totalCount = symbolSamples.totalcount;

	threads.clear();

	// here we assume inclusive samples are a superset of exclusive samples
	for (auto &incSample : symbolSamples.inclusive)
	{
		ThreadRow row;

		row.tid = incSample.first;
		row.name = database->getThreadNames().at(row.tid);
		row.inclusive = incSample.second;

		auto exSample = symbolSamples.exclusive.find(row.tid);
		if (exSample != symbolSamples.exclusive.end())
			row.exclusive = exSample->second;
		else
			row.exclusive = 0;

		threads.push_back(row);
	}

	fillList();
	SortItems(ThreadSampleComparator, (wxIntPtr)this);
}

void ThreadSamplesView::reset()
{
	DeleteAllItems();
	totalCount = 0;
	threads.clear();
}

void ThreadSamplesView::fillList()
{
	Freeze();
	DeleteAllItems();

	for (int i = 0; i < (int)threads.size(); ++i)
	{
		InsertItem(i, "", -1);
		SetItemPtrData(i, (wxUIntPtr)&threads[i]);

		wxString tid = wxString::Format("%d", threads[i].tid);
		SetItem(i, COL_TID, tid);

		SetItem(i, COL_NAME, threads[i].name);

		wxString inclusive = wxString::Format("%0.2fs", threads[i].inclusive);
		wxString exclusive = wxString::Format("%0.2fs", threads[i].exclusive);
		wxString inclusivepercent = wxString::Format("%0.2f%%", threads[i].inclusive * 100.0f / totalCount);
		wxString exclusivepercent = wxString::Format("%0.2f%%", threads[i].exclusive * 100.0f / totalCount);

		SetItem(i, COL_EXCLUSIVE, exclusive);
		SetItem(i, COL_INCLUSIVE, inclusive);
		SetItem(i, COL_EXCLUSIVEPCT, exclusivepercent);
		SetItem(i, COL_INCLUSIVEPCT, inclusivepercent);
	}

	Thaw();
}
