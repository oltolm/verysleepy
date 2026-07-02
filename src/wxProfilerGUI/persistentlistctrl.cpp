#include "persistentlistctrl.h"

#include "profilergui.h"

#include <vector>
#include <wx/persist/window.h>

namespace
{
class PersistentConfigManager final : public wxPersistenceManager
{
protected:
	wxConfigBase *GetConfig() const override
	{
		return const_cast<wxConfig*>(&config);
	}
};

PersistentConfigManager g_persistenceManager;

class PersistentListCtrl final : public wxPersistentWindow<wxListCtrl>
{
public:
	explicit PersistentListCtrl(wxListCtrl *list)
		: wxPersistentWindow<wxListCtrl>(list)
	{
	}

	void Save() const override
	{
		wxListCtrl * const list = Get();
		const int columnCount = list->GetColumnCount();

		for (int col = 0; col < columnCount; ++col)
		{
			const wxString prefix = MakeColumnPrefix(list, col);
			SaveValue(prefix + "Width", list->GetColumnWidth(col));
#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
			SaveValue(prefix + "Order", list->GetColumnOrder(col));
#endif
		}
	}

	bool Restore() override
	{
		wxListCtrl * const list = Get();
		const int columnCount = list->GetColumnCount();

#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		std::vector<int> orders(columnCount, -1);
		std::vector<bool> usedOrders(columnCount, false);
		bool hasSavedOrder = false;
#endif

		for (int col = 0; col < columnCount; ++col)
		{
			const wxString prefix = MakeColumnPrefix(list, col);

			int width = -1;
			if (RestoreValue(prefix + "Width", &width) && width >= 0)
				list->SetColumnWidth(col, width);

#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
			int order = -1;
			if (RestoreValue(prefix + "Order", &order) &&
				order >= 0 &&
				order < columnCount &&
				!usedOrders[order])
			{
				orders[col] = order;
				usedOrders[order] = true;
				hasSavedOrder = true;
			}
#endif
		}

#ifdef wxHAS_LISTCTRL_COLUMN_ORDER
		if (hasSavedOrder)
		{
			int nextFreeOrder = 0;
			for (int col = 0; col < columnCount; ++col)
			{
				if (orders[col] != -1)
					continue;

				while (nextFreeOrder < columnCount && usedOrders[nextFreeOrder])
					++nextFreeOrder;

				if (nextFreeOrder >= columnCount)
					break;

				orders[col] = nextFreeOrder;
				usedOrders[nextFreeOrder] = true;
			}

			wxArrayInt orderArray;
			orderArray.reserve(columnCount);
			for (int col = 0; col < columnCount; ++col)
				orderArray.Add(orders[col] == -1 ? col : orders[col]);

			list->SetColumnsOrder(orderArray);
		}
#endif

		return true;
	}

	wxString GetKind() const override
	{
		return "ListCtrl";
	}

private:
	static wxString GetColumnTitle(wxListCtrl *list, int col)
	{
		wxListItem item;
		item.SetMask(wxLIST_MASK_TEXT);
		if (list->GetColumn(col, item) && !item.GetText().empty())
			return item.GetText();
		return wxString::Format("Column%d", col);
	}

	static wxString MakeColumnPrefix(wxListCtrl *list, int col)
	{
		return wxString::Format("/Columns/%s/", GetColumnTitle(list, col));
	}
};
}

void InitializeGuiPersistence()
{
	wxPersistenceManager::Set(g_persistenceManager);
}

bool RegisterListCtrlPersistence(wxListCtrl *list, const wxString& name)
{
	list->SetName(name);
	return wxPersistenceManager::Get().RegisterAndRestore(list, new PersistentListCtrl(list));
}
