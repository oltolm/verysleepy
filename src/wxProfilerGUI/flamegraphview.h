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

http://www.gnu.org/copyleft/gpl.html.
=====================================================================*/
#pragma once

#include "database.h"
#include <memory>
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

private:
	struct LayoutNode
	{
		const Database::FlameGraphNode *node;
		wxRect rect;
		int depth;
	};

	Database *database;
	wxButton *resetZoomButton;
	std::unique_ptr<Database::FlameGraphNode> flameGraph;
	std::vector<LayoutNode> layout;
	std::vector<const Database::FlameGraphNode *> zoomPath;
	const Database::Symbol *selectedSymbol;
	const LayoutNode *hoveredNode;
	const Database::FlameGraphNode *zoomRoot;
	int maxDepth;
	bool chartDirty;
	int rowHeight;
	int visibleDepthLimit;
	int hiddenDepthCount;

	void rebuildChart();
	void rebuildLayout();
	void layoutNode(const Database::FlameGraphNode *node, int depth, double x, double width, bool includeNode);
	void layoutZoomPath(double x, double width);
	int getSubtreeDepth(const Database::FlameGraphNode *node) const;
	bool buildZoomPath(const Database::FlameGraphNode *node,
		const Database::FlameGraphNode *target,
		std::vector<const Database::FlameGraphNode *> &path) const;
	void updateResetButton();
	bool isDepthVisible(int depth) const;
	const LayoutNode *hitTest(wxPoint point) const;
	wxColour colorForNode(const Database::FlameGraphNode *node) const;
	wxString makeTooltip(const LayoutNode *node) const;
	void activateNode(const LayoutNode *node, bool inspect);
	void zoomToNode(const LayoutNode *node);
	void zoomOut();
	void resetZoom();

	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnMouseMove(wxMouseEvent &event);
	void OnMouseLeave(wxMouseEvent &event);
	void OnLeftDown(wxMouseEvent &event);
	void OnLeftDClick(wxMouseEvent &event);
	void OnRightDown(wxMouseEvent &event);
	void OnResetZoom(wxCommandEvent &event);

	DECLARE_EVENT_TABLE()
};
