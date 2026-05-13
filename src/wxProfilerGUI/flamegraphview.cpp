/*=====================================================================
flamegraphview.cpp
------------------

Copyright (C) Very Sleepy authors and contributors

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

#include "flamegraphview.h"

#include "mainwin.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace
{
const int kBarGap = 2;
const int kMargin = 8;
const int kMinLabelWidth = 36;
const int kPreferredBarHeight = 16;
const int kToolbarHeight = 30;
const int kResetZoomButton = wxID_HIGHEST + 201;
}

BEGIN_EVENT_TABLE(FlameGraphView, wxScrolledWindow)
EVT_PAINT(FlameGraphView::OnPaint)
EVT_SIZE(FlameGraphView::OnSize)
EVT_MOTION(FlameGraphView::OnMouseMove)
EVT_LEAVE_WINDOW(FlameGraphView::OnMouseLeave)
EVT_LEFT_DOWN(FlameGraphView::OnLeftDown)
EVT_RIGHT_DOWN(FlameGraphView::OnRightDown)
EVT_BUTTON(kResetZoomButton, FlameGraphView::OnResetZoom)
END_EVENT_TABLE()

FlameGraphView::FlameGraphView(wxWindow *parent, Database *database_)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME),
	  database(database_),
	  resetZoomButton(NULL),
	  selectedSymbol(NULL),
	  hoveredNode(NULL),
	  zoomRoot(NULL),
	  maxDepth(0),
	  chartDirty(true),
	  rowHeight(FromDIP(kPreferredBarHeight)),
	  visibleDepthLimit(0),
	  hiddenDepthCount(0)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetScrollRate(0, 0);

	resetZoomButton = new wxButton(this, kResetZoomButton, "Reset Zoom");
	resetZoomButton->Hide();
}

FlameGraphView::~FlameGraphView()
{
}

void FlameGraphView::showChart(const Database::Symbol *selectedSymbol_)
{
	selectedSymbol = selectedSymbol_;
	if (chartDirty)
	{
		rebuildChart();
		chartDirty = false;
	}
	Refresh();
}

void FlameGraphView::reset()
{
	flameGraph.reset();
	layout.clear();
	selectedSymbol = NULL;
	hoveredNode = NULL;
	zoomRoot = NULL;
	zoomPath.clear();
	maxDepth = 0;
	chartDirty = true;
	visibleDepthLimit = 0;
	hiddenDepthCount = 0;
	updateResetButton();
	Refresh();
}

void FlameGraphView::rebuildChart()
{
	flameGraph = database->buildFlameGraph();
	zoomRoot = flameGraph.get();
	zoomPath.clear();
	rebuildLayout();
}

void FlameGraphView::rebuildLayout()
{
	layout.clear();
	hoveredNode = NULL;
	maxDepth = 0;

	if (!flameGraph || flameGraph->inclusive <= 0.0)
	{
		updateResetButton();
		return;
	}

	int clientWidth = std::max(GetClientSize().GetWidth(), FromDIP(200));
	double chartWidth = std::max(1, clientWidth - 2 * FromDIP(kMargin));

	const Database::FlameGraphNode *root = zoomRoot ? zoomRoot : flameGraph.get();
	zoomPath.clear();
	buildZoomPath(flameGraph.get(), root, zoomPath);

	int ancestorRows = 0;
	for (size_t i = 0; i + 1 < zoomPath.size(); ++i)
	{
		if (zoomPath[i]->symbol)
			ancestorRows++;
	}
	int subtreeRows = std::max(1, getSubtreeDepth(root));
	int totalRows = ancestorRows + subtreeRows;
	int availableHeight = std::max(1, GetClientSize().GetHeight() - FromDIP(kToolbarHeight) - 2 * FromDIP(kMargin));
	rowHeight = FromDIP(kPreferredBarHeight);
	int slotHeight = rowHeight + FromDIP(kBarGap);
	visibleDepthLimit = std::max(1, (availableHeight + FromDIP(kBarGap)) / std::max(1, slotHeight));
	hiddenDepthCount = std::max(0, totalRows - visibleDepthLimit);
	maxDepth = totalRows;

	layoutZoomPath(FromDIP(kMargin), chartWidth);
	layoutNode(root, ancestorRows, FromDIP(kMargin), chartWidth, true);
	updateResetButton();
}

void FlameGraphView::layoutNode(const Database::FlameGraphNode *node, int depth, double x, double width, bool includeNode)
{
	if (!node || node->inclusive <= 0.0)
		return;

	if (includeNode)
	{
		if (isDepthVisible(depth))
		{
			LayoutNode item;
			item.node = node;
			item.depth = depth;
			item.rect = wxRect((int)std::lround(x), 0, std::max(1, (int)std::lround(width)), rowHeight);
			layout.push_back(item);
		}
		depth++;
	}

	if (node->children.empty())
		return;

	double cursor = x;
	for (size_t i = 0; i < node->children.size(); ++i)
	{
		const auto *child = node->children[i].get();
		double next = (i + 1 == node->children.size())
			? (x + width)
			: (cursor + width * (child->inclusive / node->inclusive));

		layoutNode(child, depth, cursor, next - cursor, true);
		cursor = next;
	}
}

void FlameGraphView::layoutZoomPath(double x, double width)
{
	if (zoomPath.size() <= 1)
		return;

	int depth = 0;
	for (size_t i = 0; i + 1 < zoomPath.size(); ++i)
	{
		if (!zoomPath[i]->symbol)
			continue;

		if (!isDepthVisible(depth))
		{
			depth++;
			continue;
		}

		LayoutNode item;
		item.node = zoomPath[i];
		item.depth = depth++;
		item.rect = wxRect((int)std::lround(x), 0, std::max(1, (int)std::lround(width)), rowHeight);
		layout.push_back(item);
	}
}

int FlameGraphView::getSubtreeDepth(const Database::FlameGraphNode *node) const
{
	if (!node)
		return 0;

	int depth = 1;
	for (const auto &child : node->children)
		depth = std::max(depth, 1 + getSubtreeDepth(child.get()));
	return depth;
}

bool FlameGraphView::buildZoomPath(const Database::FlameGraphNode *node,
	const Database::FlameGraphNode *target,
	std::vector<const Database::FlameGraphNode *> &path) const
{
	if (!node)
		return false;

	path.push_back(node);
	if (node == target)
		return true;

	for (const auto &child : node->children)
	{
		if (buildZoomPath(child.get(), target, path))
			return true;
	}

	path.pop_back();
	return false;
}

void FlameGraphView::updateResetButton()
{
	if (!resetZoomButton)
		return;

	const int margin = FromDIP(kMargin);
	const int toolbarHeight = FromDIP(kToolbarHeight);
	const wxSize client = GetClientSize();
	const wxSize buttonSize = resetZoomButton->GetBestSize();
	resetZoomButton->SetSize(client.GetWidth() - buttonSize.GetWidth() - margin,
							 margin,
							 buttonSize.GetWidth(),
							 std::min(buttonSize.GetHeight(), toolbarHeight - margin));
	resetZoomButton->Show(zoomRoot && flameGraph && zoomRoot != flameGraph.get());
}

bool FlameGraphView::isDepthVisible(int depth) const
{
	return depth >= 0 && depth < visibleDepthLimit;
}

const FlameGraphView::LayoutNode *FlameGraphView::hitTest(wxPoint point) const
{
	int chartBottom = GetClientSize().GetHeight() - FromDIP(kMargin);

	for (const auto &item : layout)
	{
		wxRect rect(item.rect);
		rect.y = chartBottom - (item.depth + 1) * rowHeight - item.depth * FromDIP(kBarGap);
		if (rect.Contains(point))
			return &item;
	}

	return NULL;
}

wxColour FlameGraphView::colorForNode(const Database::FlameGraphNode *node) const
{
	if (!node || !node->symbol)
		return wxColour(180, 180, 180);

	const size_t hash = std::hash<std::wstring>{}(node->symbol->procname);
	const int hue = (int)(hash % 64);
	const int r = std::min(255, 220 + hue / 3);
	const int g = std::max(96, 145 - hue / 4);
	const int b = std::max(72, 90 - hue / 6);
	return wxColour(r, g, b);
}

wxString FlameGraphView::makeTooltip(const LayoutNode *node) const
{
	if (!node)
		return wxString();

	double total = flameGraph ? flameGraph->inclusive : 0.0;
	double pct = total > 0.0 ? (node->node->inclusive * 100.0 / total) : 0.0;
	const wchar_t *name = (node->node && node->node->symbol)
		? node->node->symbol->procname.c_str()
		: L"[root]";
	return wxString::Format(
		"%ls\nInclusive: %.2fs (%.2f%%)\nClick to inspect and zoom | Right-click to reset zoom",
		name, node->node->inclusive, pct);
}

void FlameGraphView::activateNode(const LayoutNode *node, bool inspect)
{
	if (!node || !node->node || !node->node->symbol)
		return;

	const Database::AddrInfo *addrinfo = database->getAddrInfo(node->node->address);
	if (inspect)
		theMainWin->inspectSymbol(addrinfo);
	else
		theMainWin->focusSymbol(addrinfo);
}

void FlameGraphView::zoomToNode(const LayoutNode *node)
{
	if (!node)
		return;

	zoomRoot = node->node;
	rebuildLayout();
	Refresh();
}

void FlameGraphView::zoomOut()
{
	if (!flameGraph || !zoomRoot || zoomRoot == flameGraph.get())
		return;

	const Database::FlameGraphNode *target = zoomRoot;
	const Database::FlameGraphNode *parent = NULL;
	std::function<bool(const Database::FlameGraphNode *)> findParent =
		[&](const Database::FlameGraphNode *node) -> bool
		{
			for (const auto &child : node->children)
			{
				if (child.get() == target)
				{
					parent = node;
					return true;
				}
				if (findParent(child.get()))
					return true;
			}
			return false;
		};

	if (findParent(flameGraph.get()))
	{
		zoomRoot = parent;
		rebuildLayout();
		Refresh();
	}
}

void FlameGraphView::resetZoom()
{
	if (!flameGraph)
		return;

	zoomRoot = flameGraph.get();
	rebuildLayout();
	Refresh();
}

void FlameGraphView::OnPaint(wxPaintEvent &WXUNUSED(event))
{
	wxAutoBufferedPaintDC dc(this);
	PrepareDC(dc);

	dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
	dc.Clear();

	if (!flameGraph || flameGraph->inclusive <= 0.0)
	{
		dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
		dc.DrawText("No samples to display for the current filters.", FromDIP(kMargin), FromDIP(kToolbarHeight));
		return;
	}

	if (hiddenDepthCount > 0)
	{
		dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
		dc.DrawText(wxString::Format("%d upper stack %s hidden", hiddenDepthCount, hiddenDepthCount == 1 ? "row" : "rows"),
					FromDIP(kMargin), FromDIP(kToolbarHeight));
	}

	const wxColour textColor = *wxBLACK;
	const int chartBottom = GetClientSize().GetHeight() - FromDIP(kMargin);

	for (const auto &item : layout)
	{
		wxRect rect(item.rect);
		rect.y = chartBottom - (item.depth + 1) * rowHeight - item.depth * FromDIP(kBarGap);

		wxColour fill = colorForNode(item.node);
		if (item.node->symbol == selectedSymbol)
			fill = fill.ChangeLightness(135);
		if (&item == hoveredNode)
			fill = fill.ChangeLightness(112);

		wxPen pen(fill.ChangeLightness(70));
		if (item.node->symbol == selectedSymbol)
		{
			pen = wxPen(*wxBLACK, std::max(2, FromDIP(2)));
		}
		else if (&item == hoveredNode)
		{
			pen = wxPen(fill.ChangeLightness(45));
		}

		dc.SetPen(pen);
		dc.SetBrush(wxBrush(fill));
		dc.DrawRectangle(rect);

		if (item.node->symbol && item.rect.width >= FromDIP(kMinLabelWidth))
		{
			wxString text = item.node->symbol->procname;
			dc.SetTextForeground(textColor);
			dc.DrawLabel(text, rect.Deflate(FromDIP(4), FromDIP(2)),
						 wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxELLIPSIZE_END);
		}
	}
}

void FlameGraphView::OnSize(wxSizeEvent &event)
{
	rebuildLayout();
	Refresh();
	event.Skip();
}

void FlameGraphView::OnMouseMove(wxMouseEvent &event)
{
	const LayoutNode *hit = hitTest(event.GetPosition());
	if (hit != hoveredNode)
	{
		hoveredNode = hit;
		SetToolTip(makeTooltip(hit));
		Refresh();
	}
	event.Skip();
}

void FlameGraphView::OnMouseLeave(wxMouseEvent &event)
{
	if (hoveredNode)
	{
		hoveredNode = NULL;
		UnsetToolTip();
		Refresh();
	}
	event.Skip();
}

void FlameGraphView::OnLeftDown(wxMouseEvent &event)
{
	const LayoutNode *node = hitTest(event.GetPosition());
	if (!node)
		return;

	zoomToNode(node);
	activateNode(node, true);
}

void FlameGraphView::OnRightDown(wxMouseEvent &event)
{
	resetZoom();
	event.Skip();
}

void FlameGraphView::OnResetZoom(wxCommandEvent &WXUNUSED(event))
{
	resetZoom();
}
