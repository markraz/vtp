//
// RoadMapEdit.cpp
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "RoadMapEdit.h"
#include "assert.h"
#include "NodeDlg.h"
#include "RoadDlg.h"
#include "RoadLayer.h"
#include "Frame.h"
#include "ScaledView.h"

#define NODE_RADIUS 5

wxPen RoadPen[11];
wxPen NodePen[VIT_TOTAL];
static bool g_bInitializedPens = false;

#define RP_HIGHWAY 0
// 1 through 7 are SURFT_GRAVEL through SURFT_STONE
#define RP_SELECTION 8
#define RP_DIRECTION 9
#define RP_CROSSES	10

NodeEdit::NodeEdit() : TNode()
{
	m_bSelect = false;
	m_iPathIndex = 0;
	m_pPrevPathNode = NULL;
	m_pPrevPathLink = NULL;
	m_iVisual = VIT_UNKNOWN;
}

//
// Equality operator
//
bool NodeEdit::operator==(NodeEdit &ref)
{
	if (! ((*((TNode *)this)) == ref))
		return false;

	return (m_iVisual == ref.m_iVisual);
}

void NodeEdit::Copy(NodeEdit* node)
{
	TNode::Copy(node);
	m_bSelect = node->m_bSelect;
	m_iVisual = node->m_iVisual;
}

//
// draw a node as a circle
//
bool NodeEdit::Draw(wxDC* pDC, vtScaledView *pView)
{
	pDC->SetLogicalFunction(wxCOPY);
	assert(m_iVisual >= VIT_UNKNOWN && m_iVisual <= VIT_STOPSIGN);
	pDC->SetPen(NodePen[m_iVisual]);
	pDC->SetBrush(wxBrush(wxColour(0,0,0), wxTRANSPARENT));
	wxRect box;

	int x = pView->sx(m_p.x);
	int y = pView->sy(m_p.y);
	box.x = x - NODE_RADIUS;
	box.y = y - NODE_RADIUS;
	box.width = NODE_RADIUS << 1;
	box.height = NODE_RADIUS << 1;
	pDC->DrawEllipse(box);
	if (m_bSelect)
	{
		pDC->SetLogicalFunction(wxINVERT);
		pDC->SetPen(NodePen[5]);
		pDC->DrawEllipse(box);
	}
	return true;
}

//
// bring up dialog box to edit node properties.
//
bool NodeEdit::EditProperties(vtRoadLayer *pLayer)
{
	NodeDlg dlg(NULL, -1, _("Node Properties"));
	dlg.SetNode(this, pLayer);
	return (dlg.ShowModal() == wxID_OK);
}

void NodeEdit::Translate(const DPoint2 &offset)
{
	m_p += offset;

	// update the endpoints of all the links that meet here
	for (int i = 0; i < m_iLinks; i++)
	{
		TLink *pR = m_connect[i].pLink;
		if (pR->GetNode(0) == this)
			pR->SetAt(0, m_p);
		if (pR->GetNode(1) == this)
			pR->SetAt(pR->GetSize()-1, m_p);
	}
}

void NodeEdit::DetermineVisualFromLinks()
{
	IntersectionType it;

	int nlights = 0, nstops = 0;

	for (int i = 0; i < m_iLinks; i++)
	{
		it = GetIntersectType(i);
		if (it == IT_LIGHT) nlights++;
		if (it == IT_STOPSIGN) nstops++;
	}
	if (nlights == m_iLinks)
		m_iVisual = VIT_ALLLIGHTS;
	else if (nstops == m_iLinks)
		m_iVisual = VIT_ALLSTOPS;
	else if (nlights > 0)
		m_iVisual = VIT_LIGHTS;
	else if (nstops > 0)
		m_iVisual = VIT_STOPSIGN;
	else
		m_iVisual = VIT_NONE;
}


///////////////////////////////////////////////////////////////////

LinkEdit::LinkEdit() : TLink(), Selectable()
{
	m_extent.SetRect(0,0,0,0);
	m_iPriority = 3;
	m_fLength = 0.0f;
	m_bDrawPoints = false;
	m_bSidesComputed = false;
}

//
// Equality operator
//
bool LinkEdit::operator==(LinkEdit &ref)
{
	if (! ((*((TLink *)this)) == ref))
		return false;

	return (m_iPriority == ref.m_iPriority &&
		m_fLength == ref.m_fLength);
}

void LinkEdit::ComputeExtent()
{
	int size = GetSize();

	DPoint2 p;
	p = GetAt(0);
	m_extent.SetRect(p.x, p.y, p.x, p.y);
	for (int i = 1; i < size; i++)
		m_extent.GrowToContainPoint(GetAt(i));
}

void LinkEdit::ComputeDisplayedLinkWidth(const DPoint2 &ToMeters)
{
	// also refresh the parallel left and right road edges
	m_fWidth = EstimateWidth();
	double half_width = (m_fWidth / 2);

	unsigned int size = GetSize();
	m_WidthOffset.SetSize(size);

	DPoint2 norm, prev, offset;

	for (unsigned int i = 0; i < size; i++)
	{
		prev = norm;

		DPoint2 p = GetAt(i);
		if (i < size-1)
		{
			DPoint2 vec = GetAt(i+1) - p;
			norm.x = -vec.y;
			norm.y = vec.x;
			norm.Normalize();
		}
		if (i == 0)		// first point
			offset = norm * half_width;
		else if (i > 0 && i < size-1)
		{
			// vector which bisects this point is the combination of both normals
			DPoint2 bisect = (norm + prev).Normalize();

			// compute angle between the vectors
			double dot = prev.Dot(-norm);
			if (dot <= -0.97 || dot >= 0.97)
			{
				// simple case: close enough to colinear
				offset = bisect * half_width;
			}
			else
			{
				double angle = acos(dot);

				// factor to widen this corner is proportional to the angle
				double wider = 1.0 / sin(angle / 2);
				offset = bisect * half_width * wider;
			}
		}
		else if (i == size-1)	// last point
			offset = prev * half_width;

		offset.x /= ToMeters.x;			// convert (potentially) to degrees
		offset.y /= ToMeters.y;
		m_WidthOffset[i] = offset;
	}
}

bool LinkEdit::WithinExtent(const DRECT &target)
{
	return (target.left < m_extent.right && target.right > m_extent.left &&
		target.top > m_extent.bottom && target.bottom < m_extent.top);
}

bool LinkEdit::WithinExtent(const DPoint2 &p)
{
	return (p.x > m_extent.left && p.x < m_extent.right &&
			p.y > m_extent.bottom && p.y < m_extent.top);
}

//is extent of the road in "bound"
bool LinkEdit::InBounds(const DRECT &bound)
{
	//eliminate easy cases.
	if (m_extent.top < bound.bottom ||
			m_extent.bottom > bound.top ||
			m_extent.right < bound.left ||
			m_extent.left > bound.right) {
		return false;
	}

	//simple correct case:
	if ((m_extent.top < bound.top) &&
			(m_extent.bottom > bound.bottom) &&
			(m_extent.right < bound.right) &&
			(m_extent.left > bound.left)) {
		return true;
	}

	return false;
}

//is extent of the road in "bound"
bool LinkEdit::PartiallyInBounds(const DRECT &bound)
{
	//eliminate easy cases.
	if (m_extent.top < bound.bottom ||
			m_extent.bottom > bound.top ||
			m_extent.right < bound.left ||
			m_extent.left > bound.right) {
		return false;
	}

	//simple correct case:
	for (unsigned int i = 0; i < GetSize(); i++)
	{
		DPoint2 point = GetAt(i);
		if (point.x > bound.left && point.x < bound.right &&
				point.y > bound.bottom && point.y < bound.top)
		{
			return true;
		}
	}

	return false;
}


bool LinkEdit::Draw(wxDC* pDC, vtScaledView *pView, bool bShowDirection,
	bool bShowWidth)
{
	// base road color on type of road
	pDC->SetLogicalFunction(wxCOPY);
	if (m_iHwy != -1)
		pDC->SetPen(RoadPen[RP_HIGHWAY]);
	else
		pDC->SetPen(RoadPen[m_Surface]);

	int c, size = GetSize();
	if (bShowWidth)
		pView->DrawDoubleLine(pDC, *this, m_WidthOffset);
	else
		pView->DrawLine(pDC, *this, false);

	if (m_bSelect)
	{
		pDC->SetLogicalFunction(wxINVERT);
		pDC->SetPen(RoadPen[RP_SELECTION]);
		pView->DrawLine(pDC, *this, false);
	}
	if (bShowDirection)
	{
		int mid = (GetSize() == 2) ? 0 : GetSize() / 2;

		FPoint2 p0, p1, diff, center;
		FPoint2 fw, side;
		int r = 0;

		diff.x = diff.y = 0;
		while (fabs(diff.x) < 2 && fabs(diff.y) < 2)
		{
			p0.x = g_screenbuf[mid-r].x;
			p0.y = g_screenbuf[mid-r].y;
			p1.x = g_screenbuf[mid+r+1].x;
			p1.y = g_screenbuf[mid+r+1].y;
			diff = p1 - p0;
			r++;
		}
		center = p0 + (diff * 0.5f);

		fw.x = diff.x;
		fw.y = diff.y;
		fw.Normalize();
		side.x = -fw.y;
		side.y = fw.x;
		pDC->SetPen(RoadPen[RP_DIRECTION]);
		if (m_iFlags & RF_FORWARD)
		{
			pDC->DrawLine((int) (center.x - side.x * 5.0f),
				(int) (center.y - side.y * 5.0f),
				(int) (center.x + fw.x * 6.0f),
				(int) (center.y + fw.y * 6.0f));
			pDC->DrawLine((int) (center.x + fw.x * 6.0f),
				(int) (center.y + fw.y * 6.0f),
				(int) (center.x + side.x * 5.0f),
				(int) (center.y + side.y * 5.0f));
		}
		if (m_iFlags & RF_REVERSE)
		{
			pDC->DrawLine((int) (center.x - side.x * 5.0f),
				(int) (center.y - side.y * 5.0f),
				(int) (center.x - fw.x * 6.0f),
				(int) (center.y - fw.y * 6.0f));
			pDC->DrawLine((int) (center.x - fw.x * 6.0f),
				(int) (center.y - fw.y * 6.0f),
				(int) (center.x + side.x * 5.0f),
				(int) (center.y + side.y * 5.0f));
		}
	}
	if (m_bDrawPoints)
	{
		pDC->SetPen(RoadPen[RP_CROSSES]);
		for (c = 0; c < size && c < SCREENBUF_SIZE; c++)
		{
			pDC->DrawLine(g_screenbuf[c].x-3, g_screenbuf[c].y,
				g_screenbuf[c].x+3, g_screenbuf[c].y);
			pDC->DrawLine(g_screenbuf[c].x, g_screenbuf[c].y-3,
				g_screenbuf[c].x, g_screenbuf[c].y+3);
		}
	}
	return true;
}

bool LinkEdit::EditProperties(vtRoadLayer *pLayer)
{
	RoadDlg dlg(NULL, -1, _("Road Properties"));
	dlg.SetRoad(this, pLayer);
	return (dlg.ShowModal() == wxID_OK);
}

	// override because we need to update width when flags change
void LinkEdit::SetFlag(int flag, bool value)
{
	int before = m_iFlags & (RF_SIDEWALK | RF_PARKING | RF_MARGIN);
	TLink::SetFlag(flag, value);
	int after = m_iFlags & (RF_SIDEWALK | RF_PARKING | RF_MARGIN);
	if (before != after)
		m_bSidesComputed = false;
}

// call whenever the link's goemetry is changed
void LinkEdit::Dirtied()
{
	ComputeExtent();
	m_bSidesComputed = false;
}


////////////////////////////////////////////////////////////////////////////////

RoadMapEdit::RoadMapEdit() : vtRoadMap()
{
	// create Pens for drawing roads
	if (!g_bInitializedPens)
	{
		g_bInitializedPens = true;

		RoadPen[RP_HIGHWAY].SetColour(128,0,0);	// dark red highways
		RoadPen[RP_HIGHWAY].SetWidth(2);

		RoadPen[SURFT_GRAVEL].SetColour(128,128,128);

		RoadPen[SURFT_TRAIL].SetColour(130,100,70);
		RoadPen[SURFT_TRAIL].SetStyle(wxDOT);

		RoadPen[SURFT_2TRACK].SetColour(130,100,70);

		RoadPen[SURFT_DIRT].SetColour(130,100,70);

		RoadPen[SURFT_PAVED].SetColour(0,0,0);

		RoadPen[SURFT_RAILROAD].SetColour(0,0,0);
		RoadPen[SURFT_RAILROAD].SetStyle(wxSHORT_DASH);

		RoadPen[RP_SELECTION].SetColour(255,255,255);	// for selection
		RoadPen[RP_SELECTION].SetWidth(3);

		RoadPen[RP_DIRECTION].SetColour(0,180,0);	// for direction
		RoadPen[RP_DIRECTION].SetWidth(2);

		RoadPen[RP_CROSSES].SetColour(128,0,128);	// for edit crosses

		NodePen[VIT_UNKNOWN].SetColour(255,0,255);

		NodePen[VIT_NONE].SetColour(0,128,255);

		NodePen[VIT_STOPSIGN].SetColour(128,0,0);
		NodePen[VIT_STOPSIGN].SetStyle(wxDOT);

		NodePen[VIT_ALLSTOPS].SetColour(128,0,0);

		NodePen[VIT_LIGHTS].SetColour(0,128,0);
		NodePen[VIT_LIGHTS].SetStyle(wxDOT);

		NodePen[VIT_ALLLIGHTS].SetColour(0,128,0);

		NodePen[VIT_SELECTED].SetColour(255,255,255);  //for selection
		NodePen[VIT_SELECTED].SetWidth(3);
	}
}

RoadMapEdit::~RoadMapEdit()
{
}

//merge 2 selected nodes
bool RoadMapEdit::Merge2Nodes()
{
	NodeEdit *pN = NULL;
	NodeEdit *pN2 = NULL;
	NodeEdit *curNode = GetFirstNode();
	int count = 0;

	while (curNode)
	{
		if (!curNode->IsSelected())
			continue;
		count++;
		if (pN == NULL)
			pN = curNode;
		else if (pN2 == NULL)
			pN2 = curNode;
		curNode = curNode->GetNext();
	}
	if (count != 2)
		return false;

	// we've got a pair that need to be merged
	//new point is placed between the 2 original points
	pN2->m_p.x = (pN2->m_p.x + pN->m_p.x) / 2;
	pN2->m_p.y = (pN2->m_p.y + pN->m_p.y) / 2;
	// we're going to remove the "pN" node
	// inform any roads which may have referenced it
	ReplaceNode(pN, pN2);
	// remove pN
	RemoveNode(pN);

	// for the roads that now end in pN2, move their end points
	pN2->EnforceLinkEndpoints();
	return true;
}

//
// draw the road network in window, given center and size or drawing area
//
void RoadMapEdit::Draw(wxDC* pDC, vtScaledView *pView, bool bNodes)
{
	if (bNodes)
	{
		for (NodeEdit *curNode = GetFirstNode(); curNode; curNode = curNode->GetNext())
			curNode->Draw(pDC, pView);
	}

	DPoint2 center;
	DPoint2 ToMeters(1.0, 1.0);	// convert (estimate) width to meters
	bool bGeo = (m_proj.IsGeographic() != 0);

	bool bShowWidth = vtRoadLayer::GetDrawWidth();
	bool bShowDir = vtRoadLayer::GetShowDirection();

	for (LinkEdit *curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (!curLink->m_bSidesComputed)
		{
			if (bGeo)
			{
				curLink->m_extent.GetCenter(center);
				ToMeters.x = EstimateDegreesToMeters(center.y);
				ToMeters.y = METERS_PER_LATITUDE;
			}
			curLink->ComputeDisplayedLinkWidth(ToMeters);
			curLink->m_bSidesComputed = true;
		}
		curLink->Draw(pDC, pView, bShowDir, bShowWidth);
	}
}

//
// delete all selected roads
//
DRECT *RoadMapEdit::DeleteSelected(int &nDeleted)
{
	nDeleted = NumSelectedLinks();
	if (nDeleted == 0)
		return NULL;

	DRECT* array = new DRECT[nDeleted];

	LinkEdit *prevLink = NULL;
	LinkEdit *tmpLink;
	LinkEdit *curLink = GetFirstLink();
	TNode *tmpNode;
	int n = 0;

	while (curLink)
	{
		tmpLink = curLink;
		curLink = curLink->GetNext();
		if (tmpLink->IsSelected())
		{
			//delete the road
			array[n] = tmpLink->m_extent;
			n++;

			if (prevLink)
				prevLink->m_pNext = curLink;
			else
				m_pFirstLink = curLink;

			tmpNode = tmpLink->GetNode(0);
			if (tmpNode)
				tmpNode->DetachLink(tmpLink, true);
			tmpNode = tmpLink->GetNode(1);
			if (tmpNode)
				tmpNode->DetachLink(tmpLink, false);
			delete tmpLink;
		}
		else
			prevLink = tmpLink;
	}
	m_bValidExtents = false;

	return array;
}

bool RoadMapEdit::SelectLink(DPoint2 point, float error, DRECT &bound)
{
	LinkEdit *link = FindLink(point, error);
	if (link)
	{
		link->ToggleSelect();
		bound = link->m_extent;
		return true;
	}
	return false;
}

int RoadMapEdit::SelectLinks(DRECT bound, bool bval)
{
	int found = 0;
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink->InBounds(bound)) {
			curLink->Select(bval);
			found++;
		}
	}
	return found;
}

bool RoadMapEdit::SelectAndExtendLink(DPoint2 point, float error, DRECT &bound)
{
	LinkEdit *originalLink = FindLink(point, error);
	if (originalLink == NULL)
		return false;

	originalLink->Select(true);
	bound = originalLink->m_extent;
	//extend the given road
	NodeEdit* node;
	LinkEdit* road = originalLink;
	//run twice.  once in node 0 direction.  once in node 1 direction
	node = (NodeEdit*) originalLink->GetNode(0);
	bool search;
	for (int i = 0; i < 2; i++) {
		search = true;
		while (search) {
			//ignore if there is only the one road.
			int index = -1;
			int j;
			float bestAngle = PI2f;
			int bestLinkIndex = -1;
			if (node->m_iLinks > 1) {
				node->SortLinksByAngle();
				//find index for current road
				for (j=0; j < node->m_iLinks; j++) {
					if (road == node->GetLink(j)) {
						index = j;	
					}
				}

				//compare index with all the other roads at the node.
				for (j = 0; j < node->m_iLinks; j++) {
					if (j != index) {
						float newAngle  = node->GetLinkAngle(j) - (node->GetLinkAngle(index) + PIf);
						//adjust to value between 180 and -180 degrees
						while (newAngle > PIf) {
							newAngle -= PI2f;
						}
						while (newAngle < -PIf) {
							newAngle += PI2f;
						}
						newAngle = fabsf(newAngle);

						//same highway number
						if (road->m_iHwy > 0 && road->m_iHwy == node->GetLink(j)->m_iHwy) {
							bestLinkIndex = j;
							bestAngle = 0;
							break;
						}
						if (newAngle < bestAngle) {
							bestAngle = newAngle;
							bestLinkIndex = j;
						}
					}
				}
				//wxLogMessage("best angle:%f, road: %i\n", bestAngle, bestLinkIndex);
				//ignore result if angle is too big
				if (bestAngle > PIf/6 && node->m_iLinks > 2) {
					bestLinkIndex = -1;
				} else if (road->m_iHwy > 0 && road->m_iHwy != node->GetLink(bestLinkIndex)->m_iHwy) {
					//highway must match with same highway number
					bestLinkIndex = -1;
				} else if (road->m_iHwy < 0 && node->GetLink(bestLinkIndex)->m_iHwy > 0) {
					//non-highway can't pair with a highway
					bestLinkIndex = -1;
				}
				if (bestLinkIndex != -1) {
					//select the road
					road = node->GetLink(bestLinkIndex);
					if (node == road->GetNode(0)) {
						node = road->GetNode(1);
					} else {
						node = road->GetNode(0);
					}
					road->Select(true);
					//increase the size of the extent
					if (road->m_extent.left < bound.left) bound.left = road->m_extent.left;
					if (road->m_extent.bottom < bound.bottom) bound.bottom = road->m_extent.bottom;
					if (road->m_extent.right > bound.right) bound.right = road->m_extent.right;
					if (road->m_extent.top > bound.top) bound.top = road->m_extent.top;
					if (road == originalLink) {
						bestLinkIndex = -1;
					}
				}
			}
			if (bestLinkIndex == -1) {
				//wxLogMessage("Stop!\n");
				search = false;
			}
		}
		//search in node(1) direction.
		node = (NodeEdit*) originalLink->GetNode(1);
		road = originalLink;
	}
	return true;
}

bool RoadMapEdit::SelectHwyNum(int num)
{
	bool found = false;
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink->m_iHwy == num) {
			curLink->Select(true);
			found = true;
		}
	}
	return found;
}

bool RoadMapEdit::CrossSelectLinks(DRECT bound, bool bval)
{
	bool found = false;
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink->PartiallyInBounds(bound)) {
			curLink->Select(bval);
			found = true;
		}
	}
	return found;
}

void RoadMapEdit::InvertSelection()
{
	for (NodeEdit* curNode = GetFirstNode(); curNode; curNode = curNode->GetNext())
		curNode->Select(!curNode->IsSelected());

	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
		curLink->Select(!curLink->IsSelected());
}

//
// Inverts selected value of node within epsilon of point
//
bool RoadMapEdit::SelectNode(const DPoint2 &point, float epsilon, DRECT &bound)
{
	NodeEdit *node = (NodeEdit *) FindNodeAtPoint(point, epsilon);
	if (node)
	{
		node->ToggleSelect();
		bound.left = bound.right = node->m_p.x;
		bound.top = bound.bottom = node->m_p.y;
		return true;
	}
	else
		return false;
}

//if bval true, select nodes within bound.  otherwise deselect nodes
int RoadMapEdit::SelectNodes(DRECT bound, bool bval)
{
	int found = 0;
	for (NodeEdit* curNode = GetFirstNode(); curNode; curNode = curNode->GetNext())
	{
		if (bound.ContainsPoint(curNode->m_p))
		{
			curNode->Select(bval);
			found++;
		}
	}
	return found;
}

int RoadMapEdit::NumSelectedNodes()
{
	int n = 0;
	for (NodeEdit* curNode = GetFirstNode(); curNode; curNode = curNode->GetNext())
		if (curNode->IsSelected())
			n++;
	return n;
}

int RoadMapEdit::NumSelectedLinks()
{
	int n = 0;
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
		if (curLink->IsSelected())
			n++;
	return n;
}

//
// caller is responsible for deleting the array returned!
//
DRECT *RoadMapEdit::DeSelectAll(int &numRegions)
{
	// count the number of regions (number of selected elements)
	int n = 0;
	n += NumSelectedNodes();
	n += NumSelectedLinks();
	numRegions = n;

	// make an array large enough to hold them all
	DRECT* array = new DRECT[n];

	// fill the array with the element's extents, and deselect them
	n = 0;
	for (NodeEdit* curNode = GetFirstNode(); curNode; curNode = curNode->GetNext())
	{
		if (curNode->IsSelected()) {
			array[n++] = DRECT(curNode->m_p.x, curNode->m_p.y, curNode->m_p.x, curNode->m_p.y);
			curNode->Select(false);
		}
	}
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink->IsSelected()) {
			array[n++] = curLink->m_extent;
			curLink->Select(false);
		}
	}
	return array;
}


LinkEdit *RoadMapEdit::FindLink(DPoint2 point, float error)
{
	LinkEdit *bestSoFar = NULL;
	double dist = error;
	double b;

	//a backwards rectangle, to provide greater flexibility for finding the road.
	DRECT target(point.x-error, point.y+error, point.x+error, point.y-error);
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink->WithinExtent(target))
		{
			b = curLink->DistanceToPoint(point);
			if (b < dist)
			{
				bestSoFar = curLink;
				dist = b;
			}
		}
	}
	return bestSoFar;
}

void RoadMapEdit::DeleteSingleLink(LinkEdit *pDeleteLink)
{
	LinkEdit *prev = NULL;
	for (LinkEdit* curLink = GetFirstLink(); curLink; curLink = curLink->GetNext())
	{
		if (curLink == pDeleteLink)
		{
			if (prev)
				prev->m_pNext = curLink->GetNext();
			else
				m_pFirstLink = curLink->GetNext();
			curLink->GetNode(0)->DetachLink(curLink, true);
			curLink->GetNode(1)->DetachLink(curLink, false);
			delete curLink;
			return;
		}
		prev = curLink;
	}
}

void RoadMapEdit::ReplaceNode(NodeEdit *pN, NodeEdit *pN2)
{
	bool lights = false;

	for (int i = 0; i < pN->m_iLinks; i++)
	{
		LinkConnect &lc = pN->GetLinkConnect(i);

		if (lc.eIntersection == IT_LIGHT)
			lights = true;

		if (lc.bStart)
			lc.pLink->SetNode(0, pN2);
		else
			lc.pLink->SetNode(1, pN2);

		int iNewLinkNum = pN2->AddLink(lc.pLink, lc.bStart);
		pN2->SetIntersectType(iNewLinkNum, lc.eIntersection);
	}
	while (TLink *pL = pN->GetLink(0))
		pN->DetachLink(pL, pN->GetLinkConnect(0).bStart);

	if (lights)
		pN2->AdjustForLights();
}

class LinkEdit *NodeEdit::GetLink(int n)
{
	if (n >= 0 && n < m_iLinks)	// safety check
		return (LinkEdit *) m_connect[n].pLink;
	else
		return NULL;
}

LinkEdit *RoadMapEdit::AddRoadSegment(OGRLineString *pLineString)
{
	// Road: implicit nodes at start and end
	LinkEdit *r = NewLink();
	int num_points = pLineString->getNumPoints();
	for (int j = 0; j < num_points; j++)
	{
		r->Append(DPoint2(pLineString->getX(j),	pLineString->getY(j)));
	}
	TNode *n1 = NewNode();
	n1->m_p.Set(pLineString->getX(0), pLineString->getY(0));

	TNode *n2 = NewNode();
	n2->m_p.Set(pLineString->getX(num_points-1), pLineString->getY(num_points-1));

	AddNode(n1);
	AddNode(n2);
	r->SetNode(0, n1);
	r->SetNode(1, n2);
	n1->AddLink(r, true);
	n2->AddLink(r, false);

	//set bounding box for the road
	r->ComputeExtent();

	AddLink(r);
	return r;
}

