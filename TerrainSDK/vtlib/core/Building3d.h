//
// Building3d.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef BUILDING3DH
#define BUILDING3DH

#include "vtdata/Building.h"
#include "vtdata/StructArray.h"
#include "Structure3d.h"

class vtHeightField;

struct MatMesh
{
	int		m_iMatIdx;
	vtMesh	*m_pMesh;
	int		m_iPrimType;
};

class vtBuilding3d : public vtBuilding, public vtStructure3d
{
public:
	vtBuilding3d();
	~vtBuilding3d();

	// implement vtStructure3d methods
	virtual bool CreateNode(vtTerrain *pTerr);
	vtGeom *GetGeom();
	virtual void DeleteNode();
	// display a bounding box around to object to highlight it
	virtual void ShowBounds(bool bShow);

	// copy
	vtBuilding3d &operator=(const vtBuilding &v);

	void DestroyGeometry();
	bool CreateGeometry(vtHeightField3d *pHeightField);
	void AdjustHeight(vtHeightField3d *pHeightField);

	// randomize building properties
	void Randomize(int iStories);

protected:
	bool MakeFacade(vtEdge *pEdge, FLine3 &quad, int stories);

protected:

	// the geometry is composed of several meshes, one for each potential material used
	Array<MatMesh>	m_Mesh;

	vtMesh *FindMatMesh(const vtString &Material, const RGBi &color, int iPrimType);
	// center of the building in world coordinates (the origin of
	// the building's local coordinate system)
	FPoint3 m_center;

	// internal methods
	void UpdateWorldLocation(vtHeightField3d *pHeightField);
	float GetHeightOfStories();
	void CreateUpperPolygon(vtLevel *lev, FLine3 &poly, FLine3 &poly2);

	void CreateEdgeGeometry(vtLevel *pLev, FLine3 &poly1, FLine3 &poly2,
		int iEdge, bool bShowEdge);

	// create special, simple geometry for a level which is uniform
	void CreateUniformLevel(int iLevel, float fHeight, int iHighlightEdge);

	// creates a wall.  base_height is height from base of floor
	// (to make siding texture match up right.)
	void AddWallSection(vtEdge *pEdge, bool bUniform, const FLine3 &quad,
		float h1, float h2, float hf1 = -1.0f);

	void AddHighlightSection(vtEdge *pEdge, const FLine3 &quad);

	//adds a wall section with a door
	void AddDoorSection(vtEdge *pWall, vtEdgeFeature *pFeat,
		const FLine3 &quad);

	//adds a wall section with a window
	void AddWindowSection(vtEdge *pWall, vtEdgeFeature *pFeat,
		const FLine3 &quad);

	void AddWallNormal(vtEdge *pWall, vtEdgeFeature *pFeat,
			const FLine3 &quad);

	void AddFlatRoof(const FLine3 &pp, vtLevel *pLev);
	FPoint3	Normal(const FPoint3 &p0, const FPoint3 &p1, const FPoint3 &p2);

	// Felkel straight skeleton
	float MakeFelkelRoof(const FLine3 &pp, vtLevel *pLev);
	int FindVertex(FPoint2 Point, FLine3 &RoofSection3D, Array<int> &iaVertices);

	vtGeom		*m_pGeom;		// The geometry node which contains the building geometry
	vtGeom		*m_pHighlight;	// The wireframe highlight
};

#endif
