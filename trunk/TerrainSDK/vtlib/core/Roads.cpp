//
// Roads.cpp
//
// also shorelines and rivers
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"
#include "vtlib/core/Light.h"
#include "Roads.h"

#define ROAD_HEIGHT			(vtRoadMap3d::s_fHeight)	// height about the ground
#define ROADSIDE_WIDTH		2.0f
#define ROADSIDE_DEPTH		-ROADSIDE_WIDTH

#define UV_SCALE_ROAD		(.08f)
#define UV_SCALE_SIDEWALK	(1.00f)

#define TEXTURE_ARGS(alpha)		true, true, alpha, false, TERRAIN_AMBIENT, \
	TERRAIN_DIFFUSE, 1.0f, TERRAIN_EMISSIVE, false, false

#define ROADTEXTURE_4WD		"GeoTypical/road_4wd1.png"
#define ROADTEXTURE_TRAIL	"GeoTypical/trail1.png"

////////////////////////////////////////////////////////////////////

// helper
FPoint3 find_adjacent_roadpoint(LinkGeom *pR, NodeGeom *pN)
{
	if (pR->GetNode(0) == pN)
		return pR->m_p3[1];
	else if (pR->GetNode(1) == pN)
		return pR->m_p3[pR->GetSize() - 2];
	else
	{
		// Adjacent road point not found!
		return FPoint3(-1, -1,-1);
	}
}

//
// return the positive difference of two angles (a - b)
// allow for wrapping around 2 PI
//
float angle_diff(float a, float b)
{
	if (a > b)
		return a-b;
	else
		return PI2f+a-b;
}

//
// helper: given two points along a road, produce a vector
// along to that road, parallel to the ground plane,
// with length corresponding to the supplied width
//
FPoint3 CreateRoadVector(FPoint3 p1, FPoint3 p2, float w)
{
	FPoint3 v = p2 - p1;
	v.y = 0;
	v.Normalize();
	v.x *= (w / 2.0f);
	v.z *= (w / 2.0f);
	return v;
}

FPoint3 CreateUnitRoadVector(FPoint3 p1, FPoint3 p2)
{
	FPoint3 v = p2 - p1;
	v.y = 0;
	v.Normalize();
	return v;
}


/////////////////////////////////////////////////////////////////////////

NodeGeom::NodeGeom()
{
	m_iVerts = 0;
	m_v = NULL;
	m_Lights = NULL;
}

NodeGeom::~NodeGeom()
{
	if (m_v) delete m_v;
}

FPoint3 NodeGeom::GetRoadVector(int i)
{
	LinkGeom *pR = GetRoad(i);
	FPoint3 pn1 = find_adjacent_roadpoint(pR, this);
	return CreateRoadVector(m_p3, pn1, pR->m_fWidth);
}

FPoint3 NodeGeom::GetUnitRoadVector(int i)
{
	FPoint3 pn1 = find_adjacent_roadpoint(GetRoad(i), this);
	return CreateUnitRoadVector(m_p3, pn1);
}


// statistics
int one = 0, two = 0, many = 0;

void NodeGeom::BuildIntersection()
{
	FPoint3 v, v_next, v_prev;
	FPoint3 pn0, pn1, upvector(0.0f, 1.0f, 0.0f);
	float w;				// road width

	SortLinksByAngle();

	// how many roads meet here?
	if (m_iLinks == 0)
	{
		// bogus case
		int foo = 1;
	}
	else if (m_iLinks == 1)
	{
		// dead end: only need 2 vertices for this node
		m_iVerts = 2;
		m_v = new FPoint3[2];

		// get info about the road
		LinkGeom *r = GetRoad(0);
		w = r->m_fWidth;

		pn1 = find_adjacent_roadpoint(r, this);
		v = CreateRoadVector(m_p3, pn1, w);

		m_v[0].Set(m_p3.x + v.z, m_p3.y + ROAD_HEIGHT, m_p3.z - v.x);
		m_v[1].Set(m_p3.x - v.z, m_p3.y + ROAD_HEIGHT, m_p3.z + v.x);

		one++;
	}
	else if (m_iLinks == 2)
	{
		// only need 2 vertices for this node; no intersection
		m_iVerts = 2;
		m_v = new FPoint3[2];

		// get info about the roads
		w = (GetRoad(0)->m_fWidth + GetRoad(1)->m_fWidth) / 2.0f;
		if (m_id == 8311) {
			int hello = 1;
		}
		pn0 = find_adjacent_roadpoint(GetRoad(0), this);
		pn1 = find_adjacent_roadpoint(GetRoad(1), this);

		v = CreateRoadVector(pn0, pn1, w);

		m_v[0].Set(m_p3.x + v.z, m_p3.y + ROAD_HEIGHT, m_p3.z - v.x);
		m_v[1].Set(m_p3.x - v.z, m_p3.y + ROAD_HEIGHT, m_p3.z + v.x);

		two++;
	}
	else
	{
		// intersection: need 2 vertices for each road meeting here
		m_iVerts = 2 * m_iLinks;
		m_v = new FPoint3[m_iVerts];

		// for each road
		for (int i = 0; i < m_iLinks; i++)
		{
			// indices of the next and previous roads
			int i_next = (i == m_iLinks-1) ? 0 : i+1;
			int i_prev = (i == 0) ? m_iLinks-1 : i-1;

			// angle between this road and the next and previous roads
			float a_next = angle_diff(m_fLinkAngle[i_next], m_fLinkAngle[i]);
			float a_prev = angle_diff(m_fLinkAngle[i], m_fLinkAngle[i_prev]);

			// get info about the roads
			Link *pR = GetRoad(i);
			Link *pR_next = GetRoad(i_next);
			Link *pR_prev = GetRoad(i_prev);
			w = pR->m_fWidth;
			float w_next = pR_next->m_fWidth;
			float w_prev = pR_prev->m_fWidth;

			// find corner between this road and next
			v = GetUnitRoadVector(i);

			float w_avg;
			w_avg = (w + w_next) / 2.0f;
			float offset_next = w_avg / tanf(a_next / 2.0f);
			float offset_prev = w_avg / tanf(a_prev / 2.0f);
			float offset_largest = MAX(offset_next, offset_prev);
			offset_largest += 2.0f;

			pn0 = m_p3;
			pn0 += (v * (offset_largest / 2.0f));
			v *= (w/2.0f);

			m_v[i * 2 + 0].Set(pn0.x + v.z, pn0.y + ROAD_HEIGHT, pn0.z - v.x);
			m_v[i * 2 + 1].Set(pn0.x - v.z, pn0.y + ROAD_HEIGHT, pn0.z + v.x);
		}
		many++;
	}
}


//
// Given a node and a road, return the two points that the road
// will need in order to hook up with the node.
//
void NodeGeom::FindVerticesForRoad(Link *pR, FPoint3 &p0, FPoint3 &p1)
{
	if (m_iLinks == 1)
	{
		p0 = m_v[0];
		p1 = m_v[1];
	}
	else if (m_iLinks == 2)
	{
		if (pR == m_r[0])
		{
			p0 = m_v[1];
			p1 = m_v[0];
		}
		else
		{
			p0 = m_v[0];
			p1 = m_v[1];
		}
	}
	else
	{
		int i;
		for (i = 0; i < m_iLinks; i++)
			if (m_r[i] == pR)
				break;
		if (i == m_iLinks)
		{
			// bad case!  This node does not reference the road passed
			int foo = 1;
		}
		p0 = m_v[i*2];
		p1 = m_v[i*2+1];
	}
}


vtMesh *NodeGeom::GenerateGeometry()
{
	if (m_iLinks < 3)
		return NULL;

#if 1
	// Intersections currently look terrible, and are buggy.
	// Turn them off completely until we can implement decent ones.
	return NULL;
#else

	int j;
	FPoint3 p, upvector(0.0f, 1.0f, 0.0f);

	vtMesh *pMesh = new vtMesh(GL_TRIANGLES, VT_TexCoords | VT_Normals, (m_iLinks*2+1)*2);
	int verts = 0;

	// find the center of the junction
#if 0
	p.Set(0.0f, 0.0f, 0.0f);
	for (j = 0; j < m_iLinks*2; j++)
		p += m_v[j];
	p *= (1.0f / ((float)m_iLinks*2));
#else
	p = m_p3;
	p.y += ROAD_HEIGHT;
#endif

	for (int times = 0; times < 2; times++)
	{
		pMesh->SetVtxPUV(verts, p, 0.5, 0.5f);
		pMesh->SetVtxNormal(verts, upvector);
		verts++;

		for (j = 0; j < m_iLinks; j++)
		{
			if (times)
			{
				pMesh->SetVtxPUV(verts, m_v[j*2+1], 0.0, 0.0f);
				pMesh->SetVtxPUV(verts+1, m_v[j*2], 0.0, 1.0f);
			}
			else
			{
				pMesh->SetVtxPUV(verts, m_v[j*2+1], 0.0, 1.0f);
				pMesh->SetVtxPUV(verts+1, m_v[j*2], 1.0, 1.0f);
			}
			pMesh->SetVtxNormal(verts, upvector);
			pMesh->SetVtxNormal(verts+1, upvector);
			verts += 2;
		}
	}

	int iNVerts = m_iLinks*2+1;
	int iInCircle = m_iLinks*2;

	// create triangles
	int idx[3];
	for (j = 0; j < m_iLinks; j++)
	{
		idx[0] = 0;
		idx[1] = (j*2+1);
		idx[2] = (j*2+2);
		pMesh->AddTri(idx[0], idx[1], idx[2]);
	}
	for (j = 0; j < m_iLinks; j++)
	{
		idx[0] = iNVerts + 0;
		idx[1] = iNVerts + (j*2+1) + 1;
		idx[2] = iNVerts + ((j*2+2) % iInCircle) + 1;
		pMesh->AddTri(idx[0], idx[1], idx[2]);
	}
	return pMesh;
#endif
}


////////////////////////////////////////////////////////////////////////

LinkGeom::LinkGeom()
{
	m_p3 = NULL;
	m_pLanes = NULL;
}

LinkGeom::~LinkGeom()
{
	if (m_p3)
		delete m_p3;
	if (m_pLanes)
		delete [] m_pLanes;
}

RoadBuildInfo::RoadBuildInfo(int iCoords)
{
	left = new FPoint3[iCoords];
	right = new FPoint3[iCoords];
	center = new FPoint3[iCoords];
	crossvector = new FPoint3[iCoords];
	fvLength = new float[iCoords];
	verts = vert_index = 0;
}

RoadBuildInfo::~RoadBuildInfo()
{
	// clean up our temporary storage
	delete right;
	delete left;
	delete center;
	delete crossvector;
	delete fvLength;
}

void LinkGeom::SetupBuildInfo(RoadBuildInfo &bi)
{
	FPoint3 v, pn0, pn1, pn2;
	float length = 0.0f;

	//  for each point in the road, determine coordinates
	for (int j = 0; j < GetSize(); j++)
	{
		if (j > 0)
		{
			// increment length
			v.x = m_p3[j].x - m_p3[j-1].x;
			v.z = m_p3[j].z - m_p3[j-1].z;
			length += v.Length();
		}
		bi.fvLength[j] = length;

		// we will add 2 vertices to the road mesh
		FPoint3 p0, p1;
		if (j == 0)
		{
			// add 2 vertices at this point, copied from the start node
			NodeGeom *pN = GetNode(0);
			pN->FindVerticesForRoad(this, bi.right[j], bi.left[j]);
		}
		if (j > 0 && j < GetSize()-1)
		{
			pn0 = m_p3[j-1];
			pn1 = m_p3[j];
			pn2 = m_p3[j+1];

			// add 2 vertices at this point, directed at the previous and next points
			v = CreateRoadVector(pn0, pn2, m_fWidth);

			bi.right[j].Set(pn1.x + v.z, pn1.y + ROAD_HEIGHT, pn1.z - v.x);
			bi.left[j].Set(pn1.x - v.z, pn1.y + ROAD_HEIGHT, pn1.z + v.x);
		}
		if (j == GetSize()-1)
		{
			// add 2 vertices at this point, copied from the end node
			NodeGeom *pN = GetNode(1);
			pN->FindVerticesForRoad(this, bi.left[j], bi.right[j]);
		}
		bi.crossvector[j] = bi.right[j] - bi.left[j];
		bi.center[j] = bi.left[j] + (bi.crossvector[j] * 0.5f);
		bi.crossvector[j].Normalize();
	}
}

void LinkGeom::AddRoadStrip(vtMesh *pMesh, RoadBuildInfo &bi,
							float offset_left, float offset_right,
							float height_left, float height_right,
							VirtualTexture &vt,
							float u1, float u2, float uv_scale,
							normal_direction nd)
{
	FPoint3 local0, local1, normal;
	float texture_v;
	FPoint2 uv;

	for (int j = 0; j < GetSize(); j++)
	{
		texture_v = bi.fvLength[j] * uv_scale;

		local0 = bi.center[j] + (bi.crossvector[j] * offset_left);
		local1 = bi.center[j] + (bi.crossvector[j] * offset_right);
		local0.y += height_left;
		local1.y += height_right;

		if (nd == ND_UP)
			normal.Set(0.0f, 1.0f, 0.0f);	// up
		else if (nd == ND_LEFT)
			normal = (bi.crossvector[j] * -1.0f);	// left
		else
			normal = bi.crossvector[j];		// right

		vt.Adapt(FPoint2(u2, texture_v), uv);
		pMesh->AddVertexUV(local1, uv);

		vt.Adapt(FPoint2(u1, texture_v), uv);
		pMesh->AddVertexUV(local0, uv);

		pMesh->SetVtxNormal(bi.verts, normal);
		pMesh->SetVtxNormal(bi.verts+1, normal);
		bi.verts += 2;
	}
	// create tristrip
	pMesh->AddStrip2(GetSize() * 2, bi.vert_index);
	bi.vert_index += (GetSize() * 2);
}

void LinkGeom::GenerateGeometry(vtRoadMap3d *rmgeom)
{
	int j;

	if (GetSize() < 2 || (m_pNode[0] == m_pNode[1]))
		return;

	bool do_roadside = true;
	switch (m_Surface)
	{
	case SURFT_DIRT:
	case SURFT_2TRACK:
	case SURFT_TRAIL:
	case SURFT_GRAVEL:
		do_roadside = false;
		break;
	}
	do_roadside = false;	// temporary override
	if (m_iHwy > 0)
		m_iFlags |= RF_MARGIN;

	// calculate total vertex count for this geometry
	int total_vertices = GetSize() * 2;	// main surface
	if (m_iFlags & RF_MARGIN)
		total_vertices += (GetSize() * 2 * 2);	// 2 margin strips
	if (m_iFlags & RF_PARKING)
		total_vertices += (GetSize() * 2 * 2);	// 2 parking strips
	if (m_iFlags & RF_SIDEWALK)
		total_vertices += (GetSize() * 2 * 4);	// 4 sidewalk strips
	if (do_roadside)
		total_vertices += (GetSize() * 2 * 2);		// 2 roadside strips

	vtMesh *pMesh = new vtMesh(GL_TRIANGLE_STRIP, VT_TexCoords | VT_Normals,
		total_vertices);

	RoadBuildInfo bi(GetSize());
	SetupBuildInfo(bi);

	float offset = -m_fWidth/2;
	if (m_iFlags & RF_MARGIN)
		offset -= MARGIN_WIDTH;
	if (m_iFlags & RF_PARKING)
		offset -= PARKING_WIDTH;
	if (m_iFlags & RF_SIDEWALK)
		offset -= SIDEWALK_WIDTH;
	if (do_roadside)
		offset -= ROADSIDE_WIDTH;

#if 0
	// create left roadside strip
	if (do_roadside)
	{
		AddRoadStrip(pMesh, bi,
					offset, offset+ROADSIDE_WIDTH,
					ROADSIDE_DEPTH,
					(m_iFlags & RF_SIDEWALK) ? CURB_HEIGHT : 0.0f,
					rmgeom->m_vt[],
					0.02f, 0.98f, UV_SCALE_ROAD,
					ND_UP);
		offset += ROADSIDE_WIDTH;
	}
#endif

	// create left sidwalk
	if (m_iFlags & RF_SIDEWALK)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset + SIDEWALK_WIDTH,
					CURB_HEIGHT, CURB_HEIGHT,
					rmgeom->m_vt[VTI_SIDEWALK],
					0.0f, 0.93f, UV_SCALE_SIDEWALK,
					ND_UP);
		offset += SIDEWALK_WIDTH;
		AddRoadStrip(pMesh, bi,
					offset,
					offset,
					CURB_HEIGHT, 0.0f,
					rmgeom->m_vt[VTI_SIDEWALK],
					0.93f, 1.0f, UV_SCALE_SIDEWALK,
					ND_RIGHT);
	}
	// create left parking lane
	if (m_iFlags & RF_PARKING)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset + PARKING_WIDTH,
					0.0f, 0.0f,
					rmgeom->m_vt[VTI_1LANE],
					0.0f, 1.0f, UV_SCALE_ROAD,
					ND_UP);
		offset += PARKING_WIDTH;
	}
	// create left margin
	if (m_iFlags & RF_MARGIN)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset + MARGIN_WIDTH,
					0.0f, 0.0f,
					rmgeom->m_vt[VTI_MARGIN],
					0.0f, 1.0f, UV_SCALE_ROAD,
					ND_UP);
		offset += MARGIN_WIDTH;
	}

	// create main road surface
	AddRoadStrip(pMesh, bi,
				-m_fWidth/2, m_fWidth/2,
				0.0f, 0.0f,
				rmgeom->m_vt[m_vti],
				0.0f, 1.0f, UV_SCALE_ROAD,
				ND_UP);
	offset = m_fWidth/2;

	// create right margin
	if (m_iFlags & RF_MARGIN)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset + MARGIN_WIDTH,
					0.0f, 0.0f,
					rmgeom->m_vt[VTI_MARGIN],
					1.0f, 0.0f, UV_SCALE_ROAD,
					ND_UP);
		offset += MARGIN_WIDTH;
	}
	// create left parking lane
	if (m_iFlags & RF_PARKING)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset + PARKING_WIDTH,
					0.0f, 0.0f,
					rmgeom->m_vt[VTI_1LANE],
					0.0f, 1.0f, UV_SCALE_ROAD,
					ND_UP);
		offset += PARKING_WIDTH;
	}

	// create right sidwalk
	if (m_iFlags & RF_SIDEWALK)
	{
		AddRoadStrip(pMesh, bi,
					offset,
					offset,
					0.0f, CURB_HEIGHT,
					rmgeom->m_vt[VTI_SIDEWALK],
					1.0f, 0.93f, UV_SCALE_SIDEWALK,
					ND_LEFT);
		AddRoadStrip(pMesh, bi,
					offset,
					offset + SIDEWALK_WIDTH,
					CURB_HEIGHT, CURB_HEIGHT,
					rmgeom->m_vt[VTI_SIDEWALK],
					0.93f, 0.0f, UV_SCALE_SIDEWALK,
					ND_UP);
		offset += SIDEWALK_WIDTH;
	}

#if 0
	if (do_roadside)
	{
		// create left roadside strip
		AddRoadStrip(pMesh, bi,
					offset, offset+ROADSIDE_WIDTH,
					(m_iFlags & RF_SIDEWALK) ? CURB_HEIGHT : 0.0f,
					ROADSIDE_DEPTH,
					MATIDX_ROADSIDE,
					0.98f, 0.02f, UV_SCALE_ROAD,
					ND_UP);
	}
#endif

	// set lane coordinates
	m_pLanes = new Lane[m_iLanes];
	for (int i = 0; i < m_iLanes; i++)
		m_pLanes[i].m_p3 = new FPoint3[GetSize()];
	for (j = 0; j < GetSize(); j++)
	{
		for (int i = 0; i < m_iLanes; i++)
		{
			float offset = -((float)(m_iLanes-1) / 2.0f) + i;
			offset *= LANE_WIDTH;
			FPoint3 offset_diff = bi.crossvector[j] * offset;
			m_pLanes[i].m_p3[j] = bi.center[j] + offset_diff;
		}
	}

	assert(total_vertices == bi.verts);
	rmgeom->AddMesh(pMesh, rmgeom->m_mi_roads);
}


FPoint3 LinkGeom::FindPointAlongRoad(float fDistance)
{
	FPoint3 v;

	float length = 0.0f;

	if (fDistance <= 0) {
		static int c = 0;
		c++;
		return m_p3[0];
	}
	// compute 2D length of this road, by adding up the 2d road segment lengths
	for (int j = 0; j < GetSize()-1; j++)
	{
		// consider length of next segment
		v.x = m_p3[j+1].x - m_p3[j].x;
		v.y = 0;
		v.z = m_p3[j+1].z - m_p3[j].z;
		length = v.Length();
		if (fDistance <= length)
		{
			float fraction = fDistance / length;
			FPoint3 p0, p1, diff;
			p0 = m_p3[j];
			v *= fraction;
			return p0 + v;
		}
		fDistance -= length;
	}
	// if we pass the end of line, just return the last point
	return m_p3[GetSize()-1];
}

//
// Return the 2D length of this road segment in world units
//
float LinkGeom::Length()
{
	FPoint3 v;
	v.y = 0;
	float length = 0.0f;

	// compute 2D length of this road, by adding up the 2d road segment lengths
	for (int j = 0; j < GetSize(); j++)
	{
		if (j > 0)
		{
			// increment length
			v.x = m_p3[j].x - m_p3[j-1].x;
			v.z = m_p3[j].z - m_p3[j-1].z;
			float l = v.Length();
			if (l < 0) {
				assert(false);
			}
			length += l;
		}
	}
	return length;
}

///////////////////////////////////////////////////////////////////

float vtRoadMap3d::s_fHeight = 1.0f;

vtRoadMap3d::vtRoadMap3d()
{
	m_pGroup = NULL;
	m_pMats = NULL;
}

vtRoadMap3d::~vtRoadMap3d()
{
	if (m_pMats)
		m_pMats->Release();
}

void vtRoadMap3d::BuildIntersections()
{
	for (NodeGeom *pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
		pN->BuildIntersection();
}


int clusters_used = 0;	// for statistical purposes

void vtRoadMap3d::AddMesh(vtMesh *pMesh, int iMatIdx)
{
	// which cluster does it belong to?
	int a, b;

	FBox3 bound;
	pMesh->GetBoundBox(bound);
	FPoint3 center = bound.Center();

	a = (int)((center.x - m_cluster_min.x) / m_cluster_range.x * ROAD_CLUSTER);
	b = (int)((center.z - m_cluster_min.z) / m_cluster_range.z * ROAD_CLUSTER);

	// safety check
	if (a < 0 || a >= ROAD_CLUSTER || b < 0 || b >= ROAD_CLUSTER)
	{
		// the geometry has somehow gotten mangled, so it's producing extents
		// outside of what they should be
		// go no further
		return;
		int c = 0;
		int d = 1 / c;	// boom
	}

	vtGeom *pGeom;
	if (m_pRoads[a][b])
	{
		pGeom = (vtGeom *)m_pRoads[a][b]->GetChild(0);
	}
	else
	{
		float fDist[2];
		fDist[0] = 0.0f;
		fDist[1] = m_fLodDistance;

		m_pRoads[a][b] = new vtLOD();
		m_pRoads[a][b]->SetRanges(fDist, 2);
		m_pGroup->AddChild(m_pRoads[a][b]);

		FPoint3 lod_center;
		lod_center.x = m_cluster_min.x + ((m_cluster_range.x / ROAD_CLUSTER) * (a + 0.5f));
		lod_center.y = m_cluster_min.y + (m_cluster_range.y / 2.0f);
		lod_center.z = m_cluster_min.z + ((m_cluster_range.z / ROAD_CLUSTER) * (b + 0.5f));
		m_pRoads[a][b]->SetCenter(lod_center);

#if 0
		vtGeom *pSphere = CreateSphereGeom(m_pMats, m_mi_red, 1000.0f, 8);
		vtMovGeom *pSphere2 = new vtMovGeom(pSphere);
		m_pGroup->AddChild(pSphere2);
		pSphere2->SetTrans(lod_center);
#endif

		pGeom = new vtGeom();
		m_pRoads[a][b]->AddChild(pGeom);
		pGeom->SetMaterials(m_pMats);

		clusters_used++;
	}
	pGeom->AddMesh(pMesh, iMatIdx);	// Add, not set
}


vtGroup *vtRoadMap3d::GenerateGeometry(bool do_texture,
									   const StringArray &paths)
{
	m_pMats = new vtMaterialArray();

	// road textures
	if (do_texture)
	{
		vtString path;
		path = FindFileOnPaths(paths, "GeoTypical/roadside_32.png");
		m_mi_roadside = m_pMats->AddTextureMaterial2(path, TEXTURE_ARGS(true));
#if 0
		// 1
		m_pMats->AddTextureMaterial2("GeoTypical/margin_32.jpg",
			TEXTURE_ARGS(false));
		// 2
		m_pMats->AddTextureMaterial2("GeoTypical/sidewalk1_v2_512.jpg",
			TEXTURE_ARGS(false));
		// 3
		m_pMats->AddTextureMaterial2("GeoTypical/1lane_64.jpg",
			TEXTURE_ARGS(false));
		// 4
		m_pMats->AddTextureMaterial2("GeoTypical/2lane1way_128.jpg",
			TEXTURE_ARGS(false));
		// 5
		m_pMats->AddTextureMaterial2("GeoTypical/2lane2way_128.jpg",
			TEXTURE_ARGS(false));
		// 6
		m_pMats->AddTextureMaterial2("GeoTypical/3lane1way_256.jpg",
			TEXTURE_ARGS(false));
		// 7
		m_pMats->AddTextureMaterial2("GeoTypical/3lane2way_256.jpg",
			TEXTURE_ARGS(false));
		// 8
		m_pMats->AddTextureMaterial2("GeoTypical/4lane1way_256.jpg",
			TEXTURE_ARGS(false));
		// 9
		m_pMats->AddTextureMaterial2("GeoTypical/4lane2way_256.jpg",
			TEXTURE_ARGS(true));
		// 10
		m_pMats->AddTextureMaterial2("GeoTypical/water.jpg",
			TEXTURE_ARGS(false));
#endif
		path = FindFileOnPaths(paths, "GeoTypical/roadset_1k.jpg");
		m_mi_roads = m_pMats->AddTextureMaterial2(path, TEXTURE_ARGS(false));

		path = FindFileOnPaths(paths, ROADTEXTURE_4WD);
		m_mi_4wd = m_pMats->AddTextureMaterial2(path, TEXTURE_ARGS(true));

		path = FindFileOnPaths(paths, ROADTEXTURE_TRAIL);
		m_mi_trail = m_pMats->AddTextureMaterial2(path, TEXTURE_ARGS(true));

		m_vt[VTI_MARGIN].m_idx = m_mi_roads;
		m_vt[VTI_MARGIN].m_rect.SetRect(960.0f/1024, 1, 992.0f/1024, 0);

		m_vt[VTI_SIDEWALK].m_idx = m_mi_roads;
		m_vt[VTI_SIDEWALK].m_rect.SetRect(512.0f/1024, 1, 640.0f/1024, 0);

		m_vt[VTI_1LANE].m_idx = m_mi_roads;
		m_vt[VTI_1LANE].m_rect.SetRect(451.0f/1024, 1, 511.0f/1024, 0);

		m_vt[VTI_2LANE1WAY].m_idx = m_mi_roads;
		m_vt[VTI_2LANE1WAY].m_rect.SetRect(4.0f/1024, 1, 124.0f/1024, 0);

		m_vt[VTI_2LANE2WAY].m_idx = m_mi_roads;
		m_vt[VTI_2LANE2WAY].m_rect.SetRect(640.0f/1024, 1, 768.0f/1024, 0);

		m_vt[VTI_3LANE1WAY].m_idx = m_mi_roads;
		m_vt[VTI_3LANE1WAY].m_rect.SetRect(2.0f/1024, 1, 190.0f/1024, 0);

		m_vt[VTI_3LANE2WAY].m_idx = m_mi_roads;
		m_vt[VTI_3LANE2WAY].m_rect.SetRect(768.0f/1024, 1, .0f/1024, 0);

		m_vt[VTI_4LANE1WAY].m_idx = m_mi_roads;
		m_vt[VTI_4LANE1WAY].m_rect.SetRect(0.0f/1024, 1, 256.0f/1024, 0);

		m_vt[VTI_4LANE2WAY].m_idx = m_mi_roads;
		m_vt[VTI_4LANE2WAY].m_rect.SetRect(256.0f/1024, 1, 512.0f/1024, 0);
	}
	else
	{
		m_mi_roadside = m_pMats->AddRGBMaterial1(RGBf(0.8f, 0.6f, 0.4f), true, false);	// 0 brown roadside
		m_mi_roads = m_pMats->AddRGBMaterial1(RGBf(0.0f, 1.0f, 0.0f), true, false);	// 1 green
		m_mi_4wd = m_pMats->AddRGBMaterial1(RGBf(0.5f, 0.5f, 0.5f), true, false);	// 2 grey
		m_mi_trail = m_pMats->AddRGBMaterial1(RGBf(1.0f, 0.3f, 1.0f), true, false);	// 3 light purple
	}
	m_mi_red = m_pMats->AddRGBMaterial(RGBf(1.0f, 0.0f, 0.0f), RGBf(0.2f, 0.0f, 0.0f),
		true, true, false, 0.4f);	// red-translucent

	m_pGroup = new vtGroup();
	m_pGroup->SetName2("Roads");

	// wrap with an array of simple LOD nodes
	int a, b;
	for (a = 0; a < ROAD_CLUSTER; a++)
		for (b = 0; b < ROAD_CLUSTER; b++)
		{
			m_pRoads[a][b] = NULL;
		}

	GatherExtents(m_cluster_min, m_cluster_max);
	m_cluster_range = m_cluster_max - m_cluster_min;

#if 0
	vtGeom *pGeom = CreateLineGridGeom(m_pMats, 0,
						   m_cluster_min, m_cluster_max, ROAD_CLUSTER);
	m_pGroup->AddChild(pGeom);
#endif

	vtMesh *pMesh;
	for (LinkGeom *pR = GetFirstLink(); pR; pR=(LinkGeom *)pR->m_pNext)
	{
		pR->GenerateGeometry(this);
//		if (pMesh) AddMesh(pMesh, 0);	// TODO: correct matidx
	}
	for (NodeGeom *pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
	{
		pMesh = pN->GenerateGeometry();
		if (pMesh) AddMesh(pMesh, 0);	// TODO: correct matidx
	}

	// return top road group, ready to be added to scene graph
	return m_pGroup;
}

//
// stoplights and stopsigns
//
void vtRoadMap3d::GenerateSigns(vtLodGrid *pLodGrid)
{
	if (!pLodGrid)
		return;

#if 0
	vtString path;
	path = FindFileOnPaths(vtTerrain::m_DataPaths, "Culture/stopsign4.dsm");
	vtNode *stopsign = vtLoadModel(path);
	path = FindFileOnPaths(vtTerrain::m_DataPaths, "Culture/stoplight8rt.dsm");
	vtNode *stoplight = vtLoadModel(path);

	if (stopsign && stoplight)
	{
		float sc = 0.01f;	// cm
		stopsign->Scale2(sc, sc, sc);
		stoplight->Scale2(sc, sc, sc);
	}
	for (NodeGeom *pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
	{
		for (int r = 0; r < pN->m_iLinks; r++)
		{
			vtGeom *shape = NULL;
			if (pN->GetIntersectType(r) == IT_STOPSIGN && stopsign)
			{
				shape = (vtGeom *)stopsign->CreateClone();
			}
			if (pN->GetIntersectType(r) == IT_LIGHT && stoplight)
			{
				shape = (vtGeom *)stoplight->CreateClone();
			}
			if (!shape) continue;

			Road *road = pN->GetRoad(r);
			FPoint3 unit = pN->GetUnitRoadVector(r);
			FPoint3 perp(unit.z, unit.y, -unit.x);
			FPoint3 offset;

			shape->RotateLocal(FPoint3(0,1,0), pN->m_fRoadAngle[r] + PID2f);

			if (pN->GetIntersectType(r) == IT_STOPSIGN)
			{
				offset = pN->m_p3 + (unit * 6.0f) + (perp * (road->m_fWidth/2.0f));
			}
			if (pN->GetIntersectType(r) == IT_LIGHT)
			{
				offset = pN->m_p3 - (unit * 6.0f) + (perp * (road->m_fWidth/2.0f));;
			}
			shape->Translate2(FPoint3(offset.x, offset.y + s_fHeight, offset.z));
			pLodGrid->AppendToGrid(shape);
		}
	}
#endif
}


void vtRoadMap3d::GatherExtents(FPoint3 &cluster_min, FPoint3 &cluster_max)
{
	// find extents of area covered by roads
	cluster_min.x = 1E10f;
	cluster_min.y = 1E10f;
	cluster_min.z = 1E10f;

	cluster_max.x = -1E10f;
	cluster_max.y = -1E10f;
	cluster_max.z = -1E10f;

	// examine the range of the cluster area
	for (NodeGeom *pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
	{
		if (pN->m_p3.x < cluster_min.x) cluster_min.x = pN->m_p3.x;
		if (pN->m_p3.y < cluster_min.y) cluster_min.y = pN->m_p3.y;
		if (pN->m_p3.z < cluster_min.z) cluster_min.z = pN->m_p3.z;

		if (pN->m_p3.x > cluster_max.x) cluster_max.x = pN->m_p3.x;
		if (pN->m_p3.y > cluster_max.y) cluster_max.y = pN->m_p3.y;
		if (pN->m_p3.z > cluster_max.z) cluster_max.z = pN->m_p3.z;
	}
	// expand slightly for safety
	FPoint3 diff = cluster_max - cluster_min;
	cluster_min -= (diff / 20.0f);
	cluster_max += (diff / 20.0f);
}


void vtRoadMap3d::DetermineSurfaceAppearance()
{
	// Pre-process some road attributes
	for (LinkGeom *pR = GetFirstLink(); pR; pR = pR->GetNext())
	{
		// set material index based on surface type, number of lanes, and direction
		bool two_way = (pR->m_iFlags & RF_FORWARD) &&
					   (pR->m_iFlags & RF_REVERSE);
		switch (pR->m_Surface)
		{
		case SURFT_NONE:
//			pR->m_vti = 3;
			pR->m_vti = 0;
			break;
		case SURFT_GRAVEL:
//			pR->m_vti = MATIDX_GRAVEL;
			pR->m_vti = 0;
			break;
		case SURFT_TRAIL:
//			pR->m_vti = MATIDX_TRAIL;
			pR->m_vti = 0;
			break;
		case SURFT_2TRACK:
		case SURFT_DIRT:
//			pR->m_vti = MATIDX_4WD;
			pR->m_vti = 0;
			break;
		case SURFT_PAVED:
			switch (pR->m_iLanes)
			{
			case 1:
				pR->m_vti = VTI_1LANE;
				break;
			case 2:
				pR->m_vti = two_way ? VTI_2LANE2WAY : VTI_2LANE1WAY;
				break;
			case 3:
				pR->m_vti = two_way ? VTI_3LANE2WAY : VTI_3LANE1WAY;
				break;
			case 4:
				pR->m_vti = two_way ? VTI_4LANE2WAY : VTI_4LANE1WAY;
				break;
			}
			break;
		}
	}
}

void vtRoadMap3d::SetLodDistance(float fDistance)
{
	m_fLodDistance = fDistance;

	if (m_pGroup)
	{
		float fDist[2];
		fDist[0] = 0.0f;
		fDist[1] = m_fLodDistance;

		int a, b;
		for (a = 0; a < ROAD_CLUSTER; a++)
			for (b = 0; b < ROAD_CLUSTER; b++)
			{
				if (m_pRoads[a][b])
					m_pRoads[a][b]->SetRanges(fDist, 2);
			}
	}
}

float vtRoadMap3d::GetLodDistance()
{
	return m_fLodDistance;
}

void vtRoadMap3d::DrapeOnTerrain(vtHeightField3d *pHeightField)
{
	FPoint3 p;
	NodeGeom *pN;

#if 0
	float height;
	for (pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
	{
		bool all_same_height = true;
		height = pN->GetRoad(0)->GetHeightAt(pN);
		for (int r = 1; r < pN->m_iLinks; r++)
		{
			if (pN->GetRoad(r)->GetHeightAt(pN) != height)
			{
				all_same_height = false;
				break;
			}
		}
		if (!all_same_height)
		{
			pNew = new NodeGeom();
			for (r = 1; r < pN->m_iLinks; r++)
			{
				LinkGeom *pR = pN->GetRoad(r);
				if (pR->GetHeightAt(pN) != height)
				{
					pN->DetachRoad(pR);
					pNew->AddRoad(pR);
				}
			}
		}
	}
#endif
	for (pN = GetFirstNode(); pN; pN = (NodeGeom *)pN->m_pNext)
	{
		pHeightField->ConvertEarthToSurfacePoint(pN->m_p, pN->m_p3);
#if 0
		if (pN->m_iLinks > 0)
		{
			height = pN->GetRoad(0)->GetHeightAt(pN);
			pN->m_p3.y += height;
		}
#endif
	}
	for (LinkGeom *pR = GetFirstLink(); pR; pR = (LinkGeom *)pR->m_pNext)
	{
		pR->m_p3 = new FPoint3[pR->GetSize()];
		for (int j = 0; j < pR->GetSize(); j++)
		{
			pHeightField->ConvertEarthToSurfacePoint(pR->GetAt(j), p);
			pR->m_p3[j].x = p.x;
			pR->m_p3[j].y = p.y;
			pR->m_p3[j].z = p.z;
		}
		// ignore width from file - imply from properties
		pR->m_fWidth = pR->m_iLanes * LANE_WIDTH;
		if (pR->m_fWidth == 0)
			pR->m_fWidth = 10.0f;
	}
}

