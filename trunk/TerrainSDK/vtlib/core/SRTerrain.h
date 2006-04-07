//
// SRTerrain class : a subclass of vtDynTerrainGeom which exposes
//  Stefan Roettger's CLOD algorithm.
//
// Utilizes: Roettger's MINI library implementation
// http://stereofx.org/#Terrain
//
// Copyright (c) 2002-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef SRTERRAINH
#define SRTERRAINH

#include "DynTerrain.h"

/** \addtogroup dynterr */
/*@{*/

/*!
	The SRTerrain class implements Stefan Roettger's algorithm for
	regular-grid terrain LOD.  It was adapted directly from his sample
	implementation and correspondence with him.
*/
class SRTerrain : public vtDynTerrainGeom
{
public:
	SRTerrain();

	// initialization
	DTErr Init(const vtElevationGrid *pGrid, float fZScale);

	// overrides
	void DoRender();
	void DoCulling(const vtCamera *pCam);
	float GetElevation(int iX, int iZ, bool bTrue = false) const;
	void GetWorldLocation(int iX, int iZ, FPoint3 &p, bool bTrue = false) const;
	void SetVerticalExag(float fExag);
	void SetPolygonTarget(int iCount);

	void LoadSingleMaterial();
	void LoadBlockMaterial(int a, int b);

	int		m_iBlockSize;

	float m_fResolution;
	float m_fHResolution;
	float m_fLResolution;

protected:
	// rendering
	void RenderSurface();
	void RenderPass();

	// cleanup
	virtual ~SRTerrain();

private:
	class ministub *m_pMini;

	IPoint2 m_window_size;
	FPoint3 m_eyepos_ogl;
	float m_fFOVY;
	float m_fAspect;
	float m_fNear, m_fFar;
	FPoint3 eye_up, eye_forward;

	float m_fHeightScale;
	float m_fMaximumScale;
	float m_fDrawScale;

};

/*@}*/	// Group dynterr

#endif	// SRTerrain
