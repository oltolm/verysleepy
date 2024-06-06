/*=====================================================================
logview.cpp
----------------

Copyright (C) Nicholas Chapman, Vladimir Panteleev

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

#include "logview.h"

#include <wx/menu.h>
#include <wx/log.h>
#include <windows.h>

// Note: we override DoLogRecord instead of DoLogText to avoid the default formatting,
// which includes timestamps. Since the debug engine sends output in line fragments at a time,
// the default formatting would prepend each line fragment with a timestamp, which results
// in a corrupted log.
void LogViewLog::DoLogRecord(wxLogLevel WXUNUSED(level), const wxString& msg, const wxLogRecordInfo& WXUNUSED(info))
{
	wxString str = msg;

	// dbghelp likes to add ASCII backspaces in. (sigh)
	// convert them out into newlines.
	int backspaceat = -1;
	for (size_t n=0;n<str.length();n++)
	{
		if (str[n] == '\b')
		{
			if (backspaceat == -1)
				backspaceat = (int)n;
			str[n] = '.';
		}
	}

	bool forceupdate = false;
	if (backspaceat != -1)
	{
		str[backspaceat] = '\n';
		forceupdate = true;
	}

	// We don't want to constantly update the log, or it turns into
	// one of those hilarious situations where outputting the progress takes more
	// time than the operation itself. So we freeze it, and update either
	// when we hit the idle, or every now and again here.
	if (!view->IsFrozen())
		view->Freeze();
	view->AppendText(str);
	view->ShowPosition(-1);

	// Update if it's been more than N seconds since our last one.
	static DWORD prev = 0;
	DWORD now = GetTickCount();
	DWORD diff = now - prev;
	if (diff > 500)
		forceupdate = true;

	if (forceupdate)
	{
		prev = now;
		if (view->IsFrozen())
			view->Thaw();
		view->Update();
	}
}

//////////////////////////////////////////////////////////////////////////
// LogView
//////////////////////////////////////////////////////////////////////////

enum
{
	// menu items
	LogView_Clear = 1,
};

BEGIN_EVENT_TABLE(LogView, wxTextCtrl)
EVT_CONTEXT_MENU(LogView::OnContextMenu)
EVT_MENU(wxID_COPY, LogView::OnCopy)
EVT_MENU(LogView_Clear, LogView::OnClearLog)
EVT_MENU(wxID_SELECTALL, LogView::OnSelectAll)
EVT_IDLE(LogView::OnIdle)
END_EVENT_TABLE()

LogView::LogView(wxWindow *parent)
:	wxTextCtrl(parent, 0, "", wxDefaultPosition, parent->FromDIP(wxSize(100,100)), wxTE_MULTILINE|wxTE_READONLY)
{
	// log = new LogViewLog(this);
	// previous_log = wxLog::SetActiveTarget(log);
	menu = new wxMenu;
	menu->Append(wxID_COPY, _("&Copy"));
	menu->Append(wxID_SELECTALL, _("Select &All"));
	menu->Append(LogView_Clear, _("Clear &Log"));
}

void LogView::OnContextMenu(wxContextMenuEvent& WXUNUSED(event))
{
	menu->Enable(LogView_Clear, !GetValue().empty());

	PopupMenu(menu);
}

void LogView::OnCopy(wxCommandEvent& WXUNUSED(event))
{
	Copy();
}

void LogView::OnClearLog(wxCommandEvent& WXUNUSED(event))
{
	Clear();
}

void LogView::OnSelectAll(wxCommandEvent& WXUNUSED(event))
{
	SetSelection(-1, -1);
}

void LogView::OnIdle(wxIdleEvent& WXUNUSED(event))
{
	if (IsFrozen())
		Thaw();
}
