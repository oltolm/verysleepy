/*=====================================================================
capturewin.h
--------------

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
#pragma once

#include <wx/dialog.h>

class wxGauge;
class wxStaticText;

class CaptureWin : public wxDialog
{
public:
	CaptureWin();
	virtual ~CaptureWin() = default;

	bool UpdateProgress(const wxString& status, double progress);

	bool Paused() const { return paused; }
	bool Cancelled() const { return cancelled; }

private:
	void OnOk( wxCommandEvent & event );
	void OnPause( wxCommandEvent & event );
	void OnCancel( wxCommandEvent & event );

	bool cancelled, stopped, paused;
	wxGauge *progressBar;
	wxStaticText *progressText;
	class wxBitmapToggleButton *pauseButton;

	DECLARE_EVENT_TABLE()
};
