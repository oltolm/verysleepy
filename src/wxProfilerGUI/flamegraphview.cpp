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
const int kLabelPadding = 4;
const int kMinLabelWidth = 36;
const int kPreferredBarHeight = 16;
const int kToolbarHeight = 30;
const double kMinZoomScale = 0.25;
const double kMaxZoomScale = 128.0;
const double kZoomStep = 1.5;
const int kResetZoomButton = wxID_HIGHEST + 201;
}

BEGIN_EVENT_TABLE(FlameGraphView, wxScrolledWindow)
EVT_PAINT(FlameGraphView::OnPaint)
EVT_SIZE(FlameGraphView::OnSize)
EVT_MOTION(FlameGraphView::OnMouseMove)
EVT_LEAVE_WINDOW(FlameGraphView::OnMouseLeave)
EVT_LEFT_DOWN(FlameGraphView::OnLeftDown)
EVT_RIGHT_DOWN(FlameGraphView::OnRightDown)
EVT_MOUSEWHEEL(FlameGraphView::OnMouseWheel)
EVT_BUTTON(kResetZoomButton, FlameGraphView::OnResetZoom)
END_EVENT_TABLE()

FlameGraphView::FlameGraphView(wxWindow *parent, Database *database_)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME),
	  database(database_),
	  resetZoomButton(NULL),
	  pendingInspectAddrInfo(NULL),
	  inspectScheduled(false),
	  selectedSymbol(NULL),
	  hoveredNode(NULL),
	  zoomRoot(NULL),
	  maxDepth(0),
	  chartDirty(true),
	  pendingInitialSnap(true),
	  rowHeight(FromDIP(kPreferredBarHeight)),
	  zoomScale(1.0),
	  layoutWidth(0),
	  chartHeight(0)
{
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetScrollRate(FromDIP(16), FromDIP(16));

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
	viewHistory.clear();
	pendingInspectAddrInfo = NULL;
	inspectScheduled = false;
	selectedSymbol = NULL;
	hoveredNode = NULL;
	zoomRoot = NULL;
	zoomPath.clear();
	maxDepth = 0;
	chartDirty = true;
	pendingInitialSnap = true;
	zoomScale = 1.0;
	layoutWidth = 0;
	chartHeight = 0;
	SetVirtualSize(GetClientSize());
	Scroll(0, 0);
	updateResetButton();
	Refresh();
}

void FlameGraphView::snapToBottomLeftIfPending()
{
	if (!pendingInitialSnap)
		return;

	const wxSize client = GetClientSize();
	const wxSize virtualSize = GetVirtualSize();
	const int maxY = std::max(0, virtualSize.GetHeight() - client.GetHeight());
	scrollToPixelPosition(0, maxY);
	pendingInitialSnap = false;
}

void FlameGraphView::rebuildChart()
{
	flameGraph = database->buildFlameGraph();
	zoomRoot = flameGraph.get();
	zoomPath.clear();
	viewHistory.clear();
	pendingInitialSnap = true;
	rebuildLayout();
}

void FlameGraphView::rebuildLayout()
{
	layout.clear();
	hoveredNode = NULL;
	maxDepth = 0;
	layoutWidth = 0;
	chartHeight = 0;

	if (!flameGraph || flameGraph->inclusive <= 0.0)
	{
		updateVirtualSize();
		updateResetButton();
		return;
	}

	const Database::FlameGraphNode *root = zoomRoot ? zoomRoot : flameGraph.get();
	zoomPath.clear();
	buildZoomPath(flameGraph.get(), root, zoomPath);

	int ancestorRows = 0;
	for (size_t i = 0; i + 1 < zoomPath.size(); ++i)
	{
		if (zoomPath[i]->symbol)
			ancestorRows++;
	}

	const int subtreeRows = std::max(1, getSubtreeDepth(root));
	const int totalRows = ancestorRows + subtreeRows;
	rowHeight = FromDIP(kPreferredBarHeight);
	maxDepth = totalRows;

	const int margin = FromDIP(kMargin);
	const int requiredChartHeight = FromDIP(kToolbarHeight) + 2 * margin + std::max(1, totalRows) * rowHeight +
		std::max(0, totalRows - 1) * FromDIP(kBarGap);
	const int clientLogicalHeight = std::max(1, (int)std::ceil(GetClientSize().GetHeight() /
		std::max(zoomScale, kMinZoomScale)));
	chartHeight = std::max(requiredChartHeight, clientLogicalHeight);
	layoutWidth = std::max(1, GetClientSize().GetWidth() - 2 * margin);

	layoutZoomPath(margin, layoutWidth);
	layoutNode(root, ancestorRows, margin, layoutWidth, true);
	updateVirtualSize();
	updateResetButton();
}

void FlameGraphView::updateVirtualSize()
{
	const wxSize client = GetClientSize();
	SetVirtualSize(std::max(client.GetWidth(), logicalToVirtualX(layoutWidth + 2 * FromDIP(kMargin))),
		std::max(client.GetHeight(), logicalToVirtualY(chartHeight)));
}

void FlameGraphView::layoutNode(const Database::FlameGraphNode *node, int depth, double x, double width, bool includeNode)
{
	if (!node || node->inclusive <= 0.0)
		return;

	if (includeNode)
	{
		LayoutNode item;
		item.node = node;
		item.depth = depth;
		item.rect = wxRect((int)std::lround(x),
			chartHeight - FromDIP(kMargin) - (depth + 1) * rowHeight - depth * FromDIP(kBarGap),
			std::max(1, (int)std::lround(width)),
			rowHeight);
		layout.push_back(item);
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

		LayoutNode item;
		item.node = zoomPath[i];
		item.depth = depth++;
		item.rect = wxRect((int)std::lround(x),
			chartHeight - FromDIP(kMargin) - (item.depth + 1) * rowHeight - item.depth * FromDIP(kBarGap),
			std::max(1, (int)std::lround(width)),
			rowHeight);
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

void FlameGraphView::scheduleInspect(const Database::AddrInfo *addrinfo)
{
	pendingInspectAddrInfo = addrinfo;
	if (inspectScheduled || !addrinfo)
		return;

	inspectScheduled = true;
	CallAfter([this]()
		{
			inspectScheduled = false;
			const Database::AddrInfo *addrinfoToInspect = pendingInspectAddrInfo;
			pendingInspectAddrInfo = NULL;
			if (addrinfoToInspect)
				theMainWin->inspectSymbol(addrinfoToInspect);
		});
}

wxPoint FlameGraphView::toLogicalPoint(wxPoint point) const
{
	int x = 0;
	int y = 0;
	CalcUnscrolledPosition(point.x, point.y, &x, &y);
	const double scale = std::max(zoomScale, kMinZoomScale);
	return wxPoint((int)std::lround(x / scale), (int)std::lround(y / scale));
}

int FlameGraphView::logicalToVirtualX(int logicalX) const
{
	return (int)std::lround(logicalX * zoomScale);
}

int FlameGraphView::logicalToVirtualY(int logicalY) const
{
	return (int)std::lround(logicalY * zoomScale);
}

wxRect FlameGraphView::scaleRect(const wxRect &rect) const
{
	const int left = logicalToVirtualX(rect.x);
	const int right = logicalToVirtualX(rect.x + rect.width);
	const int top = logicalToVirtualY(rect.y);
	const int bottom = logicalToVirtualY(rect.y + rect.height);
	return wxRect(left, top, std::max(1, right - left), std::max(1, bottom - top));
}

void FlameGraphView::scrollToPixelPosition(int x, int y)
{
	int pixelsPerUnitX = 0;
	int pixelsPerUnitY = 0;
	GetScrollPixelsPerUnit(&pixelsPerUnitX, &pixelsPerUnitY);

	const int scrollX = pixelsPerUnitX > 0 ? x / pixelsPerUnitX : x;
	const int scrollY = pixelsPerUnitY > 0 ? y / pixelsPerUnitY : y;
	Scroll(std::max(0, scrollX), std::max(0, scrollY));
}

const FlameGraphView::LayoutNode *FlameGraphView::hitTest(wxPoint point) const
{
	const wxPoint logicalPoint = toLogicalPoint(point);
	for (const auto &item : layout)
	{
		if (item.rect.Contains(logicalPoint))
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
		"%ls\nInclusive: %.2fs (%.2f%%)\nClick to inspect and zoom | Right-click to reset zoom | Ctrl+Wheel to resize",
		name, node->node->inclusive, pct);
}

void FlameGraphView::zoomToNode(const LayoutNode *node)
{
	if (!node)
		return;

	zoomRoot = node->node;
	pendingInitialSnap = true;
	rebuildLayout();
	snapToBottomLeftIfPending();
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

	viewHistory.clear();
	zoomRoot = flameGraph.get();
	zoomScale = 1.0;
	pendingInitialSnap = true;
	rebuildLayout();
	snapToBottomLeftIfPending();
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

	int viewX = 0;
	int viewY = 0;
	GetViewStart(&viewX, &viewY);
	int scrollX = 0;
	int scrollY = 0;
	GetScrollPixelsPerUnit(&scrollX, &scrollY);
	const double scale = std::max(zoomScale, kMinZoomScale);
	const wxRect visibleRect(
		(int)std::floor((viewX * scrollX) / scale),
		(int)std::floor((viewY * scrollY) / scale),
		(int)std::ceil(GetClientSize().GetWidth() / scale),
		(int)std::ceil(GetClientSize().GetHeight() / scale));
	const wxColour textColor = *wxBLACK;

	dc.SetUserScale(scale, scale);

	for (const auto &item : layout)
	{
		if (!item.rect.Intersects(visibleRect))
			continue;

		wxColour fill = colorForNode(item.node);
		if (item.node->symbol == selectedSymbol)
			fill = fill.ChangeLightness(135);
		if (&item == hoveredNode)
			fill = fill.ChangeLightness(112);

		wxPen pen(fill.ChangeLightness(70));
		if (item.node->symbol == selectedSymbol)
			pen = wxPen(*wxBLACK, std::max(2, FromDIP(2)));
		else if (&item == hoveredNode)
			pen = wxPen(fill.ChangeLightness(45));

		dc.SetPen(pen);
		dc.SetBrush(wxBrush(fill));
		dc.DrawRectangle(item.rect);

		if (item.node->symbol && item.rect.width * scale >= FromDIP(kMinLabelWidth))
		{
			dc.SetTextForeground(textColor);
			dc.DrawLabel(item.node->symbol->procname,
				item.rect.Deflate(FromDIP(kLabelPadding), FromDIP(2)),
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

	const Database::AddrInfo *addrinfo = NULL;
	if (node->node && node->node->symbol)
	{
		selectedSymbol = node->node->symbol;
		addrinfo = database->getAddrInfo(node->node->address);
	}

	viewHistory.push_back(ViewState{zoomRoot, zoomScale, GetScrollPos(wxHORIZONTAL), GetScrollPos(wxVERTICAL)});
	zoomToNode(node);
	Update();
	scheduleInspect(addrinfo);
}

void FlameGraphView::OnRightDown(wxMouseEvent &event)
{
	resetZoom();
	event.Skip();
}

void FlameGraphView::OnMouseWheel(wxMouseEvent &event)
{
	if (!event.ControlDown() || !flameGraph || flameGraph->inclusive <= 0.0)
	{
		event.Skip();
		return;
	}

	const int rotation = event.GetWheelRotation();
	if (rotation == 0)
		return;

	const double previousZoom = zoomScale;
	const double factor = rotation > 0 ? kZoomStep : (1.0 / kZoomStep);
	zoomScale = std::max(kMinZoomScale, std::min(kMaxZoomScale, zoomScale * factor));
	if (std::abs(zoomScale - previousZoom) < 1e-9)
		return;

	const wxPoint cursor = event.GetPosition();
	const wxPoint before = toLogicalPoint(cursor);

	updateVirtualSize();
	const int targetX = logicalToVirtualX(before.x) - cursor.x;
	const int targetY = logicalToVirtualY(before.y) - cursor.y;
	scrollToPixelPosition(targetX, targetY);
	Refresh();
}

void FlameGraphView::OnResetZoom(wxCommandEvent &WXUNUSED(event))
{
	resetZoom();
}
