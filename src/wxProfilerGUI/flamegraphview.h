/*=====================================================================
flamegraphview.h
----------------

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
#pragma once

#include "database.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <wx/scrolwin.h>

class wxButton;

class FlameGraphView : public wxScrolledWindow
{
public:
	FlameGraphView(wxWindow *parent, Database *database);
	virtual ~FlameGraphView();

	void showChart(const Database::Symbol *selectedSymbol);
	void reset();
	void snapToBottomLeftIfPending();

private:
	struct LayoutNode
	{
		const Database::FlameGraphNode *node;
		wxRect rect;
		int depth;
	};

	struct NodeMetadata
	{
		const Database::FlameGraphNode *parent;
		int subtreeDepth;
	};

	struct CachedLayout
	{
		std::vector<LayoutNode> layout;
		std::vector<std::vector<size_t> > layoutRows;
		std::vector<const Database::FlameGraphNode *> zoomPath;
		int maxDepth;
		int baseChartHeight;
		int layoutWidth;
	};

	Database *database;
	wxButton *resetZoomButton;
	std::unique_ptr<Database::FlameGraphNode> flameGraph;
	std::shared_ptr<const CachedLayout> activeLayout;
	std::unordered_map<const Database::FlameGraphNode *, std::shared_ptr<const CachedLayout> > layoutCache;
	std::unordered_map<const Database::FlameGraphNode *, NodeMetadata> nodeMetadata;
	const Database::AddrInfo *pendingInspectAddrInfo;
	bool inspectScheduled;
	const Database::Symbol *selectedSymbol;
	const LayoutNode *hoveredNode;
	const LayoutNode *activeNode;
	const Database::FlameGraphNode *zoomRoot;
	bool chartDirty;
	bool pendingInitialSnap;
	int rowHeight;
	double zoomScale;
	const Database::FlameGraphNode *activeLayoutRoot;
	int cachedLayoutWidth;
	int layoutWidth;
	int chartHeight;
	int verticalOffset;

	void rebuildChart();
	void invalidateLayoutCache();
	void ensureActiveLayout();
	void updateViewportMetrics();
	void updateVirtualSize();
	void buildCachedLayout(CachedLayout &cached, const Database::FlameGraphNode *root, int width) const;
	void rebuildLayoutRows(CachedLayout &cached) const;
	void buildZoomPath(const Database::FlameGraphNode *target,
		std::vector<const Database::FlameGraphNode *> &path) const;
	void layoutNode(CachedLayout &cached,
		const Database::FlameGraphNode *node,
		int depth,
		double x,
		double width,
		bool includeNode) const;
	void layoutZoomPath(CachedLayout &cached, double x, double width) const;
	int cacheNodeMetadata(const Database::FlameGraphNode *node, const Database::FlameGraphNode *parent);
	void updateResetButton();
	wxPoint toLogicalPoint(wxPoint point) const;
	int logicalToVirtualX(int logicalX) const;
	int logicalToVirtualY(int logicalY) const;
	wxRect offsetRect(const wxRect &rect) const;
	void scrollToPixelPosition(int x, int y);
	void scheduleInspect(const Database::AddrInfo *addrinfo);
	const LayoutNode *hitTest(wxPoint point) const;
	const LayoutNode *findLayoutNode(const Database::FlameGraphNode *node) const;
	const LayoutNode *findDefaultActiveNode() const;
	const LayoutNode *findParentNode(const LayoutNode *node) const;
	const LayoutNode *findChildNode(const LayoutNode *node) const;
	const LayoutNode *findSiblingNode(const LayoutNode *node, int direction) const;
	bool isDescendantOf(const Database::FlameGraphNode *node, const Database::FlameGraphNode *ancestor) const;
	bool isLayoutNodeVisible(const LayoutNode *node) const;
	void setActiveNode(const LayoutNode *node,
		bool preserveHorizontalPosition = false,
		bool preserveVerticalPosition = false);
	void ensureActiveNodeVisible(bool preserveHorizontalPosition = false,
		bool preserveVerticalPosition = false);
	wxColour colorForNode(const Database::FlameGraphNode *node) const;
	wxString makeTooltip(const LayoutNode *node) const;
	void zoomToNode(const LayoutNode *node);
	void resetZoom(bool resetScale = true);

	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnMouseMove(wxMouseEvent &event);
	void OnMouseLeave(wxMouseEvent &event);
	void OnLeftDown(wxMouseEvent& event);
	void OnRightDown(wxMouseEvent &event);
	void OnMouseWheel(wxMouseEvent &event);
	void OnCharHook(wxKeyEvent &event);
	void OnResetZoom(wxCommandEvent &event);

	DECLARE_EVENT_TABLE()
};
