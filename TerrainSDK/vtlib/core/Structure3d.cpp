//
// Structure3d.cpp
//
// Copyright (c) 2001-2002 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"

#include <fstream>
#include "vtdata/vtlog.h"

#include "Structure3d.h"
#include "Building3d.h"
#include "Fence3d.h"
#include "Terrain.h"

const vtString BMAT_NAME_HIGHLIGHT = "Highlight";

// Static members
vtMaterialDescriptorArray3d vtStructure3d::s_MaterialDescriptors;


// Helper: Linear distance in RGB space
float ColorDiff(const RGBi &c1, const RGBi &c2)
{
	FPoint3 diff;
	diff.x = (float) (c1.r - c2.r);
	diff.y = (float) (c1.g - c2.g);
	diff.z = (float) (c1.b - c2.b);
	return diff.Length();
}

///////////////////////////////////////////////////////////////////////

vtStructInstance3d::vtStructInstance3d() : vtStructInstance()
{
	m_pContainer = NULL;
	m_pHighlight = NULL;
	m_pModel = NULL;
}

void vtStructInstance3d::UpdateTransform(vtHeightField3d *pHeightField)
{
	m_pContainer->Identity();

	if (m_fScale != 1.0f)
		m_pContainer->Scale3(m_fScale, m_fScale, m_fScale);

	// try to work around 3DS coordinate axes difference problem
	vtString fname2 = GetValue("filename");
	int dot = fname2.ReverseFind('.');
	if (dot > 0)
	{
		vtString ext = fname2.Mid(dot+1);
		if (ext.CompareNoCase("3ds") == 0)
		{
			// Must rotate by 90 degrees for 3DS MAX -> OpenGL
			m_pContainer->Rotate2(FPoint3(1.0f, 0.0f, 0.0f), -PID2f);
		}
	}

	if (m_fRotation != 0.0f)
		m_pContainer->Rotate2(FPoint3(0,1,0), m_fRotation);

	FPoint3 surface_pos;
	pHeightField->ConvertEarthToSurfacePoint(m_p, surface_pos);
	m_pContainer->SetTrans(surface_pos);
}

/**
 * Display some bounding wires around the object to highlight it.
 */
void vtStructInstance3d::ShowBounds(bool bShow)
{
	if (bShow)
	{
		if (!m_pHighlight)
		{
			// the highlight geometry doesn't exist, so create it
			// get bounding sphere
			FSphere sphere;
			m_pModel->GetBoundSphere(sphere);

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

// implement vtStructure3d methods
bool vtStructInstance3d::CreateNode(vtTerrain *pTerr)
{
	if (!m_pContainer)
	{
		// constructing for the first time
		m_pContainer = new vtTransform();
	}

	// if previously created, destroy to re-create
	if (m_pModel)
	{
		m_pModel->Release();
		m_pModel = NULL;
	}

	const char *filename = GetValue("filename");
	if (filename)
	{
		// relative path: look on the standards data paths
		vtString fullpath = FindFileOnPaths(vtTerrain::m_DataPaths, filename);
		if (fullpath != "")
		{
			m_pModel = vtLoadModel(fullpath);
			if (m_pModel)
				SetValue("filename", fullpath);
		}
		if (!m_pModel)
			return false;
	}
	const char *itemname = GetValue("itemname");
	if (itemname)
	{
		// Use ContentManager to create the structure
		vtItem *pItem = vtTerrain::s_Content.FindItemByName(itemname);
		if (pItem)
		{
			m_pModel = vtTerrain::s_Content.CreateInstanceOfItem(pItem);
		}
		if (!m_pModel)
			return false;
	}
	m_pContainer->AddChild(m_pModel);

	const char *scale = GetValue("scale");
	if (scale)
	{
		double sc = atof(scale);
		if (sc != 0.0)
			m_fScale = sc;
	}

	UpdateTransform(pTerr->GetHeightField());
	return true;
}


///////////////////////////////////////////////////////////////////////
//
// vtStructureArray3d
//

vtBuilding *vtStructureArray3d::NewBuilding()
{
	// Make sure that subsequent operations on this building are done in with
	// the correct local coordinate system
	vtBuilding::s_Conv.Setup(m_proj.GetUnits(), DPoint2(0, 0));

	return new vtBuilding3d;
}

vtFence *vtStructureArray3d::NewFence()
{
	return new vtFence3d;
}

vtStructInstance *vtStructureArray3d::NewInstance()
{
	return new vtStructInstance3d;
}

vtStructure3d *vtStructureArray3d::GetStructure3d(int i)
{
	vtStructure *str = GetAt(i);

	// Due to the somewhat complicated structure of the multiple inheritance
	// here, we must do a double-cast: first cast down to the object's true
	// type, then back up to vtStructure3d.

	if (str->GetType() == ST_BUILDING)
		return (vtStructure3d *) (vtBuilding3d *) str;

	if (str->GetType() == ST_LINEAR)
		return (vtStructure3d *) (vtFence3d *) str;

	if (str->GetType() == ST_INSTANCE)
		return (vtStructure3d *) (vtStructInstance3d *) str;

	return NULL;
}

bool vtStructureArray3d::ConstructStructure(vtStructure3d *str)
{
	return str->CreateNode(m_pTerrain);
}

bool vtStructureArray3d::ConstructStructure(int index)
{
	vtStructure3d *str = GetStructure3d(index);
	if (str)
		return str->CreateNode(m_pTerrain);
	return NULL;
}

void vtStructureArray3d::OffsetSelectedStructures(const DPoint2 &offset)
{
	vtStructure *str;
	for (int i = 0; i < GetSize(); i++)
	{
		str = GetAt(i);
		if (!str->IsSelected())
			continue;
		if (str->GetType() == ST_BUILDING)
		{
			vtBuilding3d *bld = GetBuilding(i);
			bld->Offset(offset);
			bld->AdjustHeight(m_pTerrain->GetHeightField());

			// Should really move the building to a new cell in the LOD
			// Grid, but unless it's moving really far we don't need to
			// worry about this.
		}
		if (str->GetType() == ST_LINEAR)
		{
			vtFence3d *fen = GetFence(i);
			// TODO: implement moving of fences?
		}
		if (str->GetType() == ST_INSTANCE)
		{
			vtStructInstance3d *inst = GetInstance(i);
			inst->Offset(offset);
			inst->UpdateTransform(m_pTerrain->GetHeightField());
		}
	}
}

void vtStructureArray3d::VisualDeselectAll()
{
	for (int i = 0; i < GetSize(); i++)
	{
		vtStructure *str = (vtStructure *) GetAt(i);
		vtStructure3d *str3d = GetStructure3d(i);

		str->Select(false);
		str3d->ShowBounds(false);
	}
}

//
// Be informed of edit hightlighting
//
void vtStructureArray3d::SetEditedEdge(vtBuilding *bld, int lev, int edge)
{
	vtStructure3d *str1, *str2;

	if (m_pEditBuilding && m_pEditBuilding != bld)
	{
		m_pEditBuilding->RemoveTag("level");
		m_pEditBuilding->RemoveTag("edge");
		str1 = (vtStructure3d *) (vtBuilding3d *) m_pEditBuilding;
		ConstructStructure(str1);
	}

	vtStructureArray::SetEditedEdge(bld, lev, edge);

	if (m_pEditBuilding)
	{
		m_pEditBuilding->SetValue("level", m_iEditLevel);
		m_pEditBuilding->SetValue("edge", m_iEditEdge);
		str2 = (vtStructure3d *) (vtBuilding3d *) m_pEditBuilding;
		ConstructStructure(str2);
	}
}


void vtStructureArray3d::DestroyStructure(int i)
{
	// Need to destroy the 3d geometry for this structure
	vtStructure3d *st3d = GetStructure3d(i);
	st3d->DeleteNode();
}


/////////////////////////////////////////////////////////////////////////////
// Methods for vtMaterialDescriptorArray3d
//

vtMaterialDescriptorArray3d::vtMaterialDescriptorArray3d()
{
	m_pMaterials = NULL;
	m_pWindowWall = NULL;
}

void vtMaterialDescriptorArray3d::InitializeMaterials()
{
	if (m_pMaterials != NULL)	// already initialized
		return;

	VTLOG("Creating Building Materials\n");

	int i, j, k;
	RGBf color;
	vtMaterial *pMat;
	int count = 0;
	int divisions = 6;
	float start = .25f;
	float step = (1.0f-start)/(divisions-1);
	int iSize;

	// set up colour spread
	for (i = 0; i < divisions; i++) {
		for (j = 0; j < divisions; j++) {
			for (k = 0; k < divisions; k++) {
				m_Colors[count++].Set(start+i*step, start+j*step, start+k*step);
			}
		}
	}

	m_pMaterials = new vtMaterialArray();
	m_pMaterials->SetMaxSize(500);
/*
	// Uncomment this to make a default textures file
	// remember to create an empty file in the correct place first (for FindFileOnPaths)

	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_PLAIN),
									"",
									VT_MATERIAL_COLOURED,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_SIDING),
									"BuildingModels/siding64.jpg",
									VT_MATERIAL_COLOURABLE_TEXTURE,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_WINDOW),
									"BuildingModels/window.jpg",
									VT_MATERIAL_SELFCOLOURED_TEXTURE,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_DOOR),
									"BuildingModels/door.jpg",
									VT_MATERIAL_SELFCOLOURED_TEXTURE,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_WOOD),
									"BuildingModels/wood1_256.jpg",
									VT_MATERIAL_SELFCOLOURED_TEXTURE,
									0.6f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_CEMENT),
									"BuildingModels/cement_block1_256.jpg",
									VT_MATERIAL_SELFCOLOURED_TEXTURE,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_BRICK),
									"BuildingModels/brick1_256.jpg",
									VT_MATERIAL_BRICK,
									0.8f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_BRICK),
									"BuildingModels/brick2_256.jpg",
									VT_MATERIAL_BRICK,
									0.8f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_PAINTED_BRICK),
									"BuildingModels/brick_mono_256.jpg",
									VT_MATERIAL_COLOURABLE_TEXTURE,
									0.8f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_ROLLED_ROOFING),
									"BuildingModels/roofing1_256.jpg",
									VT_MATERIAL_COLOURABLE_TEXTURE,
									1.0f));
	s_MaterialDescriptors.Append(new vtMaterialDescriptor(BMAT_NAME_WINDOWWALL),
									"BuildingModels/window_wall128.jpg",
									VT_MATERIAL_COLOURABLE_TEXTURE,
									1.0f,
									false));
	{
		std::ofstream ops(FindFileOnPaths(vtTerrain::m_DataPaths, "Culture/materials.xml"));
		ops << s_MaterialDescriptors;
		ops.close();
	}
*/

	// First create internal materials (only needed by vtlib, not vtdata)
	m_hightlight1 = m_pMaterials->AddRGBMaterial1(RGBf(1,1,1), false, false, true);
	m_hightlight2 = m_pMaterials->AddRGBMaterial1(RGBf(1,0,0), false, false, true);

	m_pWindowWall = new vtMaterialDescriptor(BMAT_NAME_WINDOWWALL,
		"BuildingModels/window_wall128.jpg", VT_MATERIAL_COLOURABLE_TEXTURE, 1.0f);
	CreateColorableMaterial(m_pWindowWall);

	// Now load external materials (user-modifiable, user-extendable)
	if (!LoadExternalMaterials(vtTerrain::m_DataPaths))
		return;

	iSize = GetSize();

	for (j = 0; j < iSize; j++)
	{
		vtMaterialDescriptor *descriptor = GetAt(j);

		switch (descriptor->GetColorable())
		{
		case VT_MATERIAL_COLOURED:
			for (i = 0; i < COLOR_SPREAD; i++)
			{
				pMat = MakeMaterial(m_Colors[i], true);
				if (i == 0)
					descriptor->SetMaterialIndex(m_pMaterials->AppendMaterial(pMat));
				else
					m_pMaterials->AppendMaterial(pMat);
			}
			break;

		case VT_MATERIAL_SELFCOLOURED_TEXTURE:
			CreateSelfColoredMaterial(descriptor);
			break;

		case VT_MATERIAL_COLOURABLE_TEXTURE:
			CreateColorableMaterial(descriptor);
			break;
		}
	}
}

void vtMaterialDescriptorArray3d::CreateSelfColoredMaterial(vtMaterialDescriptor *descriptor)
{
	RGBf color(1.0f, 1.0f, 1.0f);
	vtMaterial *pMat = MakeMaterial(color, true);
	vtString path = FindFileOnPaths(vtTerrain::m_DataPaths, descriptor->GetSourceName());
	pMat->SetTexture2(path);
	pMat->SetClamp(false);
	descriptor->SetMaterialIndex(m_pMaterials->AppendMaterial(pMat));
}

void vtMaterialDescriptorArray3d::CreateColorableMaterial(vtMaterialDescriptor *descriptor)
{
	vtString path = FindFileOnPaths(vtTerrain::m_DataPaths, descriptor->GetSourceName());
	int divisions = 6;
	float start = .25f;
	int step = (1.0f-start)/(divisions-1);
	for (int i = 0; i < COLOR_SPREAD; i++)
	{
		vtMaterial *pMat = MakeMaterial(m_Colors[i], true);
		pMat->SetTexture2(path);
		pMat->SetClamp(false);
		if (i == 0)
			descriptor->SetMaterialIndex(m_pMaterials->AppendMaterial(pMat));
		else
			m_pMaterials->AppendMaterial(pMat);
	}
}


//
// Takes the building material and color, and tries to find the closest
// existing vtMaterial.
//
int vtMaterialDescriptorArray3d::FindMatIndex(const vtString& Material,
											  const RGBf &inputColor)
{
	// handle special case of internal materials
	if (Material == "Highlight")
	{
		// Choose the correct highlight
		if (inputColor == RGBf(1,1,1))
			return m_hightlight1;
		else
			return m_hightlight2;
	}

	vtMaterialDescriptor const *pMaterialDescriptor;
	if (Material == BMAT_NAME_WINDOWWALL)
		pMaterialDescriptor = m_pWindowWall;
	else
		pMaterialDescriptor = FindMaterialDescriptor(Material, inputColor);

	if (pMaterialDescriptor == NULL)
		return -1;
	int iIndex = pMaterialDescriptor->GetMaterialIndex();
	vtMaterialColorEnum Type = pMaterialDescriptor->GetColorable();

	if (Type == VT_MATERIAL_SELFCOLOURED_TEXTURE)
	{
/*
		if (Material == BMAT_NAME_BRICK)
		{
		RGBi b1(159, 100, 83);	// (reddish medium brown)	// "0.623 0.392 0.325" // "9F 64 53"
		RGBi b2(183, 178, 171);	// (slightly pinkish grey)	// "0.718 0.698 0.670" // "B7 B2 AB"
		}
*/
			return iIndex;
	}

	// otherwise, it is of type VT_MATERIAL_COLOURED or VT_MATERIAL_COLOURABLE_TEXTURE
	// match the closest color.
	float bestError = 1E8;
	int bestMatch = -1;
	float error;

	for (int i = 0; i < COLOR_SPREAD; i++)
	{
		error = ColorDiff(m_Colors[i], inputColor);
		if (error < bestError)
		{
			bestMatch  = iIndex + i;
			bestError = error;
		}
	}
	return bestMatch;
}

vtMaterialDescriptor *vtMaterialDescriptorArray3d::FindMaterialDescriptor(const vtString& MaterialName,
																		  const RGBf &color)
{
	float bestError = 1E8;
	int bestMatch = -1;
	float error;

	vtMaterialDescriptor *desc;
	int i, iSize = GetSize();
	for (i = 0; i < iSize; i++)
	{
		desc = GetAt(i);
		if (desc->GetName() != MaterialName)
			continue;

		// look for matching name with closest color
		const RGBi rgb = desc->GetRGB();
		error = ColorDiff(rgb, color);
		if (error < bestError)
		{
			bestMatch  = i;
			bestError = error;
		}
	}
	if (bestMatch != -1)
		return GetAt(bestMatch);
	return NULL;
}

void vtMaterialDescriptorArray3d::ReleaseMaterials()
{
	if (m_pMaterials)
	{
		m_pMaterials->Release();
		m_pMaterials = NULL;
		// do not free them - they were not dynamically allocated
		// FreeGlobalMaterials();
	}
}


/////////////////////////////////////////////////////////////////////////////
// Methods for vtStructure3d
//

void vtStructure3d::InitializeMaterialArrays()
{
	s_MaterialDescriptors.InitializeMaterials();
	SetGlobalMaterials(&s_MaterialDescriptors);
}

void vtStructure3d::ReleaseSharedMaterials()
{
	s_MaterialDescriptors.ReleaseMaterials();
}

//
// Helper to make a material
//
vtMaterial *vtMaterialDescriptorArray3d::MakeMaterial(RGBf &color, bool culling)
{
	vtMaterial *pMat = new vtMaterial();
	pMat->SetDiffuse1(color * 0.7f);
	pMat->SetAmbient1(color * 0.4f);
	pMat->SetSpecular2(0.0f);
	pMat->SetCulling(culling);
	pMat->SetLighting(true);
	return pMat;
}


