#pragma once

#include <wx/listctrl.h>

void InitializeGuiPersistence();
bool RegisterListCtrlPersistence(wxListCtrl *list, const wxString& name);
