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

#include "CallstackView.h"
#include <algorithm>
#include <wx/aui/auibar.h>
#include <wx/filedlg.h>
#include <wx/dcclient.h>
#include <wx/gdicmn.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/listctrl.h>
#include "contextmenu.h"
#include "mainwin.h"
#include "../utils/container.h"
#include "../utils/stringutils.h"
#include "guiutils.h"

BEGIN_EVENT_TABLE(CallstackView, wxWindow)
EVT_SIZE(CallstackView::OnSize)
EVT_TOOL_RANGE(0,10,CallstackView::OnTool)
EVT_LIST_ITEM_SELECTED(LIST_CTRL, CallstackView::OnSelected)
EVT_CONTEXT_MENU(CallstackView::OnContextMenu)
END_EVENT_TABLE()

CallstackView::CallstackView(wxWindow *parent,Database *_database)
:	wxWindow(parent,wxID_ANY), database(_database), callstackActive(0), currSymbol(NULL), itemSelected(~0u)
{
	listCtrl = new wxListView(this, LIST_CTRL);
	setupColumn(COL_NAME,			170,	_T("Name"));
	setupColumn(COL_MODULE,			70,		_T("Module"));
	setupColumn(COL_SOURCEFILE,		270,	_T("Source File"));
	setupColumn(COL_SOURCELINE,		40,		_T("Source Line"));
	setupColumn(COL_ADDRESS,		100,	_T("Address"));

	toolBar =
		new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxAUI_TB_NO_AUTORESIZE);
	toolBar->AddTool(TOOL_PREV, "-", wxBITMAP_PNG(button_prev), _T("Previous"));
	toolBar->AddTool(TOOL_NEXT, "+", wxBITMAP_PNG(button_next), _T("Next"));
	toolBar->AddTool(TOOL_EXPORT_CSV, "CSV", wxBITMAP_PNG(button_exportcsv), _T("Export as CSV"));
	toolBar->AddLabel(TOOL_LABEL);

	toolBar->Realize();

	wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
	sizer->Add(toolBar,wxSizerFlags(0).Expand());
	sizer->Add(listCtrl,wxSizerFlags(1).Expand());
	SetSizer(sizer);
	sizer->SetSizeHints(this);
}

void CallstackView::OnSelected(wxListEvent& event)
{
	itemSelected = event.GetIndex();
	if (callstackActive < callstacks.size() && (size_t)itemSelected < callstacks[callstackActive]->symbols.size())
	{
		const Database::AddrInfo *addrinfo = database->getAddrInfo(callstacks[callstackActive]->addresses[itemSelected]);
		theMainWin->focusSymbol(addrinfo);
	}
	itemSelected = ~0u;
}

void CallstackView::OnSize(wxSizeEvent& WXUNUSED(event))
{
	Layout();
}

void CallstackView::setupColumn(ColumnType index, int width, const wxString &name)
{
	wxListItem itemCol;
	itemCol.SetText(name);
	if (width >= 0)
		itemCol.SetWidth(FromDIP(width));
	listCtrl->InsertColumn(index, itemCol);
}

CallstackView::~CallstackView() {}

void CallstackView::showCallStack(const Database::Symbol *symbol)
{
	updateTools();

	if(currSymbol == symbol || symbol == NULL)
		return;

	currSymbol = symbol;

	const Database::CallStack *pCallStackSelected;
	if(callstackActive < callstacks.size()) {
		pCallStackSelected = callstacks[callstackActive];
	} else {
		pCallStackSelected = NULL;
	}

	callstacks = database->getCallstacksContaining(symbol);
	std::vector<std::pair<const Database::CallStack *, double>> callstackCosts(callstacks.size());
	for (size_t i = 0; i < callstackCosts.size(); ++i)
	{
		callstackCosts[i].first = callstacks[i];
		callstackCosts[i].second = database->getFilteredSampleCount(callstacks[i]->samples);
	}

	std::sort(callstackCosts.begin(), callstackCosts.end(),
			  [](auto const& a, auto const& b) { return a.second > b.second; });

	callstackActive = 0;

	for(size_t i=0;i<callstacks.size();i++)  {
		callstacks[i] = callstackCosts[i].first;
		if(callstacks[i] == pCallStackSelected) {
			callstackActive = i;
		}
	}
	updateList();
}

void CallstackView::reset()
{
	callstacks.clear();
	callstackActive = 0;
	callstackStats.clear();
	currSymbol = NULL;
	itemSelected = ~0u;
	listCtrl->DeleteAllItems();
}

void CallstackView::updateTools()
{
	toolBar->EnableTool(TOOL_PREV,callstackActive != 0);
	toolBar->EnableTool(TOOL_NEXT,int(callstackActive) < int(callstacks.size()-1));
	toolBar->EnableTool(TOOL_EXPORT_CSV,!callstacks.empty());
	toolBar->SetToolLabel(TOOL_LABEL, callstackStats);
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
			(int)(callstackActive+1),(int)callstacks.size(),nowCount,nowCount*100/totalcount);
	} else {
		callstackStats = wxString("");
	}

	updateTools();

	if(!now)
		return;

	const ViewState *viewstate = theMainWin->getViewState();

	for (size_t i = 0; i < now->symbols.size(); i++)
	{
		const Database::Symbol *snow = now->symbols[i];
		Database::Address addr = now->addresses[i];
		const Database::AddrInfo *addrinfo = database->getAddrInfo(addr);

		if (i == (size_t)listCtrl->GetItemCount())
			listCtrl->InsertItem(i, snow->procname);
		else
			listCtrl->SetItem(i, COL_NAME, snow->procname);

		if (snow->isCollapseFunction || snow->isCollapseModule)
			listCtrl->SetItemTextColour(i, lightOrDark(wxTheColourDatabase->Find("green")));
		else
			listCtrl->SetItemTextColour(i, listCtrl->GetTextColour());

		if (set_get(viewstate->highlighted, snow->address))
			listCtrl->SetItemBackgroundColour(i, lightOrDark(*wxYELLOW));
		else
			listCtrl->SetItemBackgroundColour(i, listCtrl->GetBackgroundColour());

		listCtrl->SetItem(i, COL_MODULE    , database->getModuleName(snow->module));
		listCtrl->SetItem(i, COL_SOURCEFILE, database->getFileName  (snow->sourcefile));
		listCtrl->SetItem(i, COL_SOURCELINE, wxString::Format("%d", addrinfo->sourceline));
		listCtrl->SetItem(i, COL_ADDRESS   , wxString::Format("%#llx", addr));

		wxFont font = listCtrl->GetFont();
		if(snow == currSymbol)
			font.SetWeight(wxFONTWEIGHT_BOLD);
		else
			font.SetWeight(wxFONTWEIGHT_NORMAL);

		listCtrl->SetItemFont(i, font);
		if(i == itemSelected) {
			listCtrl->Select(i);
			listCtrl->Focus(i);
		} else {
			listCtrl->Select(i, false);
		}
		listCtrl->SetItemPtrData(i, (wxUIntPtr)addrinfo);
	}

	while (listCtrl->GetItemCount() > int(now->symbols.size()))
		listCtrl->DeleteItem(listCtrl->GetItemCount()-1);
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
		writeQuote(txt, column.GetText().wc_string(), '"');
		txt << ((columnIndex == (columnCount - 1)) ? "\n" : ",");
	}

	for(int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		for(int columnIndex = 0; columnIndex < columnCount; columnIndex++ )
		{
			writeQuote(txt, listCtrl->GetItemText(rowIndex, columnIndex).wc_string(), '"');
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
		wxFileDialog dlg(this, "Export Callstack As", wxEmptyString, "callstack.csv",
						 "CSV Files (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
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
