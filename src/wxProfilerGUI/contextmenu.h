#pragma once

class Database;
class wxListView;

/// Show a right-click menu for a given wxListCtrl.
/// We assume that the selected items are the actionable ones,
/// and that the list stores Database::Address-es in its
/// ItemData.
void FunctionMenu(wxListView *list, Database *database);
