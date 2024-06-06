/*=====================================================================
threadsview.h
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
#pragma once

#include "database.h"

#include <wx/listctrl.h>
#include <wx/timer.h>

class ThreadsView : public wxListView
{
public:
	ThreadsView(wxWindow *parent, Database *database);
	virtual ~ThreadsView() = default;

	void OnSelected(wxListEvent &event);
	void OnDeSelected(wxListEvent &event);
	void OnSort(wxListEvent &event);
	void OnTimer(wxTimerEvent &event);

	void updateList();

	std::vector<Database::ThreadID> getSelectedThreads();
	void clearSelectedThreads();

	void focusThread(Database::ThreadID tid);
	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr *OnGetItemAttr(long item) const override;

	enum ColumnType {
		COL_TID,
		COL_NAME,
		MAX_COLUMNS,
	};

	struct ThreadRow {
		Database::ThreadID tid;
		std::wstring name;
	};

private:
	std::vector<ThreadRow> threads;

	Database *database;
	wxTimer selectionTimer;
	Database::ThreadID m_focused;

	void startSelectionTimer();
	void getThreadsFromDatabase();
	void fillList();

	DECLARE_EVENT_TABLE()
};

class ThreadSamplesView : public wxListView
{
public:
	ThreadSamplesView(wxWindow *parent, Database *database);
	virtual ~ThreadSamplesView() = default;

	void OnSort(wxListEvent &event);
	void OnActivated(wxListEvent &event);

	void showList(Database::SymbolSamples const &symbolSamples);
	void reset();

	enum ColumnType {
		COL_TID,
		COL_NAME,
		COL_EXCLUSIVE,
		COL_INCLUSIVE,
		COL_EXCLUSIVEPCT,
		COL_INCLUSIVEPCT,
		MAX_COLUMNS,
	};

	struct ThreadRow {
		Database::ThreadID tid;
		std::wstring name;
		double exclusive, inclusive;
	};

	wxString OnGetItemText(long item, long column) const override;

private:
	double totalCount;
	std::vector<ThreadRow> threads;

	Database *database;

	void fillList();

	DECLARE_EVENT_TABLE()
};

enum {
	THREADS_VIEW = 5107,
	THREADS_VIEW_TIMER,
	THREAD_SAMPLES_VIEW,
};
