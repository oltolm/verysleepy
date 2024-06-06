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

#include "CallstackView.h"
#include "database.h"
#include <wx/listctrl.h>

/*=====================================================================
ProcList
--------

=====================================================================*/
class ProcList : public wxListView, public AddressList
{
public:
	ProcList(wxWindow *parent, bool isroot, Database *database);

	virtual ~ProcList() = default;

	void OnSelected(wxListEvent& event);
	void OnActivated(wxListEvent& event);
	void OnSort(wxListEvent& event);
	void OnContextMenu(wxContextMenuEvent& event);

	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr *OnGetItemAttr(long item) const override;

	/// Recreates the GUI list from the given one. Preserves selection.
	void showList(Database::List list);

	void focusSymbol(const Database::Symbol *symbol, bool select = false);
	const Database::Symbol *getFocusedSymbol();

	Database::Address getAddress(int item) const override;

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

private:
	friend struct ProcessInfoPred;
	Database::List list;
	const Database::Symbol *m_selected = nullptr;

	struct Column
	{
		wxString name;
		int columnPosition;
		bool ascending;
	};

	bool isroot; // Are we the main proc list?
	bool updating; // Is a selection update in progress? (ignore selection events)

	Database* database;

	Column columns[MAX_COLUMNS];
	void setupColumn(ColumnType id, int width, bool ascending, const wxString &name);

	/// Displays our in-memory list. Preserves selection.
	void displayList();

	DECLARE_EVENT_TABLE()
};
