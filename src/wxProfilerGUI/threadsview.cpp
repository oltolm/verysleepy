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
#include "mainwin.h"
#include "threadsview.h"

#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <utility>
#include <wx/itemattr.h>
#include <wx/listctrl.h>

BEGIN_EVENT_TABLE(ThreadsView, wxListView)
EVT_LIST_ITEM_SELECTED(THREADS_VIEW, ThreadsView::OnSelected)
EVT_LIST_ITEM_DESELECTED(THREADS_VIEW, ThreadsView::OnDeSelected)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadsView::OnSort)
EVT_TIMER(THREADS_VIEW_TIMER, ThreadsView::OnTimer)
END_EVENT_TABLE()


ThreadsView::ThreadsView(wxWindow *parent, Database *database_)
	: wxListView(parent, THREADS_VIEW, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL)
	, selectionTimer(this, THREADS_VIEW_TIMER), m_focused(0)
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

struct ThreadsViewPred
{
	ThreadsViewPred(wxListView *view)
		: m_view(view)
	{
	}

	bool operator()(const ThreadsView::ThreadRow & item1, const ThreadsView::ThreadRow & item2) const
	{
		auto sort_column = m_view->GetSortIndicator();
		bool ascending = m_view->IsAscendingSortIndicator();
		auto a = &item1;
		auto b = &item2;
		if (!ascending)
			std::swap(a, b);

		switch (sort_column)
		{
		case ThreadsView::COL_TID:
			return a->tid < b->tid;
		case ThreadsView::COL_NAME:
			return wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
		case ThreadsView::MAX_COLUMNS:
			break;
		}
		return 0;
	}

private:
	wxListView *m_view;
};

void ThreadsView::OnSort(wxListEvent &event)
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

	fillList();
}

void ThreadsView::updateList()
{
	getThreadsFromDatabase();
	fillList();
}

std::vector<Database::ThreadID> ThreadsView::getSelectedThreads()
{
	std::vector<Database::ThreadID> selected;
	for (long i = GetFirstSelected(); i != wxNOT_FOUND; i = GetNextSelected(i))
	{
		const ThreadRow& row = threads.at(i);
		selected.push_back(row.tid);
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
	for (size_t i = 0; i < threads.size(); ++i)
	{
		const auto& row = threads.at(i);

		if (row.tid == tid)
		{
			m_focused = tid;
			Focus(i);
			EnsureVisible(i);
			break;
		}
	}
}

void ThreadsView::getThreadsFromDatabase()
{
	threads.clear();
	for (const auto &[tid, name] : database->getThreadNames())
	{
		ThreadRow row;
		row.tid = tid;
		row.name = name;
		threads.push_back(row);
	}
}

void ThreadsView::fillList()
{
	ThreadsViewPred pred(this);
	std::stable_sort(threads.begin(), threads.end(), pred);
	SetItemCount(threads.size());
	Refresh();
}

wxString ThreadsView::OnGetItemText(long item, long column) const
{
	switch (column)
	{
	case COL_TID:
		return wxString::Format("%d", threads[item].tid);
	case COL_NAME:
		return threads[item].name;
	}
	return "";
}

wxItemAttr *ThreadsView::OnGetItemAttr(long item) const
{
	static wxItemAttr* itemAttr = new wxItemAttr();
	*itemAttr = {};
	
	if (threads.at(item).tid == m_focused)
		itemAttr->SetTextColour(wxColor(0, 128, 0));

	return itemAttr;
}

BEGIN_EVENT_TABLE(ThreadSamplesView, wxListView)
EVT_LIST_COL_CLICK(wxID_ANY, ThreadSamplesView::OnSort)
EVT_LIST_ITEM_ACTIVATED(THREAD_SAMPLES_VIEW, ThreadSamplesView::OnActivated)
END_EVENT_TABLE()


ThreadSamplesView::ThreadSamplesView(wxWindow *parent, Database *database_)
	: wxListView(parent, THREAD_SAMPLES_VIEW, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL)
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

	fillList();
}

void ThreadSamplesView::OnActivated(wxListEvent &event)
{
	Database::ThreadID tid = threads.at(event.GetIndex()).tid;
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
}

void ThreadSamplesView::reset()
{
	SetItemCount(0);
	totalCount = 0;
	threads.clear();
}

struct ThreadSamplesPred
{
	ThreadSamplesPred(wxListView *view)
		: m_view(view)
	{
	}

	bool operator()(const ThreadSamplesView::ThreadRow& item1, const ThreadSamplesView::ThreadRow& item2) const
	{
		auto sort_column = m_view->GetSortIndicator();
		auto ascending = m_view->IsAscendingSortIndicator();
		auto a = &item1;
		auto b = &item2;
		if (!ascending)
			std::swap(a, b);

		switch (sort_column)
		{
		case ThreadSamplesView::COL_TID:
			return a->tid < b->tid;
		case ThreadSamplesView::COL_NAME:
			return wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
		case ThreadSamplesView::COL_EXCLUSIVE:
		case ThreadSamplesView::COL_EXCLUSIVEPCT:
			if (a->exclusive != b->exclusive)
				return a->exclusive < b->exclusive;
			return a->inclusive < b->inclusive;
		case ThreadSamplesView::COL_INCLUSIVE:
		case ThreadSamplesView::COL_INCLUSIVEPCT:
			if (a->inclusive != b->inclusive)
				return a->inclusive < b->inclusive;
			return a->exclusive < b->exclusive;
		case ThreadSamplesView::MAX_COLUMNS:
			break;
		}
		return false;
	}

private:
	wxListView *m_view;
};

void ThreadSamplesView::fillList()
{
	ThreadSamplesPred pred(this);
	std::stable_sort(threads.begin(), threads.end(), pred);
	SetItemCount(threads.size());
	Refresh();
}

wxString ThreadSamplesView::OnGetItemText(long item, long column) const
{
	switch(column)
	{
		case COL_TID:
			return wxString::Format("%d", threads[item].tid);
		case COL_NAME:
			return threads[item].name;
		case COL_EXCLUSIVE:
			return wxString::Format("%0.2fs", threads[item].exclusive);
		case COL_INCLUSIVE:
			return wxString::Format("%0.2fs", threads[item].inclusive);
		case COL_EXCLUSIVEPCT:
			return wxString::Format("%0.2f%%", threads[item].exclusive * 100.0f / totalCount);
		case COL_INCLUSIVEPCT:
			return wxString::Format("%0.2f%%", threads[item].inclusive * 100.0f / totalCount);
	}

	return "";
}
