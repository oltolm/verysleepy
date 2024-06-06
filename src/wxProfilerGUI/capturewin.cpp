/*=====================================================================
capturewin.cpp
----------------

Copyright (C) Richard Mitton

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
#include "capturewin.h"
#include "../appinfo.h"
#include "profilergui.h"

#include <windows.h>
#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/tglbtn.h>

enum CaptureWinId
{
	CaptureWin_Pause = 1000,
};

// Doesn't really matter what number is here, it's just a guide
// so that we get good feedback around the 10000-50000 sample range.
#define MAX_RANGE		1600

BEGIN_EVENT_TABLE(CaptureWin, wxDialog)
EVT_TOGGLEBUTTON(CaptureWin_Pause, CaptureWin::OnPause)
EVT_BUTTON(wxID_OK, CaptureWin::OnOk)
EVT_BUTTON(wxID_CANCEL, CaptureWin::OnCancel)
END_EVENT_TABLE()

unsigned WM_TASKBARBUTTONCREATED = RegisterWindowMessage(L"TaskbarButtonCreated");

CaptureWin::CaptureWin()
	: wxDialog(NULL, wxID_ANY, wxString(_T(APPNAME) _T(" - profiling")), wxDefaultPosition,
			   wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
	  cancelled(false),
	  stopped(false),
	  paused(false)
{
	wxBoxSizer *rootsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *panelsizer = new wxBoxSizer(wxVERTICAL);

	rootsizer->SetMinSize(FromDIP(400), 0);

	wxPanel *panel = new wxPanel(this);

	progressText = new wxStaticText( panel, wxID_ANY, "Waiting..." );
	progressBar = new wxGauge( panel, wxID_ANY, 0, wxDefaultPosition, FromDIP(wxSize(100, 18)), wxGA_HORIZONTAL | wxGA_PROGRESS);
	progressBar->SetRange(MAX_RANGE);

	wxBitmap pause = LoadPngResource(L"button_pause", this);
	pauseButton = new wxBitmapToggleButton(
		panel, CaptureWin_Pause, pause, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );

	wxButton *okButton = new wxButton(panel, wxID_OK, "&Stop");
	okButton->SetToolTip("Stop profiling and display collected results.");
	wxButton *cancelButton = new wxButton(panel, wxID_CANCEL, "&Abort");
	cancelButton->SetToolTip("Stop profiling, discard collected results, and exit.");

	wxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(pauseButton,				0, wxALIGN_LEFT,	FromDIP(5));
	buttons->AddStretchSpacer();
	buttons->Add(okButton,					0, wxLEFT|wxRIGHT,	FromDIP(5));
	buttons->Add(cancelButton,				0, wxLEFT,			FromDIP(5));

	panelsizer->Add(progressText, 0, wxBOTTOM, FromDIP(5));
	panelsizer->Add(progressBar, 0, wxBOTTOM|wxEXPAND, FromDIP(10));
	panelsizer->Add(buttons, 0, wxEXPAND);

	okButton->SetDefault();

	panel->SetSizer(panelsizer);
	panel->SetAutoLayout(TRUE);

	rootsizer->Add(panel, 1, wxEXPAND | wxALL, FromDIP(10));
	SetSizer(rootsizer);
	rootsizer->SetSizeHints(this);
	SetAutoLayout(TRUE);

	Centre();
}

bool CaptureWin::UpdateProgress(const wxString& status, double progress)
{
	progressText->SetLabel(status);

	if (progress != progress) // NAN
	{
		if (paused)
			progressBar->SetValue(0);
		else
			progressBar->Pulse();
	}
	else
	{
		if (progress < 0) progress = 0;
		if (progress > 1) progress = 1;
		int n = progress * MAX_RANGE;
		progressBar->SetValue(n);
	}

	return !stopped;
}

void CaptureWin::OnPause(wxCommandEvent& event)
{
	paused = event.IsChecked();
	pauseButton->SetBitmapLabel(LoadPngResource(paused ? L"button_go" : L"button_pause", this));
	SetTitle(paused ? _T(APPNAME) L" - paused" : _T(APPNAME) L" - profiling");
}

void CaptureWin::OnOk(wxCommandEvent& WXUNUSED(event))
{
	stopped = true;
}

void CaptureWin::OnCancel(wxCommandEvent& WXUNUSED(event))
{
	cancelled = true;
	stopped = true;
}
