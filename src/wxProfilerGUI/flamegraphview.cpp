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

http://www.gnu.org/copyleft/gpl.html
=====================================================================*/

#include "flamegraphview.h"

#include "mainwin.h"
#include <algorithm>
#include <cmath>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace
{
const int kBarGap = 2;
const int kMargin = 8;
const int kLabelPadding = 4;
const int kMinLabelWidth = 36;
const int kMinVisibleBarWidth = 12;
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
EVT_CHAR_HOOK(FlameGraphView::OnCharHook)
EVT_MOTION(FlameGraphView::OnMouseMove)
EVT_LEAVE_WINDOW(FlameGraphView::OnMouseLeave)
EVT_LEFT_DOWN(FlameGraphView::OnLeftDown)
EVT_RIGHT_DOWN(FlameGraphView::OnRightDown)
EVT_MOUSEWHEEL(FlameGraphView::OnMouseWheel)
EVT_BUTTON(kResetZoomButton, FlameGraphView::OnResetZoom)
END_EVENT_TABLE()

FlameGraphView::FlameGraphView(wxWindow *parent, Database *database_)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME | wxWANTS_CHARS),
	  database(database_),
	  resetZoomButton(NULL),
	  pendingInspectAddrInfo(NULL),
	  inspectScheduled(false),
	  selectedSymbol(NULL),
	  hoveredNode(NULL),
	  activeNode(NULL),
	  zoomRoot(NULL),
	  chartDirty(true),
	  pendingInitialSnap(true),
	  rowHeight(FromDIP(kPreferredBarHeight)),
	  zoomScale(1.0),
	  activeLayoutRoot(NULL),
	  cachedLayoutWidth(0),
	  layoutWidth(0),
	  chartHeight(0),
	  verticalOffset(0)
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
	ensureActiveLayout();
	if (!activeNode)
		setActiveNode(findDefaultActiveNode());
	Refresh();
}

void FlameGraphView::reset()
{
	flameGraph.reset();
	activeLayout.reset();
	layoutCache.clear();
	nodeMetadata.clear();
	pendingInspectAddrInfo = NULL;
	inspectScheduled = false;
	selectedSymbol = NULL;
	hoveredNode = NULL;
	activeNode = NULL;
	zoomRoot = NULL;
	chartDirty = true;
	pendingInitialSnap = true;
	zoomScale = 1.0;
	activeLayoutRoot = NULL;
	cachedLayoutWidth = 0;
	layoutWidth = 0;
	chartHeight = 0;
	verticalOffset = 0;
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
	pendingInitialSnap = true;
	nodeMetadata.clear();
	if (flameGraph)
		cacheNodeMetadata(flameGraph.get(), NULL);
	invalidateLayoutCache();
}

void FlameGraphView::invalidateLayoutCache()
{
	activeLayout.reset();
	layoutCache.clear();
	activeLayoutRoot = NULL;
	cachedLayoutWidth = 0;
	hoveredNode = NULL;
	activeNode = NULL;
	layoutWidth = 0;
	chartHeight = 0;
	verticalOffset = 0;
}

void FlameGraphView::ensureActiveLayout()
{
	hoveredNode = NULL;

	const Database::FlameGraphNode *root = zoomRoot ? zoomRoot : flameGraph.get();
	const int width = std::max(1, GetClientSize().GetWidth() - 2 * FromDIP(kMargin));
	if (width != cachedLayoutWidth)
	{
		activeLayout.reset();
		layoutCache.clear();
		activeLayoutRoot = NULL;
		cachedLayoutWidth = width;
	}

	if (flameGraph && flameGraph->inclusive > 0.0 && root)
	{
		if (!activeLayout || activeLayoutRoot != root)
		{
			auto it = layoutCache.find(root);
			if (it == layoutCache.end())
			{
				auto cached = std::make_shared<CachedLayout>();
				buildCachedLayout(*cached, root, width);
				it = layoutCache.emplace(root, cached).first;
			}

			activeLayout = it->second;
			activeLayoutRoot = root;
		}
	}
	else
	{
		activeLayout.reset();
		activeLayoutRoot = NULL;
	}

	updateViewportMetrics();
	updateResetButton();
	if (activeNode)
		activeNode = findLayoutNode(activeNode->node);
	if (activeNode && !isLayoutNodeVisible(activeNode))
		activeNode = NULL;
	if (!activeNode)
		activeNode = findDefaultActiveNode();
}

void FlameGraphView::updateViewportMetrics()
{
	layoutWidth = activeLayout ? activeLayout->layoutWidth : cachedLayoutWidth;
	const int clientLogicalHeight = std::max(1, (int)std::ceil(GetClientSize().GetHeight() /
		std::max(zoomScale, kMinZoomScale)));
	const int baseChartHeight = activeLayout ? activeLayout->baseChartHeight : 0;
	chartHeight = std::max(baseChartHeight, clientLogicalHeight);
	verticalOffset = std::max(0, chartHeight - baseChartHeight);
	updateVirtualSize();
}

void FlameGraphView::buildCachedLayout(CachedLayout &cached, const Database::FlameGraphNode *root, int width) const
{
	cached.layout.clear();
	cached.layoutRows.clear();
	cached.zoomPath.clear();
	cached.layoutWidth = width;

	buildZoomPath(root, cached.zoomPath);

	int ancestorRows = 0;
	for (size_t i = 0; i + 1 < cached.zoomPath.size(); ++i)
	{
		if (cached.zoomPath[i]->symbol)
			ancestorRows++;
	}

	const auto rootMetadata = nodeMetadata.find(root);
	const int subtreeRows = std::max(1, rootMetadata != nodeMetadata.end() ? rootMetadata->second.subtreeDepth : 1);
	const int totalRows = ancestorRows + subtreeRows;
	const int margin = FromDIP(kMargin);
	const int barGap = FromDIP(kBarGap);
	cached.maxDepth = totalRows;
	cached.baseChartHeight = FromDIP(kToolbarHeight) + 2 * margin + std::max(1, totalRows) * rowHeight +
		std::max(0, totalRows - 1) * barGap;

	layoutZoomPath(cached, margin, cached.layoutWidth);
	layoutNode(cached, root, ancestorRows, margin, cached.layoutWidth, true);
	rebuildLayoutRows(cached);
}

void FlameGraphView::rebuildLayoutRows(CachedLayout &cached) const
{
	cached.layoutRows.clear();
	cached.layoutRows.resize(std::max(0, cached.maxDepth));

	for (size_t i = 0; i < cached.layout.size(); ++i)
	{
		const LayoutNode &item = cached.layout[i];
		if (item.depth >= 0 && item.depth < (int)cached.layoutRows.size())
			cached.layoutRows[item.depth].push_back(i);
	}
}

void FlameGraphView::updateVirtualSize()
{
	const wxSize client = GetClientSize();
	SetVirtualSize(std::max(client.GetWidth(), logicalToVirtualX(layoutWidth + 2 * FromDIP(kMargin))),
		std::max(client.GetHeight(), logicalToVirtualY(chartHeight)));
}

void FlameGraphView::buildZoomPath(const Database::FlameGraphNode *target,
	std::vector<const Database::FlameGraphNode *> &path) const
{
	path.clear();

	for (const Database::FlameGraphNode *node = target; node; )
	{
		path.push_back(node);
		const auto it = nodeMetadata.find(node);
		node = it != nodeMetadata.end() ? it->second.parent : NULL;
	}

	std::reverse(path.begin(), path.end());
}

void FlameGraphView::layoutNode(CachedLayout &cached,
	const Database::FlameGraphNode *node,
	int depth,
	double x,
	double width,
	bool includeNode) const
{
	if (!node || node->inclusive <= 0.0)
		return;

	if (includeNode)
	{
		LayoutNode item;
		item.node = node;
		item.depth = depth;
		item.rect = wxRect((int)std::lround(x),
			cached.baseChartHeight - FromDIP(kMargin) - (depth + 1) * rowHeight - depth * FromDIP(kBarGap),
			std::max(1, (int)std::lround(width)),
			rowHeight);
		cached.layout.push_back(item);
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

		layoutNode(cached, child, depth, cursor, next - cursor, true);
		cursor = next;
	}
}

void FlameGraphView::layoutZoomPath(CachedLayout &cached, double x, double width) const
{
	if (cached.zoomPath.size() <= 1)
		return;

	int depth = 0;
	for (size_t i = 0; i + 1 < cached.zoomPath.size(); ++i)
	{
		if (!cached.zoomPath[i]->symbol)
			continue;

		LayoutNode item;
		item.node = cached.zoomPath[i];
		item.depth = depth++;
		item.rect = wxRect((int)std::lround(x),
			cached.baseChartHeight - FromDIP(kMargin) - (item.depth + 1) * rowHeight - item.depth * FromDIP(kBarGap),
			std::max(1, (int)std::lround(width)),
			rowHeight);
		cached.layout.push_back(item);
	}
}

int FlameGraphView::cacheNodeMetadata(const Database::FlameGraphNode *node, const Database::FlameGraphNode *parent)
{
	if (!node)
		return 0;

	int depth = 1;
	for (const auto &child : node->children)
		depth = std::max(depth, 1 + cacheNodeMetadata(child.get(), node));

	nodeMetadata[node] = NodeMetadata{parent, depth};
	return depth;
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

wxRect FlameGraphView::offsetRect(const wxRect &rect) const
{
	return wxRect(rect.x, rect.y + verticalOffset, rect.width, rect.height);
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
	if (!activeLayout)
		return NULL;

	const wxPoint logicalPoint = toLogicalPoint(point);
	const int margin = FromDIP(kMargin);
	const int barGap = FromDIP(kBarGap);
	const int slotHeight = rowHeight + barGap;
	const int fromBottom = chartHeight - margin - 1 - logicalPoint.y;
	if (fromBottom < 0 || slotHeight <= 0)
		return NULL;

	const int depth = fromBottom / slotHeight;
	if (depth < 0 || depth >= (int)activeLayout->layoutRows.size())
		return NULL;

	const std::vector<size_t> &row = activeLayout->layoutRows[depth];
	if (row.empty())
		return NULL;

	size_t left = 0;
	size_t right = row.size();
	while (left < right)
	{
		const size_t mid = left + (right - left) / 2;
		const LayoutNode &candidate = activeLayout->layout[row[mid]];
		const wxRect rect = offsetRect(candidate.rect);
		if (logicalPoint.x < rect.x)
		{
			right = mid;
		}
		else if (logicalPoint.x >= rect.x + rect.width)
		{
			left = mid + 1;
		}
		else
		{
			return rect.Contains(logicalPoint) && isLayoutNodeVisible(&candidate) ? &candidate : NULL;
		}
	}

	return NULL;
}

const FlameGraphView::LayoutNode *FlameGraphView::findLayoutNode(const Database::FlameGraphNode *node) const
{
	if (!activeLayout || !node)
		return NULL;

	for (const auto &item : activeLayout->layout)
	{
		if (item.node == node)
			return &item;
	}

	return NULL;
}

const FlameGraphView::LayoutNode *FlameGraphView::findDefaultActiveNode() const
{
	if (!activeLayout)
		return NULL;

	if (selectedSymbol)
	{
		for (const auto &item : activeLayout->layout)
		{
			if (item.node && item.node->symbol == selectedSymbol && isLayoutNodeVisible(&item))
				return &item;
		}
	}

	for (const auto &item : activeLayout->layout)
	{
		if (item.node && item.node->symbol && isLayoutNodeVisible(&item))
			return &item;
	}

	return activeLayout->layout.empty() ? NULL : &activeLayout->layout.front();
}

const FlameGraphView::LayoutNode *FlameGraphView::findParentNode(const LayoutNode *node) const
{
	if (!node || !node->node)
		return NULL;

	for (const Database::FlameGraphNode *current = node->node; current; )
	{
		auto it = nodeMetadata.find(current);
		if (it == nodeMetadata.end() || !it->second.parent)
			return NULL;

		current = it->second.parent;
		const LayoutNode *candidate = findLayoutNode(current);
		if (candidate && isLayoutNodeVisible(candidate))
			return candidate;
	}

	return NULL;
}

const FlameGraphView::LayoutNode *FlameGraphView::findChildNode(const LayoutNode *node) const
{
	if (!activeLayout || !node)
		return NULL;

	const wxRect rect = offsetRect(node->rect);
	const int centerX = rect.x + rect.width / 2;
	const LayoutNode *best = NULL;
	int bestDistance = 0;
	for (const auto &candidate : activeLayout->layout)
	{
		if (candidate.depth <= node->depth || !isLayoutNodeVisible(&candidate))
			continue;

		if (!candidate.node || candidate.node == node->node)
			continue;

		if (!isDescendantOf(candidate.node, node->node))
			continue;

		const wxRect candidateRect = offsetRect(candidate.rect);
		if (candidateRect.y >= rect.y)
			continue;

		if (candidateRect.x <= centerX && centerX < candidateRect.x + candidateRect.width)
		{
			if (!best || candidate.depth < best->depth)
				best = &candidate;
			continue;
		}

		const int candidateCenter = candidateRect.x + candidateRect.width / 2;
		const int distance = std::abs(candidateCenter - centerX);
		if (!best || best->depth > candidate.depth ||
			(best->depth == candidate.depth &&
				(distance < bestDistance ||
					(distance == bestDistance && candidateRect.width > best->rect.width))))
		{
			best = &candidate;
			bestDistance = distance;
		}
	}

	return best;
}

const FlameGraphView::LayoutNode *FlameGraphView::findSiblingNode(const LayoutNode *node, int direction) const
{
	if (!activeLayout || !node || direction == 0 || node->depth < 0 ||
		node->depth >= (int)activeLayout->layoutRows.size())
		return NULL;

	const std::vector<size_t> &row = activeLayout->layoutRows[node->depth];
	const wxRect currentRect = offsetRect(node->rect);
	const int currentCenter = currentRect.x + currentRect.width / 2;
	const LayoutNode *best = NULL;
	int bestDistance = 0;
	for (size_t i = 0; i < row.size(); ++i)
	{
		const LayoutNode &candidate = activeLayout->layout[row[i]];
		if (&candidate == node || !isLayoutNodeVisible(&candidate))
			continue;

		const wxRect candidateRect = offsetRect(candidate.rect);
		const int candidateCenter = candidateRect.x + candidateRect.width / 2;
		const int delta = candidateCenter - currentCenter;
		if ((direction < 0 && delta >= 0) || (direction > 0 && delta <= 0))
			continue;

		const int distance = std::abs(delta);
		if (!best || distance < bestDistance ||
			(distance == bestDistance && candidateRect.width > best->rect.width))
		{
			best = &candidate;
			bestDistance = distance;
		}
	}

	return best;
}

bool FlameGraphView::isDescendantOf(const Database::FlameGraphNode *node,
	const Database::FlameGraphNode *ancestor) const
{
	if (!node || !ancestor || node == ancestor)
		return false;

	for (const Database::FlameGraphNode *current = node; current; )
	{
		auto it = nodeMetadata.find(current);
		if (it == nodeMetadata.end() || !it->second.parent)
			return false;

		current = it->second.parent;
		if (current == ancestor)
			return true;
	}

	return false;
}

bool FlameGraphView::isLayoutNodeVisible(const LayoutNode *node) const
{
	if (!node)
		return false;

	if (!node->node || !node->node->symbol)
		return true;

	return node->rect.width * std::max(zoomScale, kMinZoomScale) >= FromDIP(kMinVisibleBarWidth);
}

void FlameGraphView::setActiveNode(const LayoutNode *node,
	bool preserveHorizontalPosition,
	bool preserveVerticalPosition)
{
	activeNode = node;
	selectedSymbol = activeNode && activeNode->node ? activeNode->node->symbol : NULL;
	ensureActiveNodeVisible(preserveHorizontalPosition, preserveVerticalPosition);
	Refresh();
}

void FlameGraphView::ensureActiveNodeVisible(bool preserveHorizontalPosition,
	bool preserveVerticalPosition)
{
	if (!activeNode)
		return;

	const wxRect rect = offsetRect(activeNode->rect);
	int viewX = 0;
	int viewY = 0;
	GetViewStart(&viewX, &viewY);
	int scrollX = 0;
	int scrollY = 0;
	GetScrollPixelsPerUnit(&scrollX, &scrollY);
	const double scale = std::max(zoomScale, kMinZoomScale);
	const int currentPixelX = viewX * scrollX;
	const int currentPixelY = viewY * scrollY;
	const int currentLeft = (int)std::floor(currentPixelX / scale);
	const int currentTop = (int)std::floor(currentPixelY / scale);
	const int viewportWidth = (int)std::ceil(GetClientSize().GetWidth() / scale);
	const int viewportHeight = (int)std::ceil(GetClientSize().GetHeight() / scale);
	const int currentRight = currentLeft + viewportWidth;
	const int currentBottom = currentTop + viewportHeight;
	int targetPixelX = currentPixelX;
	int targetPixelY = currentPixelY;
	if (!preserveHorizontalPosition)
	{
		int targetX = currentLeft;
		if (rect.x < currentLeft)
			targetX = rect.x;
		else if (rect.x + rect.width > currentRight)
			targetX = rect.x + rect.width - viewportWidth;
		targetPixelX = logicalToVirtualX(targetX);
	}
	if (!preserveVerticalPosition)
	{
		int targetY = currentTop;
		if (rect.y < currentTop)
			targetY = rect.y;
		else if (rect.y + rect.height > currentBottom)
			targetY = rect.y + rect.height - viewportHeight;
		targetPixelY = logicalToVirtualY(targetY);
	}

	scrollToPixelPosition(targetPixelX, targetPixelY);
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

	if (node->node == zoomRoot)
	{
		setActiveNode(node, true, true);
		return;
	}

	const Database::FlameGraphNode *focusNode = node->node;
	zoomRoot = node->node;
	activeNode = NULL;
	ensureActiveLayout();
	const LayoutNode *target = findLayoutNode(focusNode);
	if (!target || !isLayoutNodeVisible(target))
		target = findDefaultActiveNode();
	setActiveNode(target, true, false);
}

void FlameGraphView::resetZoom(bool resetScale)
{
	if (!flameGraph)
		return;

	const Database::FlameGraphNode *focusNode = activeNode && activeNode->node
		? activeNode->node
		: zoomRoot;

	zoomRoot = flameGraph.get();
	activeNode = NULL;
	if (resetScale)
		zoomScale = 1.0;
	pendingInitialSnap = resetScale;
	ensureActiveLayout();
	if (resetScale)
		snapToBottomLeftIfPending();
	const LayoutNode *target = findLayoutNode(focusNode);
	if (!target || !isLayoutNodeVisible(target))
		target = findDefaultActiveNode();
	setActiveNode(target);
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

	if (!activeLayout)
		return;

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
	const wxColour activeOutline(0, 120, 215);
	const wxColour activeFill(235, 244, 255);

	dc.SetUserScale(scale, scale);

	for (const auto &item : activeLayout->layout)
	{
		if (!isLayoutNodeVisible(&item))
			continue;

		const wxRect rect = offsetRect(item.rect);
		if (!rect.Intersects(visibleRect))
			continue;

		wxColour fill = colorForNode(item.node);
		if (item.node->symbol == selectedSymbol)
			fill = fill.ChangeLightness(135);
		if (&item == hoveredNode)
			fill = fill.ChangeLightness(112);
		if (&item == activeNode)
			fill = activeFill;

		wxPen pen(fill.ChangeLightness(70));
		if (&item == activeNode)
			pen = wxPen(activeOutline, std::max(3, FromDIP(3)));
		else if (item.node->symbol == selectedSymbol)
			pen = wxPen(wxColour(90, 90, 90), std::max(2, FromDIP(2)));
		else if (&item == hoveredNode)
			pen = wxPen(fill.ChangeLightness(45));

		dc.SetPen(pen);
		dc.SetBrush(wxBrush(fill));
		dc.DrawRectangle(rect);
		if (&item == activeNode && rect.width > 6 && rect.height > 6)
		{
			wxRect innerRect(rect);
			innerRect.Deflate(1);
			dc.SetPen(wxPen(*wxWHITE, std::max(1, FromDIP(1))));
			dc.SetBrush(*wxTRANSPARENT_BRUSH);
			dc.DrawRectangle(innerRect);
		}

		if (item.node->symbol && rect.width * scale >= FromDIP(kMinLabelWidth))
		{
			dc.SetTextForeground(textColor);
			dc.DrawLabel(item.node->symbol->procname,
				rect.Deflate(FromDIP(kLabelPadding), FromDIP(2)),
				wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxELLIPSIZE_END);
		}
	}
}

void FlameGraphView::OnSize(wxSizeEvent &event)
{
	ensureActiveLayout();
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
	SetFocus();
	const LayoutNode *node = hitTest(event.GetPosition());
	if (!node)
		return;

	const Database::AddrInfo *addrinfo = NULL;
	if (node->node && node->node->symbol)
		addrinfo = database->getAddrInfo(node->node->address);

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
	const LayoutNode *cursorNode = hoveredNode ? hoveredNode : hitTest(cursor);
	const Database::FlameGraphNode *focusNode = cursorNode && cursorNode->node
		? cursorNode->node
		: (activeNode && activeNode->node ? activeNode->node : NULL);
	const wxPoint before = toLogicalPoint(cursor);

	updateViewportMetrics();
	const int targetX = logicalToVirtualX(before.x) - cursor.x;
	const int targetY = logicalToVirtualY(before.y) - cursor.y;
	scrollToPixelPosition(targetX, targetY);

	if (focusNode)
	{
		const LayoutNode *target = findLayoutNode(focusNode);
		if (target && isLayoutNodeVisible(target))
		{
			activeNode = target;
			selectedSymbol = target->node ? target->node->symbol : NULL;
		}
		else
		{
			activeNode = findDefaultActiveNode();
			selectedSymbol = activeNode && activeNode->node ? activeNode->node->symbol : NULL;
		}
	}
	else if (activeNode && !isLayoutNodeVisible(activeNode))
	{
		activeNode = findDefaultActiveNode();
		selectedSymbol = activeNode && activeNode->node ? activeNode->node->symbol : NULL;
	}

	Refresh();
}

void FlameGraphView::OnCharHook(wxKeyEvent &event)
{
	if (!activeLayout || !flameGraph || flameGraph->inclusive <= 0.0)
	{
		event.Skip();
		return;
	}

	const LayoutNode *target = NULL;
	switch (event.GetKeyCode())
	{
	case WXK_LEFT:
		target = activeNode ? findSiblingNode(activeNode, -1) : findDefaultActiveNode();
		break;
	case WXK_RIGHT:
		target = activeNode ? findSiblingNode(activeNode, 1) : findDefaultActiveNode();
		break;
	case WXK_UP:
		target = activeNode ? findChildNode(activeNode) : findDefaultActiveNode();
		break;
	case WXK_DOWN:
		target = activeNode ? findParentNode(activeNode) : findDefaultActiveNode();
		break;
	case WXK_RETURN:
	case WXK_NUMPAD_ENTER:
		if (activeNode)
		{
			zoomToNode(activeNode);
			if (activeNode && activeNode->node && activeNode->node->symbol)
				scheduleInspect(database->getAddrInfo(activeNode->node->address));
		}
		return;
	case WXK_ESCAPE:
		resetZoom(false);
		return;
	default:
		event.Skip();
		return;
	}

	if (target)
		setActiveNode(target,
			event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN,
			event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT);
}

void FlameGraphView::OnResetZoom(wxCommandEvent &WXUNUSED(event))
{
	resetZoom();
}
