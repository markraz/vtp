//
// ElevLayer.cpp
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "ElevLayer.h"
#include "ScaledView.h"
#include "Helper.h"
#include "RawDlg.h"
#include "vtBitmap.h"
#include "vtdata/ElevationGrid.h"
#include "vtdata/vtDIB.h"
#include "vtdata/vtLog.h"

#define SHADING_BIAS	200

////////////////////////////////////////////////////////////////////

vtTin2d::vtTin2d()
{
	m_fEdgeLen = NULL;
	m_bConstrain = false;
}

void vtTin2d::DrawTin(wxDC* pDC, vtScaledView *pView)
{
	wxPen TinPen(wxColor(128,0,128), 1, wxSOLID);
	pDC->SetLogicalFunction(wxCOPY);
	pDC->SetPen(TinPen);

	FPoint2 p2;
	int tris = NumTris();
	for (int j = 0; j < tris; j++)
	{
		if (m_bConstrain)
		{
			if (m_fEdgeLen[j] > m_fMaxEdge)
				continue;
		}
		int v0 = m_tri[j*3+0];
		int v1 = m_tri[j*3+1];
		int v2 = m_tri[j*3+2];

		pView->screen(m_vert[v0], g_screenbuf[0]);
		pView->screen(m_vert[v1], g_screenbuf[1]);
		pView->screen(m_vert[v2], g_screenbuf[2]);

		g_screenbuf[3] = g_screenbuf[0];
		pDC->DrawLines(4, g_screenbuf);
	}
}

void vtTin2d::SetConstraint(bool bConstrain, double fMaxEdge)
{
	m_bConstrain = bConstrain;
	m_fMaxEdge = fMaxEdge;
}

void vtTin2d::ComputeEdgeLengths()
{
	int nTris = NumTris();
	m_fEdgeLen = new double[nTris];
	for (int i = 0; i < nTris; i++)
		m_fEdgeLen[i] = GetTriMaxEdgeLength(i);
}

void vtTin2d::CullLongEdgeTris()
{
	if (!m_fEdgeLen)
		return;
	int nTris = NumTris();
	int b1, b2;
	int to = 0;
	int kept = 0;
	for (int i = 0; i < nTris; i++)
	{
		b1 = i * 3;
		if (m_fEdgeLen[i] < m_fMaxEdge)
		{
			// keep
			b2 = to * 3;
			m_tri[b2] = m_tri[b1];
			m_tri[b2+1] = m_tri[b1+1];
			m_tri[b2+2] = m_tri[b1+2];
			to++;
			kept++;
		}
	}
	m_tri.SetSize(kept*3);
}


void vtTin2d::FreeEdgeLengths()
{
	delete m_fEdgeLen;
	m_fEdgeLen = NULL;
}


////////////////////////////////////////////////////////////////////

bool vtElevLayer::m_bShowElevation = true;
bool vtElevLayer::m_bShading = true;
bool vtElevLayer::m_bDoMask = true;

vtElevLayer::vtElevLayer() : vtLayer(LT_ELEVATION)
{
	SetupDefaults();
	m_pGrid = NULL;
	m_pTin = NULL;
}

vtElevLayer::vtElevLayer(const DRECT &area, int iColumns, int iRows,
	bool bFloats, float fScale, const vtProjection &proj) : vtLayer(LT_ELEVATION)
{
	SetupDefaults();
	m_pGrid = new vtElevationGrid(area, iColumns, iRows, bFloats, proj);
	m_pGrid->SetScale(fScale);
	m_pGrid->GetDimensions(m_iColumns, m_iRows);

	m_pTin = NULL;
}

vtElevLayer::~vtElevLayer()
{
	if (m_bBitmapRendered)
	{
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
	if (m_pGrid)
		delete m_pGrid;
	if (m_pTin)
		delete m_pTin;
}

bool vtElevLayer::OnSave()
{
	if (m_pGrid)
		return m_pGrid->SaveToBT(m_strFilename.mb_str(), NULL, m_bPreferGZip);
	if (m_pTin)
		return m_pTin->Write(m_strFilename.mb_str());
	return false;
}

bool vtElevLayer::OnLoad()
{
	OpenProgressDialog(_T("Loading Elevation Layer"));

	bool success = false;

	if (m_strFilename.Contains(_T(".bt")))
	{
		// remember whether this layer was read from a compressed file
		if (!m_strFilename.Right(6).CmpNoCase(_T(".bt.gz")))
			m_bPreferGZip = true;

		m_pGrid = new vtElevationGrid();
		success = m_pGrid->LoadFromBT(m_strFilename.mb_str(), progress_callback);
		m_pGrid->GetDimensions(m_iColumns, m_iRows);
	}
	else if (!m_strFilename.Right(4).CmpNoCase(_T(".tin")) ||
			 !m_strFilename.Right(4).CmpNoCase(_T(".itf")))
	{
		m_pTin = new vtTin2d();
		success = m_pTin->Read(m_strFilename.mb_str());
	}

	CloseProgressDialog();
	return success;
}

bool vtElevLayer::ConvertProjection(vtProjection &proj_new)
{
	vtProjection proj_old;
	GetProjection(proj_old);

	if (proj_old == proj_new)
		return true;		// No conversion necessary

	OpenProgressDialog(_T("Converting Elevation Projection"));

	bool success = false;
	if (m_pGrid)
	{
		// Check to see if the projections differ *only* by datum
		vtProjection test = proj_old;
		test.SetDatum(proj_new.GetDatum());
		if (test == proj_new)
		{
			success = m_pGrid->ReprojectExtents(proj_new);
		}
		else
		{
			// actually re-project the grid elements
			vtElevationGrid *grid_new = new vtElevationGrid();

			success = grid_new->ConvertProjection(m_pGrid, proj_new, progress_callback);

			if (success)
			{
				delete m_pGrid;
				m_pGrid = grid_new;
				ReImage();
			}
			else
				delete grid_new;
		}
	}
	if (m_pTin)
	{
		success = m_pTin->ConvertProjection(proj_new);
	}

	CloseProgressDialog();
	return success;
}

bool vtElevLayer::NeedsDraw()
{
	if (m_bNeedsDraw)
		return true;
	if (m_pBitmap != NULL && m_bShowElevation && !m_bBitmapRendered)
		return true;
	return false;
}

void vtElevLayer::DrawLayer(wxDC* pDC, vtScaledView *pView)
{
	if (m_pGrid)
	{
		if (m_bShowElevation)
			DrawLayerBitmap(pDC, pView);
		else
			DrawLayerOutline(pDC, pView);
	}
	if (m_pTin)
	{
		m_pTin->DrawTin(pDC, pView);
	}
}

void vtElevLayer::DrawLayerBitmap(wxDC* pDC, vtScaledView *pView)
{
	if (!m_pGrid)
		return;

	if (m_pBitmap == NULL)
		SetupBitmap(pDC);

	if (m_pBitmap == NULL)
	{
		DrawLayerOutline(pDC, pView);
		return;
	}

	if (!m_bBitmapRendered)
		return;

	int iColumns, iRows;
	m_pGrid->GetDimensions(iColumns, iRows);

	wxRect screenrect = pView->WorldToCanvas(m_pGrid->GetAreaExtents());
	wxRect destRect = screenrect;
	wxRect srcRect(0, 0, m_iImageWidth, m_iImageHeight);

	double ratio_x = (float) srcRect.GetWidth() / destRect.GetWidth();
	double ratio_y = (float) srcRect.GetHeight() / destRect.GetHeight();

#if 0
	//clip stuff, so we only blit what we need
	int client_width, client_height;
	pView->GetClientSize(&client_width, &client_height); //get client window size
	if ((destRect.x + destRect.GetWidth() < 0) ||
			(destRect.y + destRect.GetHeight() < 0) ||
			(destRect.x > client_width) ||
			(destRect.y > client_height))
		//image completely off screen, return
		return;
	int diff;

	// clip left
	diff = 0 - destRect.x;
	if (diff > 0)
	{
		destRect.x += diff;
		srcRect.x += (long)(diff * ratio_x);
	}

	// clip top
	diff = 0 - destRect.y;
	if (diff > 0)
	{
		destRect.y += diff;
		srcRect.y += (long)(diff * ratio_y);
	}

	// clip right
	diff = destRect.x + destRect.GetWidth() - client_width;
	if (diff > 0)
	{
		destRect.width -= diff;
		srcRect.width -= (long)(diff * ratio_x);
	}

	// clip bottom
	diff = destRect.y + destRect.GetHeight() - client_height;
	if (diff > 0)
	{
		destRect.height -= diff;
		srcRect.height -= (long)(diff * ratio_y);
	}
#endif

#if WIN32
	::SetStretchBltMode((HDC) (pDC->GetHDC()), HALFTONE );
#endif

#if 0
	// Using StretchBlit is much faster and has less scaling/roundoff
	//  problems than using the wx method DrawBitmap.  However, GDI
	//  won't stretch and mask at the same time!
	wxDC2 *pDC2 = (wxDC2 *) pDC;
	pDC2->StretchBlit(*m_pBitmap->m_pBitmap, destRect.x, destRect.y,
		destRect.width, destRect.height);
#else
	// scale and draw the bitmap
	// must use SetUserScale since wxWindows doesn't provide StretchBlt
	double scale_x = 1.0/ratio_x;
	double scale_y = 1.0/ratio_y;
	pDC->SetUserScale(scale_x, scale_y);
	pDC->DrawBitmap(*m_pBitmap->m_pBitmap, (int) (destRect.x/scale_x),
		(int) (destRect.y/scale_y), m_bHasMask);

	// restore
	pDC->SetUserScale(1.0, 1.0);
#endif

#if 0
	// This is how we used to do it, with raw Win32 calls
	t->d_pDIBSection->Draw( pDC, destRect.left, destRect.top);
	pDC->SetStretchBltMode( HALFTONE );
	m_bitmap.d_pDIBSection->DrawScaled( pDC, destRect, srcRect);
#endif
}

void vtElevLayer::DrawLayerOutline(wxDC* pDC, vtScaledView *pView)
{
	wxRect screenrect = pView->WorldToCanvas(m_pGrid->GetAreaExtents());

	// draw a simple crossed box with green lines
	wxPen Pen1(wxColor(0x00, 0x80, 0x00), 1, wxSOLID);
	pDC->SetLogicalFunction(wxCOPY);
	pDC->SetPen(Pen1);

	screenrect.x++;
	screenrect.y++;
	screenrect.width-=2;
	screenrect.height-=2;

	DrawRectangle(pDC, screenrect);
}

bool vtElevLayer::GetExtent(DRECT &rect)
{
	if (m_pGrid)
	{
		rect = m_pGrid->GetEarthExtents();
		return true;
	}
	if (m_pTin)
	{
		float minh, maxh;
		m_pTin->GetExtents(rect, minh, maxh);
		return true;
	}
	return false;
}

bool vtElevLayer::AppendDataFrom(vtLayer *pL)
{
	if (pL->GetType() != LT_ELEVATION)
		return false;

	vtElevLayer *pEL = (vtElevLayer *)pL;
	if (m_pGrid)
	{
		// deliberately unimplemented - it does not make sense to do this
		// operation with grids
	}
	if (m_pTin && pEL->m_pTin)
	{
		// TODO
	}
	return false;
}


////////////////////////////////////////////////////////////////////

void vtElevLayer::SetupDefaults()
{
	m_bNeedsDraw = false;
	m_bBitmapRendered = false;
	m_strFilename = _T("Untitled");
	m_bPreferGZip = false;

	m_pBitmap = NULL;
	m_pMask = NULL;
}


void vtElevLayer::SetupBitmap(wxDC* pDC)
{
	m_pGrid->GetDimensions(m_iColumns, m_iRows);

	m_iImageWidth = m_iColumns;
	m_iImageHeight = m_iRows;

	int div = 1;
	while (m_iImageWidth * m_iImageHeight > 4096*4096)
	{
		// really huge bitmap is going to fail, chop it down
		div++;
		m_iImageWidth = m_iColumns / div;
		m_iImageHeight = m_iRows / div;
	}

	m_pBitmap = new vtBitmap();
	if (!m_pBitmap->Allocate(m_iImageWidth, m_iImageHeight))
	{
		DisplayAndLog("Couldn't create bitmap, probably too large.");
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
	m_bNeedsDraw = true;
}

void vtElevLayer::RenderBitmap()
{
	// flag as being rendered
	m_bNeedsDraw = false;

	// only show a progress dialog for large terrain (>300 points tall)
	bool bProg = (m_iRows > 300);

#if WIN32
	// mew 2002-08-17: reuse of wxProgressDialog causes SIGSEGV,
	// so just disable for now. (wxGTK 2.2.9 on Linux Mandrake 8.1)
	if (bProg)
		OpenProgressDialog(_T("Rendering Bitmap"), true);
#endif

	UpdateProgressDialog(0, _T("Generating colors..."));
	DetermineMeterSpacing();

	int i, j;		// image coord
	int x, y;		// elevation coord
	int r, g, b;
	int stepx = m_iColumns / m_iImageWidth;
	int stepy = m_iRows / m_iImageHeight;

	bool has_invalid = false;
	for (j = 0; j < m_iImageHeight; j++)
	{
		if (bProg)
		{
			if (UpdateProgressDialog(j*80/m_iImageHeight))
			{
				wxString2 msg = "Turn off displayed elevation for elevation layers?";
				if (wxMessageBox(msg, _T(""), wxYES_NO) == wxYES)
				{
					m_bShowElevation = false;
					CloseProgressDialog();
					return;
				}
				else
					ResumeProgressDialog();
			}
		}
		y = m_iRows - 1 - (j * stepy);
		for (i = 0; i < m_iImageWidth; i++)
		{
			x = (i * stepx);

			if (!has_invalid && (m_pGrid->GetValue(x, y) == INVALID_ELEVATION))
				has_invalid = true;
			GenerateShadedColor(x, y, r, g, b);
//			GenerateColorFromGrid2(i, m_iRows-1-j, r, g, b);

			m_pBitmap->SetRGB(i, j, r, g, b);
		}
	}

	if (has_invalid && m_bDoMask)
	{
		UpdateProgressDialog(90, _T("Hiding unknown areas..."));
		m_pMask = new wxMask(*m_pBitmap->m_pBitmap, wxColour(255, 0, 0));
		m_pBitmap->m_pBitmap->SetMask(m_pMask);
		m_bHasMask = true;
	}
	else
		m_bHasMask = false;

	m_bBitmapRendered = true;

	if (bProg)
		CloseProgressDialog();
}

void vtElevLayer::ReImage()
{
	if (m_pBitmap)
	{
		delete m_pBitmap;
		m_pBitmap = NULL;
	}
	m_bBitmapRendered = false;
}

#define LEVELS 14
#define RANGE 450
int level_colors[LEVELS][3] = {
	{ 40, 224, 40 },
	{ 0, 128, 0 },
	{ 100, 144, 76 },
	{ 204, 170, 136 },
	{ 136, 100, 70 },
	{ 128, 128, 128 },
	{ 180, 128, 64 },
	{ 255, 144, 32 },
	{ 200, 110, 80 },
	{ 160, 80, 160 },
	{ 144, 40, 128 },
	{ 128, 128, 128 },
	{ 255, 255, 255 },
	{ 128, 128, 128 }
};
int bathy_colors[LEVELS][3] = {
	{ 182, 228, 255 },
	{ 108, 156, 195 },
	{ 81, 121, 172 },
	{ 43, 90, 142 },
	{ 0, 0, 160 },
	{ 0, 128, 128 },
	{ 0, 128, 0 },
	{ 64, 128, 60 },
	{ 128, 128, 128 },
	{ 144, 64, 144 },
	{ 160, 0, 160 },
	{ 128, 128, 0 },
	{ 160, 80, 0 },
	{ 60, 60, 60 }
//	{ 255, 255, 255 }
};

void vtElevLayer::GenerateShadedColor(int i, int j, int &r, int &g, int &b)
{
	GenerateColorFromGrid1(i, j, r, g, b);
	if (m_bShading)
		ShadePixel(i, j, r, g, b, SHADING_BIAS);
}

void vtElevLayer::GenerateColorFromGrid1(int i, int j, int &r, int &g, int &b)
{
	float value = m_pGrid->GetFValue(i, j);
	if (value == INVALID_ELEVATION)
	{
		r = 255;
		g = b = 0;
	}
	else if (value == 0)
	{
		r = g = 0;
		b = 0xee;
	}
	else if (value < 0)
	{
		int pocket = (int) ((-value) / RANGE);
		if (pocket >= 0 && pocket < LEVELS)
		{
			float s = ((-(int)value) % RANGE) / (float)RANGE;
			r = (int) (bathy_colors[pocket][0] + (s * (bathy_colors[pocket+1][0] - bathy_colors[pocket][0])));
			g = (int) (bathy_colors[pocket][1] + (s * (bathy_colors[pocket+1][1] - bathy_colors[pocket][1])));
			b = (int) (bathy_colors[pocket][2] + (s * (bathy_colors[pocket+1][2] - bathy_colors[pocket][2])));
		}
		else
		{
			r = g = b = 0;
		}
	}
	else
	{
		int pocket = (int) (value / RANGE);
		if (pocket >= 0 && pocket < LEVELS)
		{
			float s = ((int)value % RANGE) / (float)RANGE;
			r = (int) (level_colors[pocket][0] + (s * (level_colors[pocket+1][0] - level_colors[pocket][0])));
			g = (int) (level_colors[pocket][1] + (s * (level_colors[pocket+1][1] - level_colors[pocket][1])));
			b = (int) (level_colors[pocket][2] + (s * (level_colors[pocket+1][2] - level_colors[pocket][2])));
		}
		else
		{
			r = g = b = 0;
		}
	}
}

#define RANGES	8
int ranges[RANGES] = { 0, 150, 400, 800, 1600, 2800, 4000, 5000 };
#if 0
int colors[RANGES][3] = {
	{ 132, 165, 115 },
	{ 156, 173, 132 },
	{ 189, 189, 148 },
	{ 214, 214, 165 },
	{ 231, 198, 140 },
	{ 214, 189, 123 },
	{ 189, 189, 189 },
	{ 125, 125, 125 }
};
#else
int colors[RANGES][3] = {
	{ 221, 188, 140 },
	{ 156, 173, 132 },
	{ 189, 189, 148 },
	{ 214, 214, 165 },
	{ 231, 198, 140 },
	{ 214, 189, 123 },
	{ 189, 189, 189 },
	{ 125, 125, 125 }
};
#endif

void vtElevLayer::GenerateColorFromGrid2(int i, int j, int &r, int &g, int &b)
{
	float value = m_pGrid->GetFValue(i, j);
	if (value == INVALID_ELEVATION)
	{
		r = 255;
		g = b = 0;
	}
	else if (value == 0.0f)
	{
		r = g = 0x22;
		b = 0x99;
	}
	else if (value > -1.0f && value < 0.0f)
	{
		r = 221;
		g = 178;	// land just below sea-level
		b = 125;
	}
	else if (value < 0.0f)
	{
		int pocket = (int) ((-value) / RANGE);
		if (pocket >= 0 && pocket < LEVELS)
		{
			float s = ((-(int)value) % RANGE) / (float)RANGE;
			r = (int) (bathy_colors[pocket][0] + (s * (bathy_colors[pocket+1][0] - bathy_colors[pocket][0])));
			g = (int) (bathy_colors[pocket][1] + (s * (bathy_colors[pocket+1][1] - bathy_colors[pocket][1])));
			b = (int) (bathy_colors[pocket][2] + (s * (bathy_colors[pocket+1][2] - bathy_colors[pocket][2])));
		}
		else
		{
			r = g = b = 0;
		}
	}
	else
	{
		r = g = b = 0;
		for (int i = 0; i < RANGES; i++)
		{
			if (value > ranges[i]) continue;
			float s = (value - ranges[i-1]) / (ranges[i] - ranges[i-1]);
			r = (int) (colors[i-1][0] + (s * (colors[i][0] - colors[i-1][0])));
			g = (int) (colors[i-1][1] + (s * (colors[i][1] - colors[i-1][1])));
			b = (int) (colors[i-1][2] + (s * (colors[i][2] - colors[i-1][2])));
			break;
		}
	}
}

void vtElevLayer::ShadePixel(int i, int j, int &r, int &g, int &b, int bias)
{
	float value = m_pGrid->GetFValue(i, j);
	if (value != INVALID_ELEVATION && value != 0 && i < m_iColumns-1)
	{
		float value2 = m_pGrid->GetFValue(i+1, j);
		int diff = (int) ((value2 - value) / m_fSpacing * bias);

		// clip to keep values under control
		if (diff > 128)
			diff = 128;
		else if (diff < -128)
			diff = -128;
		r += diff;
		g += diff;
		b += diff;
		if (r < 0) r = 0;
		else if (r > 255) r = 255;
		if (g < 0) g = 0;
		else if (g > 255) g = 255;
		if (b < 0) b = 0;
		else if (b > 255) b = 255;
	}
}

void vtElevLayer::FillGaps()
{
	int i, j, ix, jx, surrounding;
	bool gaps_exist = true;
	float value, value2, sum;
	float *patch_column = new float[m_iRows];

	// Create progress dialog for the slow part
	OpenProgressDialog(_T("Filling Gaps"));

	// For speed, remember which lines already have no gaps, so we don't have
	// to visit them again.
	bool *line_gap = new bool[m_iColumns];
	for (i = 0; i < m_iColumns; i++)
		line_gap[i] = true;

	while (gaps_exist)
	{
		gaps_exist = false;
		// iterate through the vertices of the new terrain
		for (i = 0; i < m_iColumns; i++)
		{
			UpdateProgressDialog(i*100/m_iColumns);
			if (!line_gap[i])
				continue;
			line_gap[i] = false;

			bool patches = false;
			for (j = 0; j < m_iRows; j++)
				patch_column[j] = INVALID_ELEVATION;

			for (j = 0; j < m_iRows; j++)
			{
				value = m_pGrid->GetFValue(i, j);
				if (value != INVALID_ELEVATION)
					continue;

				// else gap
				gaps_exist = true;
				line_gap[i] = true;

				// look at surrounding pixels
				sum = 0;
				surrounding = 0;
				for (ix = -1; ix <= 1; ix++)
				{
					for (jx = -1; jx <= 1; jx++)
					{
						value2 = m_pGrid->GetFValueSafe(i+ix, j+jx);
						if (value2 != INVALID_ELEVATION)
						{
							sum += value2;
							surrounding++;
						}
					}
				}
				if (surrounding != 0)
				{
					patch_column[j] = sum / surrounding;
					patches = true;
				}
			}
			if (patches)
			{
				for (j = 0; j < m_iRows; j++)
				{
					if (patch_column[j] != INVALID_ELEVATION)
						m_pGrid->SetFValue(i, j, patch_column[j]);
				}
			}
		}
	}
	delete line_gap;
	delete patch_column;
	CloseProgressDialog();
}

/**
 * Determine the approximate spacing, in meters, between each grid cell, in the X
 * direction.  The result is placed in the m_fSpacing member for use in shading.
 */
void vtElevLayer::DetermineMeterSpacing()
{
	vtProjection &proj = m_pGrid->GetProjection();
	if (proj.IsGeographic())
	{
		DRECT area = m_pGrid->GetEarthExtents();

		double fToMeters = EstimateDegreesToMeters(area.bottom);
		m_fSpacing = (float) (area.Width()) * fToMeters / (m_iColumns - 1);
	}
	else
	{
		// Linear units-based projections are much simpler
		DPoint2 spacing = m_pGrid->GetSpacing();
		m_fSpacing = spacing.x * GetMetersPerUnit(proj.GetUnits());
	}
}

void vtElevLayer::Offset(const DPoint2 &p)
{
	if (m_pGrid)
	{
		DRECT area = m_pGrid->GetEarthExtents();
		area.left += p.x;
		area.right += p.x;
		area.top += p.y;
		area.bottom += p.y;
		m_pGrid->SetEarthExtents(area);
	}
	if (m_pTin)
	{
		m_pTin->Offset(p);
	}
}

vtHeightField *vtElevLayer::GetHeightField()
{
	if (m_pGrid)
		return m_pGrid;
	if (m_pTin)
		return m_pTin;
	return NULL;
}

float vtElevLayer::GetElevation(DPoint2 &p)
{
	if (m_pGrid)
		return m_pGrid->GetFilteredValue(p.x, p.y);
	if (m_pTin)
	{
		float fAltitude;
		if (m_pTin->FindAltitudeAtPoint2(p, fAltitude))
			return fAltitude;
	}
	return INVALID_ELEVATION;
}

void vtElevLayer::GetProjection(vtProjection &proj)
{
	if (m_pGrid)
		proj = m_pGrid->GetProjection();
	else if (m_pTin)
		proj = m_pTin->m_proj;
}

void vtElevLayer::SetProjection(const vtProjection &proj)
{
	if (m_pGrid)
		m_pGrid->SetProjection(proj);
	if (m_pTin)
		m_pTin->m_proj = proj;
}

bool vtElevLayer::ImportFromFile(const wxString2 &strFileName,
	void progress_callback(int am))
{
	VTLOG("ImportFromFile '%s'\n", strFileName.mb_str());

	wxString strExt = strFileName.AfterLast('.');

	// The first character in the file is useful for telling which format
	// the file really is.
	FILE *fp = fopen(strFileName.mb_str(), "rb");
	char first = fgetc(fp);
	fclose(fp);

	bool success = false;

	if (m_pGrid == NULL)
		m_pGrid = new vtElevationGrid();

	if (!strExt.CmpNoCase(_T("dem")))
	{
		if (first == '*')
			success = m_pGrid->LoadFromMicroDEM(strFileName.mb_str(), progress_callback);
		else
			success = m_pGrid->LoadFromDEM(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("asc")))
	{
//		success = m_pGrid->LoadFromASC(strFileName, progress_callback);
		// vtElevationGrid does have its own ASC reader, but use GDAL instead
		success = m_pGrid->LoadWithGDAL(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("bil")))
	{
		success = m_pGrid->LoadWithGDAL(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("mem")))
	{
		success = m_pGrid->LoadWithGDAL(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("ter")))
	{
		success = m_pGrid->LoadFromTerragen(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("cdf")))
	{
		success = m_pGrid->LoadFromCDF(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("hdr")))
	{
		success = m_pGrid->LoadFromGTOPO30(strFileName.mb_str(), progress_callback);
		if (!success)
			success = m_pGrid->LoadFromGLOBE(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("dte")) ||
			!strExt.CmpNoCase(_T("dt0")) ||
			!strExt.CmpNoCase(_T("dt1")) ||
			!strExt.CmpNoCase(_T("dt2")))
	{
		success = m_pGrid->LoadFromDTED(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("pgm")))
	{
		success = m_pGrid->LoadFromPGM(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("grd")))
	{
		// might by CDF, might be GRD
		if (first == 'D')
			success = m_pGrid->LoadFromGRD(strFileName.mb_str(), progress_callback);
		else
			success = m_pGrid->LoadFromCDF(strFileName.mb_str(), progress_callback);
		if (!success)
		{
			// Might be 'Arc Binary Grid', try GDAL
			success = m_pGrid->LoadWithGDAL(strFileName.mb_str(), progress_callback);
		}
	}
	else if (!strFileName.Right(8).CmpNoCase(_T("catd.ddf")) ||
			!strExt.Left(3).CmpNoCase(_T("tif")) ||
			!strExt.Left(3).CmpNoCase(_T("png")) ||
			!strExt.Left(3).CmpNoCase(_T("img")) ||
			!strExt.CmpNoCase(_T("adf")))
	{	
		success = m_pGrid->LoadWithGDAL(strFileName.mb_str(), progress_callback);
	}
	else if (!strExt.CmpNoCase(_T("raw")))
	{
		RawDlg dlg(NULL, -1, _T("Raw Elevation File"), wxDefaultPosition);
		dlg.m_bUTM = true;
		dlg.m_bFloating = false;
		dlg.m_iBytes = 2;
		dlg.m_iWidth = 100;
		dlg.m_iHeight = 100;
		dlg.m_fVUnits = 1.0f;
		dlg.m_fSpacing = 30.0f;
		dlg.m_bBigEndian = false;
		if (dlg.ShowModal() == wxID_OK)
		{
			success = m_pGrid->LoadFromRAW(strFileName.mb_str(), dlg.m_iWidth,
					dlg.m_iHeight, dlg.m_iBytes, dlg.m_fVUnits, dlg.m_bBigEndian,
					progress_callback);
		}
		if (success)
		{
			m_pGrid->m_EarthExtents.top = dlg.m_iHeight * dlg.m_fSpacing;
			m_pGrid->m_EarthExtents.right = dlg.m_iWidth * dlg.m_fSpacing;
		}
	}
	else if (!strExt.CmpNoCase(_T("ntf")))
	{
		success = m_pGrid->LoadFromNTF5(strFileName.mb_str(), progress_callback);
	}
	if (!success)
	{
		DisplayAndLog("Couldn't import data from that file.");
	}

	return success;
}

void vtElevLayer::PaintDibFromElevation(vtDIB *dib, bool bShade)
{
	DetermineMeterSpacing();

	int w = dib->GetWidth();
	int h = dib->GetHeight();

	wxString str;
	int percent, last = -1;

	int gw, gh;
	m_pGrid->GetDimensions(gw, gh);

	int i, j, x, y, r, g, b;
	for (i = 0; i < w; i++)
	{
		percent = i * 100 / w;
		if (percent != last)
		{
			str.Printf(_T("%d%%"), percent);
			UpdateProgressDialog(percent, str);
			last = percent;
		}

		x = i * gw / w;			// find corresponding location in terrain

		for (j = 0; j < h; j++)
		{
			y = j * gh / h;
			GenerateColorFromGrid2(x, y, r, g, b);
#if 0
			r = r * 2 / 3;
			g = g * 2 / 3;
			b = b * 2 / 3;
#endif
			if (bShade)
				ShadePixel(x, y, r, g, b, 60);
			dib->SetPixel24(i, h-1-j, RGB(r, g, b));
		}
	}
}

void vtElevLayer::SetTin(vtTin2d *pTin)
{
	m_pTin = pTin;
	m_bNative = true;
}

void vtElevLayer::MergeSharedVerts(bool bSilent)
{
	if (!m_pTin)
		return;

	OpenProgressDialog(_T("Merging shared vertices"));

	int before = m_pTin->NumVerts();
	m_pTin->MergeSharedVerts(progress_callback);
	int after = m_pTin->NumVerts();

	CloseProgressDialog();

	if (!bSilent)
	{
		if (after < before)
			DisplayAndLog("Reduced vertices from %d to %d", before, after);
		else
			DisplayAndLog("There are %d vertices, unable to merge any.", before);
	}
}

void vtElevLayer::GetPropertyText(wxString &strIn)
{
	wxString str;

	if (m_pGrid)
	{
		int cols, rows;
		m_pGrid->GetDimensions(cols, rows);
		str.Printf(_T("Grid size: %d x %d\n"), cols, rows);
		strIn += str;

		bool bGeo = (m_pGrid->GetProjection().IsGeographic() != 0);
		strIn += _T("Grid spacing: ");
		DPoint2 spacing = m_pGrid->GetSpacing();
		strIn += FormatCoord(bGeo, spacing.x);
		strIn += _T(" x ");
		strIn += FormatCoord(bGeo, spacing.y);
		strIn += _T("\n");

		str.Printf(_T("Floating point: %hs\n"), m_pGrid->IsFloatMode() ? "Yes" : "No");
		strIn += str;

		m_pGrid->ComputeHeightExtents();
		float fMin, fMax;
		m_pGrid->GetHeightExtents(fMin, fMax);
		str.Printf(_T("Minimum elevation: %.2f\n"), fMin);
		strIn += str;
		str.Printf(_T("Maximum elevation: %.2f\n"), fMax);
		strIn += str;

		str.Printf(_T("Height scale (meters per vertical unit): %f\n"), m_pGrid->GetScale());
		strIn += str;

		const char *dem_name = m_pGrid->GetDEMName();
		if (*dem_name)
		{
			str.Printf(_T("Original DEM name: \"%hs\"\n"), dem_name);
			strIn += str;
		}
	}
	if (m_pTin)
	{
		int verts = m_pTin->NumVerts();
		int tris = m_pTin->NumTris();
		str.Printf(_T("TIN\nVertices: %d\nTriangles: %d\n"), verts, tris);
		strIn += str;
	}
}

wxString vtElevLayer::GetFileExtension()
{
	if (m_pTin)
		return 	_T(".itf");
	else
	{
		if (m_bPreferGZip)
			return 	_T(".bt.gz");
		else
			return 	_T(".bt");
	}
}

//
// Elevations are slightly more complicated than other layers, because there
// are two formats allowed for saving.  This gets a bit messy, especially since
// wxWindows does not support our double extension (.bt.gz) syntax.
//
bool vtElevLayer::AskForSaveFilename()
{
	wxString filter;

	if (m_pTin)
		filter = FSTRING_TIN;
	else
	{
		filter = _T("BT File (.bt)|*.bt|GZipped BT File (.bt.gz)|*.bt.gz|");
//		AddType(filter, _T(""));
	}

	wxFileDialog saveFile(NULL, _T("Save Layer"), _T(""), m_strFilename,
		filter, wxSAVE | wxOVERWRITE_PROMPT);

	if (m_pGrid && m_bPreferGZip)
		saveFile.SetFilterIndex(1);
	else
		saveFile.SetFilterIndex(0);

	VTLOG("Asking user for elevation file name\n");
	bool bResult = (saveFile.ShowModal() == wxID_OK);
	if (!bResult)
		return false;

	wxString2 name = saveFile.GetPath();
	VTLOG("Got filename: '%s'\n", name.mb_str());

	if (m_pGrid)
	{
		m_bPreferGZip = (saveFile.GetFilterIndex() == 1);

		// work around incorrect extension(s) that wxFileDialog added
		if (!name.Right(3).CmpNoCase(_T(".gz")))
			name = name.Left(name.Len()-3);
		if (!name.Right(3).CmpNoCase(_T(".bt")))
			name = name.Left(name.Len()-3);

		if (m_bPreferGZip)
			name += _T(".bt.gz");
		else
			name += _T(".bt");
	}

	SetFilename(name);
	m_bNative = true;
	return true;
}
