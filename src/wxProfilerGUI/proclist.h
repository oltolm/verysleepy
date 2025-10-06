/*=====================================================================
proclist.h
----------
File created by ClassTemplate on Tue Mar 15 21:13:18 2005

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

#include "database.h"
#include "profilergui.h"
#include <wx/listctrl.h>

/*=====================================================================
ProcList
--------

=====================================================================*/
class ProcList : public wxListView
{
public:
	ProcList(wxWindow *parent, bool isroot, Database *database);

	virtual ~ProcList();

	void OnSelected(wxListEvent& event);
	void OnActivated(wxListEvent& event);
	void OnSort(wxListEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);

	/// Recreates the GUI list from the given one. Preserves selection.
	void showList(const Database::List &list);

	void focusSymbol(const Database::Symbol *symbol);
	const Database::Symbol *getFocusedSymbol();

	struct Column
	{
		wxString name;
		int listctrl_column;
		bool default_sort;
	};

	enum ColumnType
	{
		COL_NAME,
		COL_EXCLUSIVE,
		COL_INCLUSIVE,
		COL_EXCLUSIVEPCT,
		COL_INCLUSIVEPCT,
		COL_SAMPLES,
		COL_CALLSPCT,
		COL_MODULE,
		COL_SOURCEFILE,
		COL_SOURCELINE,
		COL_ADDRESS,
		MAX_COLUMNS
	};

	Column columns[MAX_COLUMNS];

private:
	Database::List list;

	DECLARE_EVENT_TABLE()

	bool isroot; // Are we the main proc list?
	bool updating; // Is a selection update in progress? (ignore selection events)

	Database *database;

	void setupColumn(ColumnType id, int width, bool defsort, const wxString& name);
	void setColumnValue(int row, ColumnType id, const wxString &value);

	/// Sorts the in-memory list. Does not affect GUI.
	void sortList();

	/// Displays our in-memory list. Preserves selection.
	void displayList();
};
