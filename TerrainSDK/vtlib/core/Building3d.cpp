//
// Building3d.cpp
//
// The vtBuilding3d class extends vtBuilding with the ability to procedurally
// create 3D geometry of the buildings.
//
// Copyright (c) 2001-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"
#include "vtdata/HeightField.h"
#include "vtdata/Triangulate.h"
#include "vtdata/PolyChecker.h"

#include "Light.h"
#include "Terrain.h"
#include "TerrainScene.h"
#include "Building3d.h"
#include "FelkelStraightSkeleton.h"


/////////////////////////////////////////////////////////////////////////////

vtBuilding3d::vtBuilding3d() : vtBuilding()
{
	m_pContainer = NULL;
	m_pGeom = NULL;
	m_pHighlight = NULL;
}

vtBuilding3d::~vtBuilding3d()
{
	// meshes will be automatically deleted by the geometry they're in
}

vtBuilding3d &vtBuilding3d::operator=(const vtBuilding &v)
{
	// just call the copy method of the parent class
	*((vtBuilding*)this) = v;
	return *this;
}


//
// Convert the building's reference point into world coordinates.
//
void vtBuilding3d::UpdateWorldLocation(vtHeightField3d *pHeightField)
{
	// Embed the building in the ground such that the lowest corner of its
	// lowest level is at ground level.
	float base_level = CalculateBaseElevation(pHeightField);

	// Find the center of the building in world coordinates (the origin of
	// the building's local coordinate system)
	DPoint2 center;
	GetBaseLevelCenter(center);
	pHeightField->ConvertEarthToSurfacePoint(center, m_center);
	m_center.y = base_level;
}

float vtBuilding3d::GetHeightOfStories()
{
	float height = 0.0f;

	int levs = m_Levels.GetSize();
	for (int i = 0; i < levs; i++)
		height += m_Levels[i]->m_iStories * m_Levels[i]->m_fStoryHeight;

	return height;
}


void vtBuilding3d::DestroyGeometry()
{
	if (!m_pGeom)	// safety check
		return;

	m_pContainer->RemoveChild(m_pGeom);
	m_pGeom->Release();
	m_pGeom = NULL;
	m_Mesh.Empty();
}

void vtBuilding3d::AdjustHeight(vtHeightField3d *pHeightField)
{
	UpdateWorldLocation(pHeightField);
	m_pContainer->SetTrans(m_center);
}

void vtBuilding3d::CreateUpperPolygon(vtLevel *lev, FLine3 &poly, FLine3 &poly2)
{
	int i, prev, next;

	poly2 = poly;
	int edges = lev->NumEdges();
	for (i = 0; i < edges; i++)
	{
		prev = (i-1 < 0) ? edges-1 : i-1;
		next = (i+1 == edges) ? 0 : i+1;

		FPoint3 p = poly[i];

		int islope1 = lev->GetEdge(prev)->m_iSlope;
		int islope2 = lev->GetEdge(i)->m_iSlope;
		if (islope1 == 90 && islope2 == 90)
		{
			// easy case
			p.y += lev->m_fStoryHeight;
		}
		else
		{
			float slope1 = (islope1 / 180.0f * PIf);
			float slope2 = (islope2 / 180.0f * PIf);

			// get edge vectors
			FPoint3 vec1 = poly[prev] - poly[i];
			FPoint3 vec2 = poly[next] - poly[i];
			vec1.Normalize();
			vec2.Normalize();

			// get perpendicular (upward pointing) vectors
			FPoint3 perp1, perp2;
			perp1.Set(0, 1, 0);
			perp2.Set(0, 1, 0);

			// create rotation matrices to rotate them upward
			FMatrix4 mat1, mat2;
			mat1.Identity();
			mat1.AxisAngle(vec1, -slope1);
			mat2.Identity();
			mat2.AxisAngle(vec2, slope2);

			// create normals
			FPoint3 norm1, norm2;
			mat1.TransformVector(perp1, norm1);
			mat2.TransformVector(perp2, norm2);

			// vector of plane intersection is cross product of their normals
			FPoint3 inter = norm1.Cross(norm2);
			// Test that intersection vector is pointing into the polygon
			// need a better test if we are going to handle downward sloping roofs
			if (inter.y < 0)
				inter = -inter;	// Reverse vector to point upward

			inter.Normalize();
			inter *= (lev->m_fStoryHeight / inter.y);

			p += inter;
		}
		poly2[i] = p;
	}
}

bool vtBuilding3d::CreateGeometry(vtHeightField3d *pHeightField)
{
	PolyChecker PolyChecker;
	int i;
	unsigned int j, k;

	UpdateWorldLocation(pHeightField);

	if (!PolyChecker.IsSimplePolygon(GetLocalFootprint(0)))
		return false;

	// create the edges (walls and roof)
	float fHeight = 0.0f;
	int iLevels = GetNumLevels();

	int level_show = -1, edge_show = -1;
	GetValueInt("level", level_show);
	GetValueInt("edge", edge_show);

	for (i = 0; i < iLevels; i++)
	{
		vtLevel *lev = m_Levels[i];
		const FLine3 &foot = GetLocalFootprint(i);
		unsigned int edges = lev->NumEdges();

		// safety check
		if (foot.GetSize() < 3)
			return false;

		if (lev->IsHorizontal())
		{
			// make flat roof
			AddFlatRoof(foot, lev);
		}
		else if (lev->IsUniform())
		{
			int iHighlightEdge = level_show == i ? edge_show : -1;
			CreateUniformLevel(i, fHeight, iHighlightEdge);
			fHeight += lev->m_iStories * lev->m_fStoryHeight;
		}
		else if (lev->HasSlopedEdges() && edges > 4)
		{
			// For complicated roofs with sloped edges which meet at a
			// roofline of uneven height, we need a sophisticated
			// straight-skeleton solution like Petr Felkel's
			float fRoofHeight = MakeFelkelRoof(foot, lev);
			if (fRoofHeight < 0.0)
			{
				VTLOG("Failed to make Felkel roof - reverting to flat roof\n");
				AddFlatRoof(foot, lev);
			}
			fHeight += fRoofHeight;
		}
		else
		{
			// 'flat roof' for the floor
			AddFlatRoof(foot, lev);

			FLine3 poly = foot;
			FLine3 poly2;

			for (j = 0; j < lev->m_iStories; j++)
			{
				for (k = 0; k < edges; k++)
				{
					poly[k].y = fHeight;
				}
				CreateUpperPolygon(lev, poly, poly2);
				for (k = 0; k < edges; k++)
				{
					bool bShowEdge = (level_show == i && edge_show == (int) k);
					CreateEdgeGeometry(lev, poly, poly2, k, bShowEdge);
				}
				fHeight += lev->m_fStoryHeight;
			}
		}
	}

#if 0 // testing
	const FLine3 &roof = GetLocalFootprint(iLevels-1);	// roof: top level
	vtLevel *roof_lev = m_Levels[iLevels-1];
	float roof_height = (roof_lev->m_fStoryHeight * roof_lev->m_iStories);
#endif

	// wrap in a shape and set materials
	m_pGeom = new vtGeom;
	m_pGeom->SetName2("building-geom");
	vtMaterialArray *pShared = GetSharedMaterialArray();
	m_pGeom->SetMaterials(pShared);

	for (j = 0; j < m_Mesh.GetSize(); j++)
	{
		vtMesh *mesh = m_Mesh[j].m_pMesh;
		int index = m_Mesh[j].m_iMatIdx;
		m_pGeom->AddMesh(mesh, index);
		mesh->Release();	// pass ownership
	}

	// resize bounding box
	if (m_pHighlight)
	{
		bool bEnabled = m_pHighlight->GetEnabled();

		m_pContainer->RemoveChild(m_pHighlight);
		m_pHighlight->Release();

		FSphere sphere;
		m_pGeom->GetBoundSphere(sphere);
		m_pHighlight = CreateBoundSphereGeom(sphere);
		m_pContainer->AddChild(m_pHighlight);

		m_pHighlight->SetEnabled(bEnabled);
	}
	return true;
}


////////////////////////////////////////////////////////////////////////////

//
// Since each set of primitives with a specific material requires its own
// mesh, this method looks up or creates the mesh as needed.
//
vtMesh *vtBuilding3d::FindMatMesh(const vtString &Material,
								  const RGBi &color, vtMesh::PrimType ePrimType)
{
	int mi;
	int VertType;
	RGBf fcolor = color;

	// wireframe is a special case, used for highlight materials
	if (ePrimType == vtMesh::LINE_STRIP)
	{
		mi = FindMatIndex(BMAT_NAME_HIGHLIGHT, fcolor);
		VertType = 0;
	}
	else
	{
		// otherwise, find normal stored material
		if (&Material == NULL)
			mi = FindMatIndex(BMAT_NAME_PLAIN, fcolor);
		else
			mi = FindMatIndex(Material, fcolor);
		VertType = VT_Normals | VT_TexCoords;
	}

	int i, size = m_Mesh.GetSize();
	for (i = 0; i < size; i++)
	{
		if (m_Mesh[i].m_iMatIdx == mi && m_Mesh[i].m_ePrimType == ePrimType)
			return m_Mesh[i].m_pMesh;
	}
	// didn't find it, so we need to make it
	MatMesh mm;
	mm.m_iMatIdx = mi;
	mm.m_ePrimType = ePrimType;

	// Potential Optimization: should calculate how many vertices the building
	// will take.  Even the simplest building will use 20 vertices, for now
	// just use 40 as a reasonable starting point for each mesh.

	mm.m_pMesh = new vtMesh(ePrimType, VertType, 40);
	m_Mesh.Append(mm);
	return mm.m_pMesh;
}

//
// Edges are created from a series of features ("panels", "sections")
//
void vtBuilding3d::CreateEdgeGeometry(vtLevel *pLev, FLine3 &poly1,
	FLine3 &poly2, int iEdge, bool bShowEdge)
{
	int num_edges = pLev->NumEdges();
	int i = iEdge, j = (i+1)%num_edges;

	FLine3 quad(4);

	vtEdge	*pEdge = pLev->GetEdge(iEdge);

	// start with the whole wall section
	quad[0] = poly1[i];
	quad[1] = poly1[j];
	quad[2] = poly2[i];
	quad[3] = poly2[j];

	// length of the edge
	FPoint3 dir1 = quad[1] - quad[0];
	FPoint3 dir2 = quad[3] - quad[2];
	float total_length1 = dir1.Length();
	float total_length2 = dir2.Length();
	if (total_length1 > 0.0f)
		dir1.Normalize();
	if (total_length2 > 0.0f)
		dir2.Normalize();

	if (bShowEdge)
	{
		AddHighlightSection(pEdge, quad);
	}

	// How wide should each feature be?
	// Determine how much space we have for the proportional features after
	// accounting for the fixed-width features
	float fixed_width = pEdge->FixedFeaturesWidth();
	float total_prop = pEdge->ProportionTotal();
	float dyn_width = total_length1 - fixed_width;

	if (pEdge->m_Facade != "")
	{
		// If we can successfully construct the facade, we don't need to
		//  use the edge features.
		if (MakeFacade(pEdge, quad, 1))
			return;
	}

	// build the edge features.
	// point[0] is the first starting point of a panel.
	for (i = 0; i < pEdge->NumFeatures(); i++)
	{
		vtEdgeFeature &feat = pEdge->m_Features[i];

		// determine real width
		float meter_width = 0.0f;
		if (feat.m_width >= 0)
			meter_width = feat.m_width;
		else
			meter_width = (feat.m_width / total_prop) * dyn_width;
		quad[1] = quad[0] + dir1 * meter_width;
		quad[3] = quad[2] + dir2 * (meter_width * total_length2 / total_length1);

		if (feat.m_code == WFC_WALL)
		{
			AddWallNormal(pEdge, &feat, quad);
		}
		if (feat.m_code == WFC_GAP)
		{
			// do nothing
		}
		if (feat.m_code == WFC_POST)
		{
			// TODO
		}
		if (feat.m_code == WFC_WINDOW)
		{
			AddWindowSection(pEdge, &feat, quad);
		}
		if (feat.m_code == WFC_DOOR)
		{
			AddDoorSection(pEdge, &feat, quad);
		}
		quad[0] = quad[1];
		quad[2] = quad[3];
	}
}

/**
 * Creates geometry for a highlighted area (an edge).
 */
void vtBuilding3d::AddHighlightSection(vtEdge *pEdge,
	const FLine3 &quad)
{
	// determine 4 points at corners of wall section
	FPoint3 p0 = quad[0];
	FPoint3 p1 = quad[1];
	FPoint3 p3 = quad[2];
	FPoint3 p2 = quad[3];

	vtMesh *mesh = FindMatMesh(BMAT_NAME_PLAIN, RGBi(255,255,255), vtMesh::LINE_STRIP);

	// determine normal (not used for shading)
	FPoint3 norm = Normal(p0,p1,p2);

	int start =
		mesh->AddVertex(p0 + norm);
	mesh->AddVertex(p1 + norm);
	mesh->AddVertex(p2 + norm);
	mesh->AddVertex(p3 + norm);
	mesh->AddVertex(p0 + norm);
	mesh->AddFan(start, start+1, start+2, start+3, start+4);

	start = mesh->AddVertex(p0);
	mesh->AddVertex(p0 + norm);
	mesh->AddFan(start, start+1);

	start = mesh->AddVertex(p1);
	mesh->AddVertex(p1 + norm);
	mesh->AddFan(start, start+1);

	start = mesh->AddVertex(p2);
	mesh->AddVertex(p2 + norm);
	mesh->AddFan(start, start+1);

	start = mesh->AddVertex(p3);
	mesh->AddVertex(p3 + norm);
	mesh->AddFan(start, start+1);

	norm *= 0.95f;
	mesh = FindMatMesh(BMAT_NAME_PLAIN, RGBi(255,0,0), vtMesh::LINE_STRIP);
	start =
		mesh->AddVertex(p0 + norm);
	mesh->AddVertex(p1 + norm);
	mesh->AddVertex(p2 + norm);
	mesh->AddVertex(p3 + norm);
	mesh->AddVertex(p0 + norm);
	mesh->AddFan(start, start+1, start+2, start+3, start+4);
}

/**
 * Builds a wall, given material index, starting and end points, height, and
 * starting height.
 */
void vtBuilding3d::AddWallSection(vtEdge *pEdge, bool bUniform,
	const FLine3 &quad, float vf1, float vf2, float hf1)
{
	// determine 4 points at corners of wall section
	FPoint3 up1 = (quad[2] - quad[0]);
	FPoint3 up2 = (quad[3] - quad[1]);
	FPoint3 p0 = quad[0] + (up1 * vf1);
	FPoint3 p1 = quad[1] + (up2 * vf1);
	FPoint3 p3 = quad[0] + (up1 * vf2);
	FPoint3 p2 = quad[1] + (up2 * vf2);

	vtMesh *mesh;
	if (bUniform)
		mesh = FindMatMesh(BMAT_NAME_WINDOWWALL, pEdge->m_Color, vtMesh::TRIANGLE_FAN);
	else
		mesh = FindMatMesh(*pEdge->m_pMaterial, pEdge->m_Color, vtMesh::TRIANGLE_FAN);

	// determine normal and primary axes of the face
	FPoint3 norm = Normal(p0, p1, p2);
	FPoint3 axis0, axis1;
	axis0 = p1 - p0;
	axis0.Normalize();
	axis1 = norm.Cross(axis0);

	// determine UVs - special case for window-wall texture
	FPoint2 uv0, uv1, uv2, uv3;
	if (bUniform)
	{
		uv0.Set(0, 0);
		uv1.Set(hf1, 0);
		uv2.Set(hf1, vf2);
		uv3.Set(0, vf2);
	}
	else
	{
		float u1 = (p1 - p0).Dot(axis0);
		float u2 = (p2 - p0).Dot(axis0);
		float u3 = (p3 - p0).Dot(axis0);
		float v2 = (p2 - p0).Dot(axis1);
		vtMaterialDescriptor *md = s_MaterialDescriptors.FindMaterialDescriptor(*pEdge->m_pMaterial, pEdge->m_Color);
		uv0.Set(0, 0);
		uv1.Set(u1, 0);
		uv2.Set(u2, v2);
		uv3.Set(u3, v2);
		if (md != NULL)
		{
			// divide meters by [meters/uv] to get uv
			FPoint2 UVScale = md->GetUVScale();
			uv0.Div(UVScale);
			uv1.Div(UVScale);
			uv2.Div(UVScale);
			uv3.Div(UVScale);
		}
	}

	int start =
		mesh->AddVertexNUV(p0, norm, uv0);
	mesh->AddVertexNUV(p1, norm, uv1);
	mesh->AddVertexNUV(p2, norm, uv2);
	mesh->AddVertexNUV(p3, norm, uv3);

	mesh->AddFan(start, start+1, start+2, start+3);
}

void vtBuilding3d::AddWallNormal(vtEdge *pEdge, vtEdgeFeature *pFeat,
	const FLine3 &quad)
{
	float vf1 = pFeat->m_vf1;
	float vf2 = pFeat->m_vf2;
	AddWallSection(pEdge, false, quad, vf1, vf2);
}

/**
 * Builds a door section.  will also build the wall above the door to ceiling
 * height.
 */
void vtBuilding3d::AddDoorSection(vtEdge *pEdge, vtEdgeFeature *pFeat,
	const FLine3 &quad)
{
	float vf1 = 0;
	float vf2 = pFeat->m_vf2;

	// determine 4 points at corners of section
	FPoint3 up1 = (quad[2] - quad[0]);
	FPoint3 up2 = (quad[3] - quad[1]);
	FPoint3 p0 = quad[0] + (up1 * vf1);
	FPoint3 p1 = quad[1] + (up2 * vf1);
	FPoint3 p3 = quad[0] + (up1 * vf2);
	FPoint3 p2 = quad[1] + (up2 * vf2);

	vtMesh *mesh = FindMatMesh(BMAT_NAME_DOOR, pEdge->m_Color, vtMesh::TRIANGLE_FAN);

	// determine normal (flat shading, all vertices have the same normal)
	FPoint3 norm = Normal(p0, p1, p2);

	int start =
		mesh->AddVertexNUV(p0, norm, FPoint2(0.0f, 0.0f));
	mesh->AddVertexNUV(p1, norm, FPoint2(1.0f, 0.0f));
	mesh->AddVertexNUV(p2, norm, FPoint2(1.0f, 1.0f));
	mesh->AddVertexNUV(p3, norm, FPoint2(0.0f, 1.0f));

	mesh->AddFan(start, start+1, start+2, start+3);

	//add wall above door
	AddWallSection(pEdge, false, quad, vf2, 1.0f);
}

//builds a window section.  builds the wall below and above a window too.
void vtBuilding3d::AddWindowSection(vtEdge *pEdge, vtEdgeFeature *pFeat,
	const FLine3 &quad)
{
	float vf1 = pFeat->m_vf1;
	float vf2 = pFeat->m_vf2;

	// build wall to base of window.
	AddWallSection(pEdge, false, quad, 0, vf1);

	// build wall above window
	AddWallSection(pEdge, false, quad, vf2, 1.0f);

	// determine 4 points at corners of section
	FPoint3 up1 = (quad[2] - quad[0]);
	FPoint3 up2 = (quad[3] - quad[1]);
	FPoint3 p0 = quad[0] + (up1 * vf1);
	FPoint3 p1 = quad[1] + (up2 * vf1);
	FPoint3 p3 = quad[0] + (up1 * vf2);
	FPoint3 p2 = quad[1] + (up2 * vf2);

	vtMesh *mesh = FindMatMesh(BMAT_NAME_WINDOW, pEdge->m_Color, vtMesh::TRIANGLE_FAN);

	// determine normal (flat shading, all vertices have the same normal)
	FPoint3 norm = Normal(p0,p1,p2);

	int start =
		mesh->AddVertexNUV(p0, norm, FPoint2(0.0f, 0.0f));
	mesh->AddVertexNUV(p1, norm, FPoint2(1.0f, 0.0f));
	mesh->AddVertexNUV(p2, norm, FPoint2(1.0f, 1.0f));
	mesh->AddVertexNUV(p3, norm, FPoint2(0.0f, 1.0f));

	mesh->AddFan(start, start+1, start+2, start+3);
}


void vtBuilding3d::AddFlatRoof(const FLine3 &pp, vtLevel *pLev)
{
	FPoint3 up(0.0f, 1.0f, 0.0f);	// vector pointing up
	int corners = pp.GetSize();
	int i, j;
	FPoint2 uv;

	vtEdge *pEdge = pLev->GetEdge(0);
	const vtString& Material = *pEdge->m_pMaterial;
	vtMesh *mesh = FindMatMesh(Material, pEdge->m_Color, vtMesh::TRIANGLES);
	vtMaterialDescriptor *md = s_MaterialDescriptors.FindMaterialDescriptor(Material, pEdge->m_Color);

	if (corners > 4)
	{
		// roof consists of a polygon which must be split into triangles
		FLine2 roof;
		roof.SetMaxSize(corners);
		for (i = 0; i < corners; i++)
			roof.Append(FPoint2(pp[i].x, pp[i].z));

		float roof_y = pp[0].y;

		// allocate a polyline to hold the answer.
		FLine2 result;

		//  Invoke the triangulator to triangulate this polygon.
		Triangulate_f::Process(roof, result);

		// use the results.
		int tcount = result.GetSize()/3;
		int ind[3];
		FPoint2 gp;
		FPoint3 p;

		for (i=0; i<tcount; i++)
		{
			for (j = 0; j < 3; j++)
			{
				gp = result[i*3+j];
				p.Set(gp.x, roof_y, gp.y);
				uv = gp;
				if (md)
					uv.Div(md->GetUVScale());	// divide meters by [meters/uv] to get uv
				ind[j] = mesh->AddVertexNUV(p, up, uv);
			}
			mesh->AddTri(ind[0], ind[2], ind[1]);
		}
	}
	else
	{
		int idx[MAX_WALLS];
		for (i = 0; i < corners; i++)
		{
			FPoint3 p = pp[i];
			uv.Set(p.x, p.z);
			if (md)
				uv.Div(md->GetUVScale());	// divide meters by [meters/uv] to get uv
			idx[i] = mesh->AddVertexNUV(p, up, uv);
		}
		if (corners > 2)
			mesh->AddTri(idx[0], idx[1], idx[2]);
		if (corners > 3)
			mesh->AddTri(idx[2], idx[3], idx[0]);
	}
}


float vtBuilding3d::MakeFelkelRoof(const FLine3 &EavePolygon, vtLevel *pLev)
{
	PolyChecker PolyChecker;
	vtStraightSkeleton StraightSkeleton;
	CSkeleton Skeleton;
	float fMaxHeight = 0.0;
	ContourVector RoofEaves;
	int iVertices = EavePolygon.GetSize();
	int i;
	CSkeletonLine *pStartEdge;
	CSkeletonLine *pEdge;
	CSkeletonLine *pNextEdge;
	bool bEdgeReversed;
	float EaveY = EavePolygon[0].y;
#ifdef FELKELDEBUG
	float DebugX;
	float DebugY;
	float DebugZ;
#endif

	// Make a roof using felkels straight skeleton algorithm

	// First of all build the eave footprint.
	// The algorithm can handle buildings with holes in them (i.e. a courtyard)
	// but at the moment I assume the EavePolygon passed in is the the single
	// outer polygon the roof edges. It must be clockwise oriented!
	RoofEaves.push_back(Contour());

	if (PolyChecker.IsClockwisePolygon(EavePolygon))
	{
		for (i = 0; i < iVertices; i++)
		{
			FPoint3 CurrentPoint = EavePolygon[i];
			FPoint3 NextPoint = EavePolygon[(i+1)%iVertices];
			FPoint3 PreviousPoint = EavePolygon[(iVertices+i-1)%iVertices];
			int iSlope = pLev->GetEdge(i)->m_iSlope;
			if (iSlope > 89)
				iSlope = 90;
			else if (iSlope < 1)
				iSlope = 0;
			int iPrevSlope = pLev->GetEdge((iVertices+i-1)%iVertices)->m_iSlope;
			if (iPrevSlope > 89)
				iPrevSlope = 90;
			else if (iPrevSlope < 1)
				iPrevSlope = 0;
			// If edges are in line and slopes are different then
			if ((iPrevSlope != iSlope)
				&& Collinear2d(PreviousPoint, CurrentPoint, NextPoint))
			{
#ifdef FELKELDEBUG
				VTLOG("Adding dummy eave segment at %d\n", i);
#endif
				// Duplicate the current edge vector
				FPoint3 OldEdge = NextPoint - CurrentPoint;
				FPoint3 NewEdge;
				int iNewSlope;
				if (iSlope > iPrevSlope)
				{
					// Rotate new vertex inwards (clockwise)
					NewEdge.x = OldEdge.z;
					NewEdge.z = -OldEdge.x;
					iNewSlope = iPrevSlope;
				}
				else
				{
					// Rotate new vertext outwards (anticlockwise)
					NewEdge.x = -OldEdge.z;
					NewEdge.z = OldEdge.x;
					iNewSlope = iSlope;
				}
				// Scale to .01 of a co-ord unit
				NewEdge.Normalize();
				NewEdge = NewEdge/100.0f;
				NewEdge += CurrentPoint;
				RoofEaves[0].push_back(CEdge(NewEdge.x, 0, NewEdge.z,
					iNewSlope / 180.0f * PIf, pLev->GetEdge(i)->m_pMaterial,
					pLev->GetEdge(i)->m_Color));
			}
			RoofEaves[0].push_back(CEdge(CurrentPoint.x, 0, CurrentPoint.z,
				iSlope / 180.0f * PIf, pLev->GetEdge(i)->m_pMaterial,
				pLev->GetEdge(i)->m_Color));
		}
	}
	else
	{
		return -1.0;
	}

	// Now build the skeleton
	StraightSkeleton.MakeSkeleton(RoofEaves);

	if (0 == StraightSkeleton.m_skeleton.size())
		return -1.0;

	// Merge the original eaves back into the skeleton
	Skeleton = StraightSkeleton.CompleteWingedEdgeStructure(RoofEaves);

	if (0 == Skeleton.size())
		return -1.0;

#ifdef FELKELDEBUG
	VTLOG("Building Geometry\n");
#endif
	// TODO - texture co-ordinates
	// Build the geometry
	for (size_t ci = 0; ci < RoofEaves.size(); ci++)
	{
		Contour& points = RoofEaves[ci];
		for (size_t pi = 0; pi < points.size(); pi++)
		{
			// For each boundary edge zip round the polygon anticlockwise
			// and build the vertex array
			const vtString bmat = *points[pi].m_pMaterial;
			vtMesh *pMesh = FindMatMesh(bmat, points[pi].m_Color, vtMesh::TRIANGLES);
			vtMaterialDescriptor *pMd = s_MaterialDescriptors.FindMaterialDescriptor(bmat, points[pi].m_Color);
			FPoint2 UVScale;
			if (NULL != pMd)
				UVScale = pMd->GetUVScale();
			else
				UVScale = FPoint2(1.0, 1.0);				
			FLine3 RoofSection3D;
			FLine3 TriangulatedRoofSection3D;
			int iTriangleCount = 0;
			FPoint3 PanelNormal;
			FPoint3 UAxis;
			FPoint3 VAxis;
			FPoint3 TextureOrigin;
			int i, j;
			vtArray<int> iaVertices;

			C3DPoint& p1 = points[pi].m_Point;
			C3DPoint& p2 = points[(pi+1)%points.size()].m_Point;
			// Find the starting edge
			CSkeleton::iterator s1;
			for (s1 = Skeleton.begin(); s1 != Skeleton.end(); s1++)
			{
				if (((*s1).m_lower.m_vertex->m_point == p1) &&
					((*s1).m_higher.m_vertex->m_point == p2))
					break;
			}
			if (s1 == Skeleton.end())
				break;

			pStartEdge = &(*s1);
			pEdge = pStartEdge;
			bEdgeReversed = false;
#ifdef FELKELDEBUG
			VTLOG("Building panel\n");
#endif
			unsigned int iNumberofPoints = 0;
			do
			{
				if (iNumberofPoints++ > Skeleton.size())
				{
					VTLOG("MakeFelkelRoof - Roof geometry too complex - giving up\n");
					return -1.0;
				}
				if (bEdgeReversed)
				{
#ifdef FELKELDEBUG
					DebugX = pEdge->m_higher.m_vertex->m_point.m_x;
					DebugY = pEdge->m_higher.m_vertex->m_point.m_y;
					DebugZ = pEdge->m_higher.m_vertex->m_point.m_z;
#endif
					if (pEdge->m_higher.m_vertex->m_point.m_z > (double)fMaxHeight)
						fMaxHeight = (float) pEdge->m_higher.m_vertex->m_point.m_z;
					RoofSection3D.Append(FPoint3(pEdge->m_higher.m_vertex->m_point.m_x, pEdge->m_higher.m_vertex->m_point.m_y + EaveY, pEdge->m_higher.m_vertex->m_point.m_z));
					pNextEdge = pEdge->m_higher.m_right;
//					if (pEdge->m_higher.m_vertex->m_point != pNextEdge->m_higher.m_vertex->m_point)
					if (pEdge->m_higher.VertexID() != pNextEdge->m_higher.VertexID())
						bEdgeReversed = true;
					else
						bEdgeReversed = false;
				}
				else
				{
#ifdef FELKELDEBUG
					DebugX = pEdge->m_lower.m_vertex->m_point.m_x;
					DebugY = pEdge->m_lower.m_vertex->m_point.m_y;
					DebugZ = pEdge->m_lower.m_vertex->m_point.m_z;
#endif
					if (pEdge->m_lower.m_vertex->m_point.m_z > (double)fMaxHeight)
						fMaxHeight = (float) pEdge->m_lower.m_vertex->m_point.m_z;
					RoofSection3D.Append(FPoint3(pEdge->m_lower.m_vertex->m_point.m_x, pEdge->m_lower.m_vertex->m_point.m_y + EaveY, pEdge->m_lower.m_vertex->m_point.m_z));
					pNextEdge = pEdge->m_lower.m_right;
//					if (pEdge->m_lower.m_vertex->m_point != pNextEdge->m_higher.m_vertex->m_point)
					if (pEdge->m_lower.VertexID() != pNextEdge->m_higher.VertexID())
						bEdgeReversed = true;
					else
						bEdgeReversed = false;
				}
#ifdef FELKELDEBUG
				VTLOG("Adding point (ID %d) x %e y %e z %e\n", pEdge->m_ID, DebugX, DebugY, DebugZ);
#endif
				pEdge = pNextEdge;
			}
			// For some reason the pointers dont end up quite the same
			// I will work it out someday
			while (pEdge->m_ID != pStartEdge->m_ID);


			// Remove any vertices that are the same
			for (i = 0; i < (int)RoofSection3D.GetSize(); i++)
			{
				FPoint3& Point = RoofSection3D[i];

				for (j = i + 1; j < (int)RoofSection3D.GetSize(); j++)
				{
					FPoint3& NextPoint = RoofSection3D[j];
					if (NextPoint == Point)
					{
						RoofSection3D.RemoveAt(j);
						j--;
					}
				}
			}


			// determine normal and primary axes of the face
			j = RoofSection3D.GetSize();
			PanelNormal = Normal(RoofSection3D[1], RoofSection3D[0], RoofSection3D[j-1]);
			UAxis = FPoint3(RoofSection3D[j-1] - RoofSection3D[0]).Normalize();
			VAxis = PanelNormal.Cross(UAxis);
			TextureOrigin = RoofSection3D[0];
#ifdef FELKELDEBUG
			VTLOG("Panel normal x %e y %e z %e\n", PanelNormal.x, PanelNormal.y, PanelNormal.z);
#endif
			// Build transform to rotate plane parallel to the xz plane.
			// N.B. this only work with angles from the plane normal to the y axis
			// in the rangle 0 to pi/2 (this is ok for roofs). If you want
			// it to work over a greater range you will have to mess with the sign of the cosine
			// of this angle.
			float fHypot = sqrtf(PanelNormal.x * PanelNormal.x + PanelNormal.z * PanelNormal.z);
			FMatrix3 Transform;
			Transform.SetRow(0, PanelNormal.x * PanelNormal.y / fHypot, PanelNormal.x, -PanelNormal.z / fHypot);
			Transform.SetRow(1, -fHypot, PanelNormal.y, 0);
			Transform.SetRow(2, PanelNormal.z * PanelNormal.y / fHypot, PanelNormal.z, PanelNormal.x / fHypot);

			// Build vertex list
			for (i = 0; i < j; i++)
			{
				FPoint3 Vertex = RoofSection3D[i];
				FPoint2 UV = FPoint2((Vertex - TextureOrigin).Dot(UAxis), (Vertex - TextureOrigin).Dot(VAxis));
				UV.Div(UVScale);
				iaVertices.Append(pMesh->AddVertexNUV(Vertex, PanelNormal, UV));
			}

			for (i = 0; i < j; i++)
			{
				// Source and dest cannot be the same
				FPoint3 Temp = RoofSection3D[i];
				Transform.Transform(Temp, RoofSection3D[i]);
			}

			Triangulate_f::Process(RoofSection3D, TriangulatedRoofSection3D);
			iTriangleCount = TriangulatedRoofSection3D.GetSize() / 3;

			for (i = 0; i < iTriangleCount; i++)
			{
				int iaIndex[3];

				for (j = 0; j < 3; j++)
				{
					FPoint3 Point = TriangulatedRoofSection3D[i * 3 + j];
					if (-1 == (iaIndex[j] = FindVertex(Point, RoofSection3D, iaVertices)))
						return -1.0;
				}
				pMesh->AddTri(iaIndex[0], iaIndex[2], iaIndex[1]);
#ifdef FELKELDEBUG
				VTLOG("AddTri1 %d %d %d\n", iaIndex[0], iaIndex[2], iaIndex[1]);
#endif
			}
		}
	}

	return fMaxHeight;
}

bool vtBuilding3d::Collinear2d(const FPoint3& Previous, const FPoint3& Current, const FPoint3& Next)
{
	FPoint3 l1 = Previous - Current;
	FPoint3 l2 = Next - Current;

	l1.y = 0;
	l2.y = 0;

	l1.Normalize();
	l2.Normalize();

	float CosTheta = l1.Dot(l2);
	if (CosTheta < -1.0)
		CosTheta = -1.0;
	else if (CosTheta > 1.0)
		CosTheta = 1.0;
	float fTheta = acosf(CosTheta) / PIf * 180;

	if (fabs(fTheta - 180.0) < 1.0)
		return true;
	else
		return false;
}

int vtBuilding3d::FindVertex(FPoint3 Point, FLine3 &RoofSection3D,
	vtArray<int> &iaVertices)
{
	int iSize = RoofSection3D.GetSize();

	int i;
	for (i = 0; i < iSize; i++)
	{
		if ((Point.x == RoofSection3D[i].x) &&
			(Point.y == RoofSection3D[i].y) &&
			(Point.z == RoofSection3D[i].z))
			break;
	}
	if (i < iSize)
		return iaVertices[i];
	else
	{
		VTLOG("FindVertex - vertex not found\n");
		return -1;
	}
}

//
// Walls which consist of regularly spaced windows and 'siding' material
// can be modelled far more efficiently.  This is very useful for rendering
// speed for large scenes in which the user doesn't have or doesn't care
// about the exact material/windows of the buildings.  We create
// optimized geometry in which each whole wall is a single quad.
//
void vtBuilding3d::CreateUniformLevel(int iLevel, float fHeight,
	int iHighlightEdge)
{
	vtLevel *pLev = m_Levels[iLevel];
	FLine3 poly1 = GetLocalFootprint(iLevel);
	FLine3 poly2;

	int i;
	int edges = pLev->NumEdges();
	for (i = 0; i < edges; i++)
		poly1[i].y = fHeight;

	poly2 = poly1;
	for (i = 0; i < edges; i++)
		poly2[i].y += pLev->m_fStoryHeight;

	for (i = 0; i < edges; i++)
	{
		int a = i, b = (a+1)%edges;

		FLine3 quad(4);

		vtEdge	*pEdge = pLev->GetEdge(i);

		// do the whole wall section
		quad[0] = poly1[a];
		quad[1] = poly1[b];
		quad[2] = poly2[a];
		quad[3] = poly2[b];

		if (pEdge->m_Facade != "")
		{
			float extraheight = pLev->m_fStoryHeight * (pLev->m_iStories-1);
			quad[2].y += extraheight;
			quad[3].y += extraheight;
			// If we can successfully construct the facade, we don't need to
			//  use the edge features.
			if (MakeFacade(pEdge, quad, pLev->m_iStories))
				continue;
		}
		quad[2] = poly2[a];
		quad[3] = poly2[b];

		float h1 = 0.0f;
		float h2 = (float) pLev->m_iStories;
		float hf1 = (float) pEdge->NumFeaturesOfCode(WFC_WINDOW);
		AddWallSection(pEdge, true, quad, h1, h2, hf1);

		if (i == iHighlightEdge)
		{
			for (unsigned int j = 0; j < pLev->m_iStories; j++)
			{
				AddHighlightSection(pEdge, quad);
				quad[0].y += pLev->m_fStoryHeight;
				quad[1].y += pLev->m_fStoryHeight;
				quad[2].y += pLev->m_fStoryHeight;
				quad[3].y += pLev->m_fStoryHeight;
			}
		}
	}
}

bool vtBuilding3d::MakeFacade(vtEdge *pEdge, FLine3 &quad, int stories)
{
	// Paint a facade on this edge
	// Add the facade image to the materials array
	// Assume quad is ordered 0,1,3,2
	vtString fname = "BuildingModels/";
	MatMesh mm;
	FPoint3 norm = Normal(quad[0],quad[1],quad[3]);

	fname += pEdge->m_Facade;
	fname = FindFileOnPaths(vtGetDataPath(), (pcchar)fname);
	if (fname == "")
	{
		VTLOG(" Couldn't find facade texture '%s'\n", (const char*)pEdge->m_Facade);
		return false;
	}

	mm.m_iMatIdx = GetSharedMaterialArray()->AddTextureMaterial2(fname,
			true, true, false, false,
			TERRAIN_AMBIENT,
			TERRAIN_DIFFUSE,
			1.0f,		// alpha
			TERRAIN_EMISSIVE);

	// Create a mesh for the new material and add this to the mesh array
	mm.m_pMesh = new vtMesh(vtMesh::TRIANGLE_FAN, VT_Normals | VT_TexCoords, 6);
	m_Mesh.Append(mm);

	// Calculate the vertices and add them to the mesh
	float v = (float) stories;
	int start = mm.m_pMesh->AddVertexNUV(quad[0], norm, FPoint2(0.0f, 0.0f));
	mm.m_pMesh->AddVertexNUV(quad[1], norm, FPoint2(1.0f, 0.0f));
	mm.m_pMesh->AddVertexNUV(quad[3], norm, FPoint2(1.0f, v));
	mm.m_pMesh->AddVertexNUV(quad[2], norm,  FPoint2(0.0f, v));

	mm.m_pMesh->AddFan(start, start+1, start+2, start+3);
	return true;
}

FPoint3 vtBuilding3d::Normal(const FPoint3 &p0, const FPoint3 &p1, const FPoint3 &p2)
{
	FPoint3 a = p0 - p1;
	FPoint3 b = p2 - p1;
	FPoint3 norm = b.Cross(a);
	norm.Normalize();
	return norm;
}

//
// Randomize buildings characteristics
//
void vtBuilding3d::Randomize(int iStories)
{
	RGBi color;

	color = GetColor(BLD_BASIC);
	if (color.r == -1 && color.g == -1 && color.b == -1)
	{
		// unset color
		// random pastel color
		unsigned char r, g, b;
		r = (unsigned char) (128 + random(127));
		g = (unsigned char) (128 + random(127));
		b = (unsigned char) (128 + random(127));
		SetColor(BLD_BASIC, RGBi(r, g, b));
	}

	color = GetColor(BLD_ROOF);
	if (color.r == -1 && color.g == -1 && color.b == -1)
	{
		// unset color
		// random roof color
		int r = rand() %5;
		switch (r) {
			case 0: color.Set(255, 255, 250); break;	//off-white
			case 1: color.Set(153, 51, 51); break;		//reddish
			case 2: color.Set(153, 153, 255); break;	//blue-ish
			case 3: color.Set(153, 255, 153); break;	//green-ish
			case 4: color.Set(178, 102, 51); break;		//brown
		}

		SetColor(BLD_ROOF, color);
	}
}


/**
 * Creates the geometry for the building.
 * Capable of several levels of detail (defaults to full detail).
 * If the geometry was already built previously, it is destroyed and re-created.
 *
 * \param pTerr The terrain on which to plant the building.
 */
bool vtBuilding3d::CreateNode(vtTerrain *pTerr)
{
	if (m_pContainer)
	{
		// was build before; re-build geometry
		DestroyGeometry();
	}
	else
	{
		// constructing for the first time
		m_pContainer = new vtTransform;
		m_pContainer->SetName2("building container");
	}
	if (!CreateGeometry(pTerr->GetHeightField()))
		return false;
	m_pContainer->AddChild(m_pGeom);
	m_pContainer->SetTrans(m_center);
	return true;
}

void vtBuilding3d::DeleteNode()
{
	if (m_pContainer)
	{
		DestroyGeometry();
		m_pContainer->Release();
		m_pContainer = NULL;
	}
}

/**
 * Display some bounding wires around the object to highlight it.
 */
void vtBuilding3d::ShowBounds(bool bShow)
{
	if (bShow)
	{
		if (!m_pHighlight)
		{
			// the highlight geometry doesn't exist, so create it
			// get bounding sphere
			FSphere sphere;
			m_pGeom->GetBoundSphere(sphere);

			m_pHighlight = CreateBoundSphereGeom(sphere);
			m_pContainer->AddChild(m_pHighlight);
		}
		m_pHighlight->SetEnabled(true);
	}
	else
	{
		if (m_pHighlight)
			m_pHighlight->SetEnabled(false);
	}
}