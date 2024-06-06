/*=====================================================================
logview.h
--------------

Copyright (C) Vladimir Panteleev

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

#include <wx/log.h>
#include <wx/textctrl.h>

class LogViewLog;

class LogView :	public wxTextCtrl
{
public:
	LogView(wxWindow *parent);
	virtual ~LogView() = default;

	void OnContextMenu(wxContextMenuEvent& event);
	void OnCopy(wxCommandEvent& event);
	void OnClearLog(wxCommandEvent& event);
	void OnSelectAll(wxCommandEvent& event);
	void OnIdle(wxIdleEvent& event);

private:
	wxMenu* menu;
	// class wxLog *previous_log;
	// LogViewLog *log;

	DECLARE_EVENT_TABLE()
};

//////////////////////////////////////////////////////////////////////////
// LogViewLog
//////////////////////////////////////////////////////////////////////////

// We can't use the same class for both wxTextCtrl and wxLog,
// because wxWidgets will destroy the active log BEFORE it
// destroys the window. That means we'd be left with a dangling
// pointer in wxWidgets' window hierarchy that we can't do
// anything about.
class LogViewLog : public wxLogTextCtrl
{
	LogView *view;

public:
	LogViewLog(LogView *view)
		: wxLogTextCtrl(view),
		  view(view)
	{
	}

	virtual ~LogViewLog()
	{
		// Tell the view that we've been destroyed (by wxWidgets, probably).
		// view->log = NULL;
		// If we are destroyed by wxWidgets, we don't need to worry about
		// setting the old log target back - wxWidgets does that.
	}

	void DoLogRecord(wxLogLevel level, const wxString& msg, const wxLogRecordInfo& info);
};
