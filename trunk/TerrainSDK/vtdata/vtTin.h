//
// vtTin.h
//
// Copyright (c) 2002-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef VTTINH
#define VTTINH

#include "MathTypes.h"
#include "Projections.h"
#include "HeightField.h"
#include "vtString.h"

// a type useful for the Merge algorithm
typedef Array<int> Bin;


/**
 * This class represents a TIN, a 'triangulated irregular network'.  A TIN
 * consists of a set of vertices connected by triangles with no regularity.
 * However this class does expect to operate on a particular kind of
 * TIN, specifically a heightfield TIN.
 *
 * The triangles are defined by indices into the vertex array, so this is
 * an "indexed TIN".
 */
class vtTin : public vtHeightField3d
{
public:
	virtual ~vtTin() {}

	unsigned int NumVerts() const { return m_vert.GetSize(); }
	unsigned int NumTris() const { return m_tri.GetSize()/3; }

	void AddVert(const DPoint2 &p, float z);
	void AddVert(const DPoint2 &p, float z, FPoint3 &normal);
	void AddTri(int i1, int i2, int i3, int surface_type = -1);

	bool Read(const char *fname);
	bool Write(const char *fname) const;
	bool ReadDXF(const char *fname, bool progress_callback(int) = NULL);

	unsigned int AddSurfaceType(const vtString &surface_texture);

	bool ComputeExtents();
	void Offset(const DPoint2 &p);
	bool ConvertProjection(const vtProjection &proj_new);

	// Implement required vtHeightField methods
	virtual bool FindAltitudeOnEarth(const DPoint2 &p, float &fAltitude, bool bTrue = false) const;

	// Avoid implementing HeightField3d virtual methods
	bool FindAltitudeAtPoint(const FPoint3 &p3, float &fAltitude,
		bool bTrue = false, bool bIncludeCulture = false,
		FPoint3 *vNormal = NULL) const { return false; }
	bool CastRayToSurface(const FPoint3 &point, const FPoint3 &dir,
		FPoint3 &result) const { return false; }

	void CleanupClockwisdom();
	double GetTriMaxEdgeLength(int iTri) const;
	void MergeSharedVerts(bool progress_callback(int) = NULL);
	bool HasVertexNormals() { return m_vert_normal.GetSize() != 0; }

	vtProjection	m_proj;

protected:
	bool _ReadTin(FILE *fp);
	bool _ReadTinOld(FILE *fp);

	void _UpdateIndicesInInBin(int bin);
	void _CompareBins(int bin1, int bin2);

	DLine2		 m_vert;
	Array<float> m_z;
	Array<int>	 m_tri;
	FLine3		 m_vert_normal;

	// Surface Types
	Array<int>	 m_surfidx;
	vtStringArray	m_surftypes;

	// These members are used only during MergeSharedVerts
	int *m_bReplace;
	Bin *m_vertbin;
	Bin *m_tribin;
};


#endif // VTTINH
