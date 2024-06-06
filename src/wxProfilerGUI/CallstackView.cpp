/*=====================================================================
launchdlg.cpp
----------------

Copyright (C) Johan Kohler

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
#include "../utils/stringutils.h"
#include "CallstackView.h"
#include "contextmenu.h"
#include "database.h"
#include "mainwin.h"
#include "profilergui.h"

#include <algorithm>
#include <unordered_map>
#include <wx/aui/auibar.h>
#include <wx/dcclient.h>
#include <wx/filedlg.h>
#include <wx/itemattr.h>
#include <wx/listctrl.h>

enum ListCtrl {
	LIST_CTRL = 1000
};

enum ColumnType
{
	COL_NAME,
	COL_MODULE,
	COL_SOURCEFILE,
	COL_SOURCELINE,
	COL_ADDRESS,
	MAX_COLUMNS
};

class wxStaticTextTransparent: public wxControl
{
	wxString label;
public:
	wxStaticTextTransparent(wxWindow *parent,wxWindowID id)
	{
		wxString spaces(' ',1024);
		wxSize	size;
		GetTextExtent(spaces,&size.x,&size.y);
		Create(parent, id, wxDefaultPosition, size, wxTRANSPARENT_WINDOW|wxNO_BORDER);
		SetMinSize(size);
		SetBackgroundColour(*wxGREEN);
	}

	void OnPaint(wxPaintEvent& WXUNUSED(event))
	{
		wxPaintDC dc(this);
		dc.SetBackgroundMode(wxTRANSPARENT);
		dc.SetFont(GetFont());
		dc.DrawText(label, 0,0);
	}

	void SetLabel(const wxString& label_)
	{
		label = label_;
	}

	void OnEraseBackground(wxEraseEvent& WXUNUSED(event))
	{
	}

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxStaticTextTransparent, wxControl)
EVT_PAINT(wxStaticTextTransparent::OnPaint)
EVT_ERASE_BACKGROUND(wxStaticTextTransparent::OnEraseBackground)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(CallstackView, wxWindow)
EVT_SIZE(CallstackView::OnSize)
EVT_TOOL_RANGE(0,10,CallstackView::OnTool)
EVT_LIST_ITEM_SELECTED(LIST_CTRL, CallstackView::OnSelected)
EVT_CONTEXT_MENU(CallstackView::OnContextMenu)
END_EVENT_TABLE()

class CallstackViewProcList : public wxListView, public AddressList
{
	void setupColumn(ColumnType id, int width, const wxString& name);

public:
	CallstackViewProcList(wxWindow *parent)
		: wxListView(parent, LIST_CTRL, wxDefaultPosition, wxDefaultSize,
					 wxLC_REPORT | wxLC_VIRTUAL)
	{
		setupColumn(COL_NAME, 170, _T("Name"));
		setupColumn(COL_MODULE, 70, _T("Module"));
		setupColumn(COL_SOURCEFILE, 270, _T("Source File"));
		setupColumn(COL_SOURCELINE, 40, _T("Source Line"));
		setupColumn(COL_ADDRESS, 100, _T("Address"));
	}

	wxString OnGetItemText(long item, long column) const override;
	wxItemAttr *OnGetItemAttr(long item) const override;

	Database::Address getAddress(int item) const override;
};

void CallstackViewProcList::setupColumn(ColumnType index, int width, const wxString& name)
{
	wxListItem itemCol;
	itemCol.SetText(name);
	if (width >= 0)
		itemCol.SetWidth(FromDIP(width));
	InsertColumn(index, itemCol);
}


Database::Address CallstackViewProcList::getAddress(int item) const
{
	auto parent = dynamic_cast<CallstackView *>(GetParent());
	const Database::CallStack *now = parent->callstacks[parent->callstackActive];
	return now->addresses.at(item);
}

wxString CallstackViewProcList::OnGetItemText(long item, long column) const
{
	auto parent = dynamic_cast<CallstackView *>(GetParent());

	const Database::CallStack *now = parent->callstacks[parent->callstackActive];
	const Database::Symbol *snow = now->symbols.at(item);
	Database::Address addr = now->addresses.at(item);
	const Database::AddrInfo *addrinfo = parent->database->getAddrInfo(addr);

	switch (column)
	{
	case COL_NAME:
		return snow->procname;
	case COL_MODULE:
		return parent->database->getModuleName(snow->module);
	case COL_SOURCEFILE:
		return parent->database->getFileName(snow->sourcefile);
	case COL_SOURCELINE:
		return wxString::Format("%u", addrinfo->sourceline);
	case COL_ADDRESS:
		return wxString::Format("%#llx", addr);
	}

	return "";
}

wxItemAttr *CallstackViewProcList::OnGetItemAttr(long item) const
{
	static wxItemAttr *itemAttr = new wxItemAttr;
	*itemAttr = {};

	auto parent = dynamic_cast<CallstackView *>(GetParent());

	if (parent->callstacks.empty())
		return itemAttr;

	const Database::CallStack *now = parent->callstacks[parent->callstackActive];

	const Database::Symbol *snow = now->symbols.at(item);

	const ViewState *viewstate = theMainWin->getViewState();

	if (snow->isCollapseFunction || snow->isCollapseModule)
		itemAttr->SetTextColour(wxColor(0,128,0));
	else
		itemAttr->SetTextColour(wxColor(0,0,0));

	if (set_get(viewstate->highlighted, snow->address))
		itemAttr->SetBackgroundColour(wxColor(255,255,0));
	else
		itemAttr->SetBackgroundColour(wxColor(255,255,255));

	wxFont font = GetFont();
	font.SetWeight(snow == parent->currSymbol ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
	itemAttr->SetFont(font);

	return itemAttr;
}

CallstackView::CallstackView(wxWindow *parent,Database *_database)
:	wxWindow(parent,wxID_ANY), listCtrl(new CallstackViewProcList(this)), database(_database), callstackActive(0), currSymbol(NULL)
{
	toolBar = new wxAuiToolBar(this);
	toolBar->AddTool(TOOL_PREV,"-",LoadPngResource(L"button_prev", this),_T("Previous"));
	toolBar->AddTool(TOOL_NEXT,"+",LoadPngResource(L"button_next", this),_T("Next"));
	toolBar->AddTool(TOOL_EXPORT_CSV,"CSV",LoadPngResource(L"button_exportcsv", this),_T("Export as CSV"));
	toolRange = new wxStaticTextTransparent(toolBar,wxID_ANY);
	toolBar->AddControl(toolRange);

	toolBar->Realize();

	wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
	sizer->Add(toolBar,wxSizerFlags(0).Expand());
	sizer->Add(listCtrl,wxSizerFlags(1).Expand());
	SetSizer(sizer);
	sizer->SetSizeHints(this);
}

void CallstackView::OnSelected(wxListEvent& event)
{
	const Database::CallStack *now = callstacks[callstackActive];

	auto item = event.GetIndex();

	Database::Address addr = now->addresses.at(item);
	const Database::AddrInfo *addrinfo = database->getAddrInfo(addr);

	theMainWin->focusSymbol(addrinfo);
}

void CallstackView::OnSize(wxSizeEvent& WXUNUSED(event))
{
	Layout();
}

void CallstackView::showCallStack(const Database::Symbol *symbol)
{
	updateTools();

	if(currSymbol == symbol || symbol == NULL)
		return;

	currSymbol = symbol;

	callstacks = database->getCallstacksContaining(symbol);
	listCtrl->SetItemCount(0);
	std::unordered_map<const Database::CallStack*, double> callstackMap;
	for (auto callstack : callstacks)
		callstackMap.emplace(callstack, database->getFilteredSampleCount(callstack->samples));

	std::sort(callstacks.begin(), callstacks.end(), [&](const auto a, const auto b) {
		return callstackMap[a] > callstackMap[b];
	});

	callstackActive = 0;

	updateList();
}

void CallstackView::reset()
{
	callstacks.clear();
	callstackActive = 0;
	callstackStats.clear();
	currSymbol = NULL;
	listCtrl->SetItemCount(0);
}

void CallstackView::updateTools()
{
	toolBar->EnableTool(TOOL_PREV,callstackActive != 0);
	toolBar->EnableTool(TOOL_NEXT,!callstacks.empty() && callstackActive < callstacks.size()-1);
	toolBar->EnableTool(TOOL_EXPORT_CSV,!callstacks.empty());
	toolRange->SetLabel(callstackStats);
	toolBar->Realize();
	toolBar->Refresh();
}

void CallstackView::updateList()
{
	const Database::CallStack *now = NULL;
	if(callstackActive < callstacks.size())
		now = callstacks[callstackActive];
	if(now) {
		double totalcount = database->getMainList().totalcount;
		double nowCount = database->getFilteredSampleCount(now->samples);
		callstackStats = wxString::Format("Call stack %d of %d | Accounted for %0.2fs (%0.2f%%)",
			(int)callstackActive+1,(int)callstacks.size(),nowCount,nowCount*100/totalcount);
	} else {
		callstackStats = "";
	}

	updateTools();

	if(!now)
		return;

	listCtrl->SetItemCount(now->symbols.size());
	listCtrl->Refresh();
}

void CallstackView::exportCSV(wxFileOutputStream &file)
{
	wxTextOutputStream txt(file);

	int columnCount = listCtrl->GetColumnCount();
	int rowCount = listCtrl->GetItemCount();

	for (int columnIndex = 0; columnIndex < columnCount; columnIndex++)
	{
		wxListItem column;
		column.SetMask(wxLIST_MASK_TEXT);
		listCtrl->GetColumn(columnIndex, column);
		writeQuote(txt, column.GetText().ToStdWstring(), '"');
		txt << ((columnIndex == (columnCount - 1)) ? "\n" : ",");
	}

	for(int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		for(int columnIndex = 0; columnIndex < columnCount; columnIndex++ )
		{
			writeQuote(txt, listCtrl->GetItemText(rowIndex, columnIndex).ToStdWstring(), '"');
			txt << ((columnIndex == (columnCount - 1)) ? "\n" : ",");
		}
	}
}

void CallstackView::OnTool(wxCommandEvent &event)
{
	if(event.GetId() == TOOL_PREV) {
		callstackActive--;
		updateList();
	}
	if(event.GetId() == TOOL_NEXT) {
		callstackActive++;
		updateList();
	}
	if(event.GetId() == TOOL_EXPORT_CSV) {
		wxFileDialog dlg(this, "Export Callstack As", "", "callstack.csv", "CSV Files (*.csv)|*.csv", wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
		if (dlg.ShowModal() != wxID_CANCEL)
		{
			wxFileOutputStream file(dlg.GetPath());
			if(!file.IsOk())
				wxLogSysError("Could not export profile data.\n");
			exportCSV(file);
		}
	}
}

void CallstackView::OnContextMenu(wxContextMenuEvent& WXUNUSED(event))
{
	FunctionMenu(listCtrl, database);
}
