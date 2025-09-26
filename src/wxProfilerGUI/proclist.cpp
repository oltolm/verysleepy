/*=====================================================================
proclist.cpp
------------
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
#include "proclist.h"

#include "../utils/stringutils.h"
#include "sourceview.h"
#include "database.h"
#include <fstream>
#include <algorithm>
#include "contextmenu.h"
#include "mainwin.h"

enum
{
	ProcList_List = 1
};

BEGIN_EVENT_TABLE(ProcList, wxListCtrl)
EVT_LIST_ITEM_SELECTED(ProcList_List, ProcList::OnSelected)
EVT_LIST_ITEM_ACTIVATED(ProcList_List, ProcList::OnActivated)
EVT_LIST_COL_CLICK(wxID_ANY, ProcList::OnSort)
EVT_CONTEXT_MENU(ProcList::OnContextMenu)
END_EVENT_TABLE()

ProcList::ProcList(wxWindow *parent, bool isroot, Database *database)
	: wxListView(parent, ProcList_List),
	  isroot(isroot),
	  updating(false),
	  database(database)
{
	this->isroot = isroot;

	for (int n=0;n<MAX_COLUMNS;n++)
		columns[n].listctrl_column = -1;

	if (isroot)
	{
		setupColumn(COL_NAME, 360, true, _T("Name"));
		setupColumn(COL_EXCLUSIVE, 70, false, _T("Exclusive"));
		setupColumn(COL_INCLUSIVE, 70, false, _T("Inclusive"));
		setupColumn(COL_EXCLUSIVEPCT, 70, false, _T("% Exclusive"));
		setupColumn(COL_INCLUSIVEPCT, 70, false, _T("% Inclusive"));
	} else {
		setupColumn(COL_NAME, 170, true, _T("Name"));
		setupColumn(COL_SAMPLES, 70, false, _T("Samples"));
		setupColumn(COL_CALLSPCT, 70, false, _T("% Calls"));
	}
	setupColumn(COL_MODULE, 70, true, _T("Module"));
	setupColumn(COL_SOURCEFILE, 270, true, _T("Source File"));
	setupColumn(COL_SOURCELINE, 40, true, _T("Source Line"));
	setupColumn(COL_ADDRESS, 100, true, _T("Address"));

	ShowSortIndicator(columns[isroot ? COL_EXCLUSIVE : COL_SAMPLES].listctrl_column, false);
}

ProcList::~ProcList()
{

}

void ProcList::setupColumn(ColumnType id, int width, bool defsort, const wxString& name)
{
	int index = GetColumnCount();

	columns[id].name = name;
	columns[id].listctrl_column = index;
	columns[id].default_sort = defsort;

	wxListItem itemCol;
	itemCol.SetText(name);
	if (width >= 0)
		itemCol.SetWidth(FromDIP(width));
	InsertColumn(index, itemCol);
}

static int ProcComparator(wxIntPtr item1, wxIntPtr item2, wxIntPtr data)
{
	auto sort_column = ((ProcList *)data)->GetSortIndicator();
	auto ascending = ((ProcList *)data)->IsAscendingSortIndicator();
	auto a = (Database::Item *)item1;
	auto b = (Database::Item *)item2;
	if (!ascending)
		std::swap(a, b);

	int columnType = 0;
	for (int n = 0; n < ((ProcList *)data)->MAX_COLUMNS; ++n)
		if (((ProcList *)data)->columns[n].listctrl_column == sort_column)
			columnType = n;

	switch (columnType)
	{
	case ProcList::COL_NAME:
		return a->symbol->procname.compare(b->symbol->procname);
	case ProcList::COL_EXCLUSIVE:
	case ProcList::COL_EXCLUSIVEPCT:
	case ProcList::COL_SAMPLES:
	case ProcList::COL_CALLSPCT:
		if (a->exclusive != b->exclusive)
			return a->exclusive < b->exclusive ? -1 : a->exclusive > b->exclusive ? 1 : 0;
		return a->inclusive < b->inclusive ? -1 : a->inclusive > b->inclusive ? 1 : 0;
	case ProcList::COL_INCLUSIVE:
	case ProcList::COL_INCLUSIVEPCT:
		if (a->inclusive != b->inclusive)
			return a->inclusive < b->inclusive ? -1 : a->inclusive > b->inclusive ? 1 : 0;
		return a->exclusive < b->exclusive;
	case ProcList::COL_MODULE:
		return a->symbol->module < b->symbol->module   ? -1
			   : a->symbol->module > b->symbol->module ? 1
													   : 0;
	case ProcList::COL_SOURCEFILE:
		return a->symbol->sourcefile < b->symbol->sourcefile   ? -1
			   : a->symbol->sourcefile > b->symbol->sourcefile ? 1
															   : 0;
	case ProcList::COL_SOURCELINE:
	case ProcList::COL_ADDRESS:
		return a->address < b->address ? -1 : a->address > b->address ? 1 : 0;
	case ProcList::MAX_COLUMNS:
		break;
	}
	return 0;
}

void ProcList::OnSort(wxListEvent& event)
{
	bool ascending = false;
	if (GetSortIndicator() == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	}
	else
	{
		// if switching columns, start with the default sort for that column type
		for (const auto& column : columns)
			if (column.listctrl_column == event.GetColumn())
				ascending = column.default_sort;
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	SortItems(ProcComparator, (wxIntPtr)this);
}

void ProcList::OnContextMenu(wxContextMenuEvent& WXUNUSED(event))
{
	FunctionMenu(this, database);
}

void ProcList::showList(const Database::List &list_)
{
	this->list = list_;
	displayList();
	SortItems(ProcComparator, (wxIntPtr)this);
}

void ProcList::displayList()
{
	theMainWin->setProgress(L"Saving list state...");
	std::unordered_map<const Database::AddrInfo *, int> item_state;
	// TODO: use GetNextItem?
	for (long i = 0; i < GetItemCount(); i++)
		if (int state = GetItemState(i, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED))
		{
			Database::Item *item = (Database::Item *)GetItemData(i);
			const Database::AddrInfo *addrinfo = database->getAddrInfo(item->address);
			item_state[addrinfo] = state;
		}

	theMainWin->setProgress(L"Clearing list...");
	Freeze();
	DeleteAllItems();

	theMainWin->setProgress(L"Populating list...", list.items.size());
	const ViewState *viewstate = theMainWin->getViewState();

	for (auto i = list.items.begin(); i != list.items.end(); ++i)
	{
		const Database::Symbol *sym = i->symbol;

		if (isroot && set_get(viewstate->filtered, sym->address))
			continue;

		long c = GetItemCount();

		wxListItem item;
		item.SetId(c);
		item.SetText(sym->procname);

		if (sym->isCollapseFunction || sym->isCollapseModule)
			item.SetTextColour(wxColor(0,128,0));
		else
		if (i->inclusive == 0 && i->exclusive == 0)
			item.SetTextColour(wxColor(128, 128, 128));

		if (set_get(viewstate->highlighted, sym->address))
			item.SetBackgroundColour(wxColor(255,255,0));

		const Database::AddrInfo *addrinfo = database->getAddrInfo(i->address);
		int state = map_get(item_state, addrinfo, 0);
		item.SetData((void *)&*i);
		item.SetState(state);
		item.SetStateMask(wxLIST_STATE_FOCUSED|wxLIST_STATE_SELECTED);

		InsertItem(item);

		wxString inclusive = wxString::Format("%0.2fs", i->inclusive);
		wxString exclusive = wxString::Format("%0.2fs", i->exclusive);
		wxString inclusivepercent = wxString::Format("%0.2f%%", i->inclusive * 100.0f / list.totalcount);
		wxString exclusivepercent = wxString::Format("%0.2f%%", i->exclusive * 100.0f / list.totalcount);

		setColumnValue(c, COL_EXCLUSIVE,	exclusive);
		setColumnValue(c, COL_INCLUSIVE,	inclusive);
		setColumnValue(c, COL_EXCLUSIVEPCT,	exclusivepercent);
		setColumnValue(c, COL_INCLUSIVEPCT,	inclusivepercent);
		setColumnValue(c, COL_SAMPLES,		exclusive);
		setColumnValue(c, COL_CALLSPCT,		exclusivepercent);
		setColumnValue(c, COL_MODULE,		database->getModuleName(sym->module));
		setColumnValue(c, COL_SOURCEFILE,	database->getFileName  (sym->sourcefile));
		setColumnValue(c, COL_SOURCELINE,	wxString::Format("%u", addrinfo->sourceline));
		setColumnValue(c, COL_ADDRESS,	    wxString::Format("%#llx", i->address));

		if (state & wxLIST_STATE_FOCUSED)
			EnsureVisible(c);

		theMainWin->updateProgress(i-list.items.begin());
	}

	Thaw();
	theMainWin->setProgress(NULL);
}

void ProcList::setColumnValue(int row, ColumnType id, const wxString &value)
{
	int listcol = columns[id].listctrl_column;
	if (listcol != -1)
		SetItem(row, listcol, value);
}

void ProcList::focusSymbol(const Database::Symbol *symbol)
{
	// If we use Freeze/Thaw here, we'll get an unpleasant blinking
	// even though the list is not being repopulated.
	if (updating) return;
	updating = true;

	for (long i = 0; i < GetItemCount(); i++)
	{
		Database::Item *item = (Database::Item *)GetItemData(i);
		if (item->symbol == symbol)
		{
			Select(i);
			Focus(i);
		}
		else
		{
			Select(i, false);
		}
	}

	updating = false;
}

const Database::Symbol * ProcList::getFocusedSymbol()
{
	long i = GetFocusedItem();
	return i != wxNOT_FOUND ? ((const Database::AddrInfo *)GetItemData(i))->symbol : NULL;
}

void ProcList::OnSelected(wxListEvent& event)
{
	if (IsFrozen() || updating)
		return; // the list is being populated or updated
	updating = true;

	assert(GetWindowStyle() & wxLC_REPORT);

	const Database::Item *item = (const Database::Item *)GetItemData(event.m_itemIndex);
	auto *addrinfo = database->getAddrInfo(item->address);
	if (isroot)
		theMainWin->inspectSymbol(addrinfo);
	else
		theMainWin->focusSymbol(addrinfo);

	updating = false;
}

void ProcList::OnActivated(wxListEvent& event)
{
	assert(GetWindowStyle() & wxLC_REPORT);

	const Database::Item *item = (const Database::Item *)GetItemData(event.m_itemIndex);
	const Database::AddrInfo *addrinfo = database->getAddrInfo(item->address);
	if (!isroot)
		theMainWin->inspectSymbol(addrinfo);
}
