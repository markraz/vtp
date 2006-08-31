//
// Name:	 Contours.cpp
// Purpose:  Contour-related code, which interfaces vtlib to the
//	QuikGrid library.
//
// Copyright (c) 2004 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"

#if SUPPORT_QUIKGRID

#include "QuikGrid/surfgrid.h"
#include "QuikGrid/contour.h"

#ifdef _MSC_VER
#pragma comment( lib, "QuikGrid.lib" )
#endif

#include "Contours.h"

static ContourConverter *s_cc = NULL;

//
// This globally-scoped method is found at link time by the QuikGrid
//  library, which uses it as a callback.
//
void DoLineTo( float x, float y, int drawtype )
{
	if (s_cc)
		s_cc->Coord(x, y, drawtype == 0);
}


/////////////////////////////////////////////////////////////////////////////
// class ContourConverter

ContourConverter::ContourConverter()
{
	m_pMF = NULL;
	m_pGrid = NULL;
}

ContourConverter::~ContourConverter()
{
	delete m_pMF;
	delete m_pGrid;
}

/**
 * Set up the class to do draping on a terrain.
 *
 * \param pTerr The terrain you will generate the contour lines on.
 * \param color The colors of the generated lines.
 * \param fHeight The height above the terrain to drape the lines.  Generally
 *		you will want to use a small offset value here, to keep the lines from
 *		colliding with the terrain itself.
 * \return A geometry node which contains the contours.
 */
vtGeom *ContourConverter::Setup(vtTerrain *pTerr, const RGBf &color, float fHeight)
{
	if (!pTerr)
		return NULL;

	// Make a note of this terrain and its attributes
	m_pTerrain = pTerr;
	m_pHF = pTerr->GetHeightFieldGrid3d();
	if (!m_pHF)
		return NULL;
	m_ext = m_pHF->GetEarthExtents();
	m_spacing = m_pHF->GetSpacing();
	m_fHeight = fHeight;

	// Create material and geometry to contain the vector geometry
	vtMaterialArray *pMats = new vtMaterialArray();
	pMats->AddRGBMaterial1(color, false, false, true);

	m_pGeom = new vtGeom;
	m_pGeom->SetName2("Contour Geometry");
	m_pGeom->SetMaterials(pMats);
	pMats->Release();		// pass ownership

	// copy data from our grid to a QuikGrid object
	int nx, ny;
	m_pHF->GetDimensions(nx, ny);
	m_pGrid = new SurfaceGrid(nx,ny);
	int i, j;
	for (i = 0; i < nx; i++)
	{
		for (j = 0; j < ny; j++)
		{
			// use the true elevation, for true contours
			m_pGrid->zset(i, j, m_pHF->GetElevation(i, j, true));
		}
	}
	m_pMF = new vtMeshFactory(m_pGeom, vtMesh::LINE_STRIP, 0, 30000, 0);

	// Since we have to interface to a global callback, set a global
	//  pointer to the recipient of the callback.
	s_cc = this;

	return m_pGeom;
}


/**
 * Generate a contour line to be draped on the terrain.
 *
 * \param fAlt The altitude (elevation) of the line to be generated.
 */
void ContourConverter::GenerateContour(float fAlt)
{
	Contour(*m_pGrid, fAlt);
}

/**
 * Generate a set of contour lines to be draped on the terrain.
 *
 * \param fInterval  The vertical spacing between the contours.  For example,
 *		if the elevation range of your data is from 50 to 350 meters, then
 *		an fIterval of 100 will place contour bands at 100,200,300 meters.
 */
void ContourConverter::GenerateContours(float fInterval)
{
	float fMin, fMax;
	m_pHF->GetHeightExtents(fMin, fMax);

	int start = (int) (fMin / fInterval) + 1;
	int stop = (int) (fMax / fInterval);

	for (int i = start; i <= stop; i++)
	{
		Contour(*m_pGrid, i * fInterval);
	}
}

void ContourConverter::Coord(float x, float y, bool bStart)
{
	if (bStart)
		Flush();

	DPoint2 p2;
	p2.x = m_ext.left + x * m_spacing.x;
	p2.y = m_ext.bottom + y * m_spacing.y;
	m_line.Append(p2);
}

/**
 * Finishes the contour generation process.  Call once when you are done
 * using the class to generate contours.
 */
void ContourConverter::Finish()
{
	Flush();

	// Add the geometry to the terrain's scaled features, so that it will scale
	//  up/down with the terrain's vertical exaggeration.
	m_pTerrain->GetScaledFeatures()->AddChild(m_pGeom);

	s_cc = NULL;
}

void ContourConverter::Flush()
{
	if (m_line.GetSize() > 2)
	{
		bool bInterpolate = false;		// no need; it already hugs the ground
		bool bCurve = false;			// no need; it's already quite smooth
		bool bUseTrueElevation = true;	// use true elevation, not scaled

		m_pTerrain->AddSurfaceLineToMesh(m_pMF, m_line, m_fHeight,
			bInterpolate, bCurve, bUseTrueElevation);
	}
	m_line.Empty();
}

#endif // SUPPORT_QUIKGRID

