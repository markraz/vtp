//
// Globe.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef GLOBEH
#define GLOBEH

#include "vtdata/Icosa.h"
#include "vtdata/FilePath.h"

class vtTerrainScene;

struct IcoVert
{
	DPoint3 p;
	FPoint2 uv;
};

class IcoGlobe : public DymaxIcosa
{
public:
	IcoGlobe();

	enum Style
	{
		GEODESIC, RIGHT_TRIANGLE, DYMAX_UNFOLD
	};

	void Create(int iTriangleCount, const StringArray &paths,
		const vtString &strImagePrefix, Style style = GEODESIC);
	void SetInflation(float f);
	void SetLighting(bool bLight);
	void AddPoints(DLine2 &points, float fSize);

	void AddTerrainRectangles(vtTerrainScene *pTerrainScene);
	int AddGlobePoints(const char *fname);
	double AddSurfaceLineToMesh(vtMesh *mesh, const DPoint2 &g1, const DPoint2 &g2);
	vtTransform *GetTop() { return m_mgeom; }

protected:
	void CreateMaterials(const StringArray &paths, const vtString &strImagePrefix);

	// these methods create a mesh for each face composed of strips
	void add_face1(vtMesh *mesh, int face, bool second);
	void set_face_verts1(vtMesh *geom, int face, float f);

	// these methods use a right-triangle recursion to create faces
	void add_face2(vtMesh *mesh, int face, bool second, float f);
	void set_face_verts2(vtMesh *geom, int face, float f);
	void add_subface(vtMesh *mesh, int face, int v0, int v1, int v2,
								   bool flip, int depth, float f);
	void refresh_face_positions(vtMesh *mesh, int face, float f);

	int		m_red;
	int		m_yellow;

	vtMovGeom	*m_mgeom;
	vtGeom		*m_geom;
	vtMaterialArray	*m_mats;
	int		m_globe_mat[10];
	vtMesh	*m_mesh[22];

	IcoGlobe::Style m_style;

	// for GEODESIC
	int		m_freq;		// tesselation frequency

	// for RIGHT_TRIANGLE
	int		m_vert;
	Array<IcoVert>	m_rtv[20];	// right-triangle vertices
	int		m_depth;	// tesselation depth
};

vtMovGeom *CreateSimpleEarth(vtString strDataPath);

bool FindIntersection(const FPoint3 &rkOrigin, const FPoint3 &rkDirection,
					  const FSphere& rkSphere,
					  int& riQuantity, FPoint3 akPoint[2]);

void geo_to_xyz(double radius, const DPoint2 &geo, FPoint3 &p);
void geo_to_xyz(double radius, const DPoint2 &geo, DPoint3 &p);
void xyz_to_geo(double radius, const FPoint3 &p, DPoint3 &geo);

#endif
