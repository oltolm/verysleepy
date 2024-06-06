/*=====================================================================
optionsdlg.h
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

class wxCheckBox;
class wxDirPickerCtrl;
class wxListBox;
class wxRadioButton;
class wxSlider;
class wxTextCtrl;

class OptionsDlg : public wxDialog
{
public:
	OptionsDlg();
	virtual ~OptionsDlg() = default;

private:
	void OnOk( wxCommandEvent & event );

	void UpdateSymPathButtons();
	void OnSymPath( wxCommandEvent & event );
	void OnSymPathAdd( wxCommandEvent & event );
	void OnSymPathRemove( wxCommandEvent & event );
	void OnSymPathMoveUp( wxCommandEvent & event );
	void OnSymPathMoveDown( wxCommandEvent & event );
	void OnUseSymServer( wxCommandEvent & event );
	void OnSaveMinidump( wxCommandEvent & event );

	wxListBox *symPaths;
	wxButton *symPathAdd, *symPathRemove, *symPathMoveUp, *symPathMoveDown;
	wxCheckBox *useSymServer;
	wxDirPickerCtrl *symCacheDir;
	wxTextCtrl *symServer;
	wxCheckBox *saveMinidump;
	wxTextCtrl *saveMinidumpTime;
	wxRadioButton *mingwWine;
	wxRadioButton *mingwDrMingw;
	int saveMinidumpTimeValue;
	wxSlider *throttle;

	DECLARE_EVENT_TABLE()
};
