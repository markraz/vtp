//
// DynTerrain class : Dynamically rendering terrain
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef DYNTERRAINH
#define DYNTERRAINH

#include "vtdata/ElevationGrid.h"
#include "vtdata/HeightField.h"
#include "TerrainErr.h"

/**
 * This class provides a framework for implementing any kind of dynamic
 * geometry for a heightfield terrain grid.  It is the parent class which
 * contains common fuctionality used by each of the terrain CLOD
 * implementations.
 */
class vtDynTerrainGeom : public vtDynGeom, public vtHeightFieldGrid3d
{
public:
	vtDynTerrainGeom();

	virtual bool Init(vtElevationGrid *pGrid, float fZScale,
				  float fOceanDepth, int &iError) = 0;
	virtual void Init2();
	void BasicInit(vtElevationGrid *pGrid);
	void SetOptions(bool bUseTriStrips, int iBlockArrayDim, int iTextureSize);

	void SetPixelError(float fPixelError);
	float GetPixelError();
	void SetPolygonCount(int iPolygonCount);
	int GetPolygonCount();

	int GetNumDrawnTriangles();

	void SetDetailMaterial(vtMaterial *pApp, float fTiling);
	void EnableDetail(bool bOn);
	bool GetDetail() { return m_bDetailTexture; }
	void SetupTexGen(float fTiling);
	void SetupBlockTexGen(int a, int b);
	void DisableTexGen();

	// overrides for vtDynGeom
	void DoCalcBoundBox(FBox3 &box);
	void DoCull(FPoint3 &eyepos_ogl, IPoint2 window_size, float fov);

	// overrides for HeightField
	void GetChecksum(unsigned char **ppChecksum) const {}
	bool FindAltitudeAtPoint2(const DPoint2 &p, float &fAltitude) const;

	// overrides for HeightField3d
	bool FindAltitudeAtPoint(const FPoint3 &p3, float &fAltitude,
		FPoint3 *vNormal = NULL) const;

	// overridables
	virtual float GetElevation(int iX, int iZ) const = 0;
	virtual void GetLocation(int iX, int iZ, FPoint3 &p) const = 0;
	virtual void DoCulling(FPoint3 &eyepos_ogl, IPoint2 window_size, float fov) = 0;

	// control
	void SetCull(bool bOnOff);
	void CullOnce();

	void PreRender() const;
	void PostRender() const;

	int		m_iTPatchDim;
	int		m_iTPatchSize;		// size of each texture patch in texels

protected:
	// tables for quick conversion from x,y index to output X,Z coordinates
	float	*m_fXLookup, *m_fZLookup;

	// these determine the global level of detail
	// (implementation classes can choose which to obey)
	float	m_fPixelError;
	int		m_iPolygonTarget;

	// statistics
	int m_iTotalTriangles;
	int m_iDrawnTriangles;

	// flags
	bool m_bUseTriStrips;
	bool m_bCulleveryframe;
	bool m_bCullonce;

	// detail texture
	float m_fDetailTiling;
	bool m_bDetailTexture;
	vtMaterial *m_pDetailMat;

protected:
	~vtDynTerrainGeom();
};

#endif	// DYNTERRAINH

