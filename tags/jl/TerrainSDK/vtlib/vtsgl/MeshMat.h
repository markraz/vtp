//
//
//

#ifndef VTSGL_MESHMATH
#define VTSGL_MESHMATH

class vtMaterial : public vtMaterialBase
{
public:
	vtMaterial();

	void SetDiffuse(float r, float g, float b, float a = 1.0f);
	RGBAf GetDiffuse();

	void SetSpecular(float r, float g, float b);
	RGBf GetSpecular();

	void SetAmbient(float r, float g, float b);
	RGBf GetAmbient();

	void SetEmission(float r, float g, float b);
	RGBf GetEmission();

	void SetCulling(bool bCulling);
	bool GetCulling();

	void SetLighting(bool bLighting);
	bool GetLighting();

	void SetTexture(class vtImage *pImage);
	vtImage *GetTexture();

	void SetTransparent(bool bOn, bool bAdd = false);
	bool GetTransparent();

	void SetClamp(bool bClamp);
	bool GetClamp();

	void SetMipMap(bool bMipMap);
	bool GetMipMap();

	void Apply();

   	sglMaterial *m_pMaterial;
	sglTexture *m_pTexture;
	vector<sglStatelet*> m_state;
};

class vtMaterialArray : public vtMaterialArrayBase
{
public:
	int vtMaterialArray::AppendMaterial(vtMaterial *pMat);
};


/////////////////////////////////////////////

class vtMesh : public vtMeshBase
{
public:
	vtMesh(GLenum PrimType, int VertType, int NumVertices = 0);

/*	int AddVertex(float x, float y, float z);
	int AddVertexN(float x, float y, float z, float nx, float ny, float nz);
	int AddVertexUV(float x, float y, float z, float u, float v);

	int AddVertex(FPoint3 &p);
	int AddVertexN(FPoint3 &p, FPoint3 &n);
	int AddVertexUV(FPoint3 &p, float u, float v); */

	void AddTri(int p0, int p1, int p2);
	void AddFan(int p0, int p1, int p2, int p3 = -1, int p4 = -1, int p5 = -1);
	void AddFan(int *idx, int iNVerts);
	void AddStrip(int iNVerts, unsigned short *pIndices);
	void AddQuadStrip(int iNVerts, int iStartIndex);
	void AddLine(int p0, int p1);

	void SetVtxPos(int, const FPoint3&);
	FPoint3 GetVtxPos(int i);

	void SetVtxNormal(int, const FPoint3&);
	FPoint3 GetVtxNormal(int i);

	void SetVtxColor(int, const RGBf&);
	RGBf GetVtxColor(int i);

	void SetVtxTexCoord(int, const FPoint2&);
	FPoint2 GetVtxTexCoord(int i);

	int GetNumPrims();
	void ReOptimize();
	void AllowOptimize(bool bAllow);

	// Implementation
	// GeoSet doesn't actually know or care about how many vertices
	// it contains - it just needs a pointer to the beginning of
	// the vertex arrays:
	Array<sglVec3f>		m_Vert;
	Array<unsigned int>	m_Index;
	Array<unsigned int>	m_PrimLen;
	Array<sglVec3f>		m_Norm;
	Array<sglVec4f>		m_Color;
	Array<sglVec2f>		m_Tex;

	sglMonoIndexedGeoSet *m_pGeoSet;
	sglMonoIndexedGeoStripSet  *m_pGeoStripSet;

	void SendPointersToSGL();
};

#endif
