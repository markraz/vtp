//
// DynTerrain class : Dynamically rendering terrain
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef DYNTERRAINH
#define DYNTERRAINH

#include "vtdata/ElevationGrid.h"
#include "HeightField.h"
#include "TerrainErr.h"

class vtDynTerrainGeom : public vtDynGeom, public vtHeightFieldGrid
{
public:
	vtDynTerrainGeom();
	~vtDynTerrainGeom();

	virtual bool Init(vtLocalGrid *pGrid, float fZScale,
				  float fOceanDepth, int &iError) = 0;
	virtual void Init2();
	void BasicInit(vtLocalGrid *pLocalGrid);
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
	bool FindAltitudeAtPoint(const FPoint3 &p3, float &fAltitude,
		FPoint3 *vNormal = NULL);

	// overridables
	virtual void GetLocation(int iX, int iZ, FPoint3 &p) = 0;
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
};


// helper
int vt_log2(int n);

#endif

