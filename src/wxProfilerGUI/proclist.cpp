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
#include "../utils/container.h"
#include "contextmenu.h"
#include "database.h"
#include "mainwin.h"
#include "proclist.h"
#include "sourceview.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <wx/font.h>
#include <wx/itemattr.h>
#include <wx/listctrl.h>

enum
{
	ProcList_List = 1
};

BEGIN_EVENT_TABLE(ProcList, wxListView)
EVT_LIST_ITEM_SELECTED(ProcList_List, ProcList::OnSelected)
EVT_LIST_ITEM_ACTIVATED(ProcList_List, ProcList::OnActivated)
EVT_LIST_COL_CLICK(wxID_ANY, ProcList::OnSort)
EVT_CONTEXT_MENU(ProcList::OnContextMenu)
END_EVENT_TABLE()

ProcList::ProcList(wxWindow *parent, bool isroot, Database *database)
:	wxListView(parent, ProcList_List, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_VIRTUAL),
	isroot(isroot), updating(false),
	database(database)
{
	this->isroot = isroot;

	for (int n=0;n<MAX_COLUMNS;n++)
		columns[n].columnPosition = -1;

	if (isroot)
	{
		setupColumn(COL_NAME,			360,	true,	_T("Name"));
		setupColumn(COL_EXCLUSIVE,		70,		false,	_T("Exclusive"));
		setupColumn(COL_INCLUSIVE,		70,		false,	_T("Inclusive"));
		setupColumn(COL_EXCLUSIVEPCT,	70,		false,	_T("% Exclusive"));
		setupColumn(COL_INCLUSIVEPCT,	70,		false,	_T("% Inclusive"));
	} else {
		setupColumn(COL_NAME,			170,	true,	_T("Name"));
		setupColumn(COL_SAMPLES,		70,		false,	_T("Samples"));
		setupColumn(COL_CALLSPCT,		70,		false,	_T("% Calls"));
	}
	setupColumn(COL_MODULE,			70,		true,	_T("Module"));
	setupColumn(COL_SOURCEFILE,		270,	true,	_T("Source File"));
	setupColumn(COL_SOURCELINE,		40,		true,	_T("Source Line"));
	setupColumn(COL_ADDRESS,		100,	true,	_T("Address"));

	ShowSortIndicator(columns[isroot ? COL_EXCLUSIVE : COL_SAMPLES].columnPosition, false);
}


void ProcList::setupColumn(ColumnType id, int width, bool ascending, const wxString &name)
{
	int index = GetColumnCount();

	columns[id].name = name;
	columns[id].columnPosition = index;
	columns[id].ascending = ascending;

	wxListItem itemCol;
	itemCol.SetText(name);
	if (width >= 0)
		itemCol.SetWidth(FromDIP(width));
	InsertColumn(index, itemCol);
}


Database::Address ProcList::getAddress(int item) const
{
	return list.items.at(item).address;
}

wxString ProcList::OnGetItemText(long item, long column) const
{
	const Database::Item *i = &list.items.at(item);
	const Database::AddrInfo *addrinfo = database->getAddrInfo(i->address);
	const Database::Symbol *sym = addrinfo->symbol;

	int new_column = 0;
	for (int n = 0; n < MAX_COLUMNS; n++)
		if (columns[n].columnPosition == column)
			new_column = n;

	switch (new_column)
	{
	case COL_NAME:
		return sym->procname;
	case COL_EXCLUSIVE:
	case COL_SAMPLES:
		return wxString::Format("%0.2fs", i->exclusive);
	case COL_INCLUSIVE:
		return wxString::Format("%0.2fs", i->inclusive);
	case COL_EXCLUSIVEPCT:
	case COL_CALLSPCT:
		return wxString::Format("%0.2f%%", i->exclusive * 100.0f / list.totalcount);
	case COL_INCLUSIVEPCT:
		return wxString::Format("%0.2f%%", i->inclusive * 100.0f / list.totalcount);
	case COL_MODULE:
		return database->getModuleName(sym->module);
	case COL_SOURCEFILE:
		return database->getFileName(sym->sourcefile);
	case COL_SOURCELINE:
		return wxString::Format("%u", addrinfo->sourceline);
	case COL_ADDRESS:
		return wxString::Format("%#llx", i->address);
	}

	return "";
}

wxItemAttr *ProcList::OnGetItemAttr(long item) const
{
	static wxItemAttr *itemAttr = new wxItemAttr();
	*itemAttr = {};

	const Database::Item *i = &list.items.at(item);
	const Database::AddrInfo *addrinfo = database->getAddrInfo(i->address);
	const Database::Symbol *sym = addrinfo->symbol;

	const ViewState *viewstate = theMainWin->getViewState();

	if (sym->isCollapseFunction || sym->isCollapseModule)
		itemAttr->SetTextColour(wxColor(0, 128, 0));
	else if (i->inclusive == 0 && i->exclusive == 0)
		itemAttr->SetTextColour(wxColor(128, 128, 128));

	if (set_get(viewstate->highlighted, sym->address))
		itemAttr->SetBackgroundColour(wxColor(255, 255, 0));

	if (isroot && sym == m_selected)
	{
		wxFont font = GetFont();
		font.SetWeight(wxFONTWEIGHT_BOLD);
		itemAttr->SetFont(font);
	}

	return itemAttr;
}

struct ProcessInfoPred
{
	ProcessInfoPred(ProcList *procList)
		: m_procList(procList)
	{
	}

	bool operator()(const Database::Item& item1, const Database::Item& item2) const
	{
		auto sort_column = m_procList->GetSortIndicator();
		auto ascending = m_procList->IsAscendingSortIndicator();
		auto a = &item1;
		auto b = &item2;
		if (!ascending)
			std::swap(a, b);

		int columnType = 0;
		for (int n = 0; n < m_procList->MAX_COLUMNS; ++n)
			if (m_procList->columns[n].columnPosition == sort_column)
				columnType = n;

		switch (columnType)
		{
		case ProcList::COL_NAME:
			return a->symbol->procname < b->symbol->procname;
		case ProcList::COL_EXCLUSIVE:
		case ProcList::COL_EXCLUSIVEPCT:
		case ProcList::COL_SAMPLES:
		case ProcList::COL_CALLSPCT:
			if (a->exclusive != b->exclusive)
				return a->exclusive < b->exclusive;
			return a->inclusive < b->inclusive;
		case ProcList::COL_INCLUSIVE:
		case ProcList::COL_INCLUSIVEPCT:
			if (a->inclusive != b->inclusive)
				return a->inclusive < b->inclusive;
			return a->exclusive < b->exclusive;
		case ProcList::COL_MODULE:
			return a->symbol->module < b->symbol->module;
		case ProcList::COL_SOURCEFILE:
			return a->symbol->sourcefile < b->symbol->sourcefile;
		case ProcList::COL_SOURCELINE:
		case ProcList::COL_ADDRESS:
			return a->address < b->address;
		case ProcList::MAX_COLUMNS:
			break;
		}
		return 0;
	}

private:
	ProcList *m_procList;
};

void ProcList::OnSort(wxListEvent& event)
{
	int sort_column = GetSortIndicator();

	bool ascending = false;
	if (sort_column == event.GetColumn())
	{
		// toggle if we clicked on the same column as last time
		ascending = GetUpdatedAscendingSortIndicator(event.GetColumn());
	}
	else
	{
		// if switching columns, start with the default sort for that column type
		sort_column = event.GetColumn();

		for (const auto & column : columns)
			if (column.columnPosition == event.GetColumn())
				ascending = column.ascending;
	}

	ShowSortIndicator(event.GetColumn(), ascending);
	ProcessInfoPred pred(this);
	std::stable_sort(list.items.begin(), list.items.end(), pred);
	Refresh();
}

void ProcList::OnContextMenu(wxContextMenuEvent& WXUNUSED(event))
{
	FunctionMenu(this, database);
}

void ProcList::showList(Database::List list_)
{
	list = std::move(list_);
	displayList();
}

void ProcList::displayList()
{
	theMainWin->setProgress(L"Populating list...", list.items.size());

	const ViewState *viewstate = theMainWin->getViewState();

	auto partitionEnd =
		std::stable_partition(list.items.begin(), list.items.end(), [&](const Database::Item& i) {
			const Database::Symbol *sym = i.symbol;

			return !isroot || !set_get(viewstate->filtered, sym->address);
		});

	ProcessInfoPred pred(this);
	std::stable_sort(list.items.begin(), partitionEnd, pred);

	SetItemCount(std::distance(list.items.begin(), partitionEnd));
	Refresh();
	theMainWin->setProgress(NULL);
}

void ProcList::focusSymbol(const Database::Symbol *symbol, bool select)
{
	// If we use Freeze/Thaw here, we'll get an unpleasant blinking
	// even though the list is not being repopulated.
	if (updating)
		return;
	updating = true;

	for (size_t i = 0; i < list.items.size(); ++i)
	{
		auto item = &list.items.at(i);
		if (item->symbol == symbol)
		{
			Select(i);
			Focus(i);
		}
		else if (IsSelected(i))
		{
			Select(i, false);
		}
	}

	if (isroot && select)
	{
		m_selected = symbol;
		Refresh();
	}

	updating = false;
}

const Database::Symbol * ProcList::getFocusedSymbol()
{
	long i = GetFocusedItem();
	return i != wxNOT_FOUND ? list.items.at(i).symbol : nullptr;
}

void ProcList::OnSelected(wxListEvent& event)
{
	if (IsFrozen() || updating)
		return; // the list is being populated or updated
	updating = true;

	assert(GetWindowStyle() & wxLC_REPORT);

	const auto& item = list.items.at(event.GetIndex());
	auto addrInfo = database->getAddrInfo(item.address);
	const Database::Symbol *sym = addrInfo->symbol;	
	if (isroot)
		theMainWin->inspectSymbol(addrInfo);
	else
		theMainWin->focusSymbol(addrInfo);

	if (isroot)
	{
		m_selected = sym;
		Refresh();
	}

	updating = false;
}

void ProcList::OnActivated(wxListEvent& event)
{
	assert(GetWindowStyle() & wxLC_REPORT);

	if (!isroot)
	{
		const auto& item = list.items.at(event.GetIndex());
		auto addrInfo = database->getAddrInfo(item.address);
		theMainWin->inspectSymbol(addrInfo);
	}
}
