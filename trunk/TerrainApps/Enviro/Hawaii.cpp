//
// Hawai`i.cpp
//
// Terrain implementation specific to the Big Island of Hawai`i.
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"
#include "vtlib/core/Building3d.h"
#include "vtlib/core/DynTerrain.h"
#include "vtlib/core/Content3d.h"
#include "vtdata/Features.h"
#include "vtdata/vtLog.h"
#include "vtdata/FilePath.h"
#include "xmlhelper/easyxml.hpp"

#include "Hawaii.h"

///////////////////////////////

void dump_mat(const FMatrix4 &mat);
void dump_mat(const FMatrix3 &mat);

IslandTerrain::IslandTerrain() : PTerrain()
{
	// Points of Interest
	// bottom left (x,y) then top right (x,y)
	AddPointOfInterest(283376, 2181205, 287025, 2182614, "Hilo Airport",
		"http://www.hawaii.gov/dot/hilo.htm");
	AddPointOfInterest(237000, 2188779, 247000, 2202455, "Mauna Kea",
		"/Html/maunakea.htm");
	AddPointOfInterest(223000, 2150000, 233000, 2159000, "Mauna Loa",
		"http://www.yahoo.com");
	AddPointOfInterest(190000, 2153953, 194500, 2157500, "Kealakekua bay",
		"/Html/kealakekua.htm");
	AddPointOfInterest(253500, 2080000, 282500, 2103500, "Lo`ihi",
		"/Html/loihi_seamount.htm");
}

IslandTerrain::~IslandTerrain()
{
	if (m_pDetailMats)
		m_pDetailMats->Release();
}

void IslandTerrain::CreateCustomCulture(bool bDoSound)
{
	// create a container for the Hawai'i-specific structures
	m_pSA = NewStructureArray();

	do_test_code();

	// import the lighthouses
	vtTransform *lighthouse1 = LoadModel("BuildingModels/mahukonalthse.dsm");
	if (lighthouse1)
	{
		// scale was one unit = 1 m
		// plant it on the ground
		PlantModelAtPoint(lighthouse1, DPoint2(197389, 2230283));

		AddNodeToStructGrid(lighthouse1);
	}

	create_building_manually();

	// TODO: replace these with a .vtst in the .ini
	create_airports();

#if 0
	// Here is an example of how to load structures from a VTST file:
	if (PointIsInTerrain(DPoint2(240749, 2194370))) // if area includes top of Mauna Kea
	{
		vtString path = FindFileOnPaths(s_DataPaths, "BuildingData/MaunaKea.vtst");
		if (path != "")
			CreateStructuresFromXML(path);
	}
#endif

	create_state_park();

	DPoint2 mauna_loa(227611, 2155222);
	if (PointIsInTerrain(mauna_loa)) // if area includes top of Mauna Loa
	{
		vtGeom *thebox = make_red_cube();
		vtGeom *thecone = make_test_cone();
		vtTransform *container = new vtTransform();
		container->SetName2("Test Shape");
		container->AddChild(thebox);
		container->AddChild(thecone);
		AddNode(container);
		PlantModelAtPoint(container, mauna_loa);
	}

	if (m_Params.m_bVehicles)
	{
		SetupVehicles();
		create_airplanes(m_Params.m_fVehicleSize, m_Params.m_fVehicleSpeed, bDoSound);
		create_ground_vehicles(m_Params.m_fVehicleSize, m_Params.m_fVehicleSpeed);
	}

	if (m_Params.m_bDetailTexture)
		set_detail_texture();
}


void IslandTerrain::create_state_park()
{
	DPoint2 park_location(234900, 2185840);
	if (!PointIsInTerrain(park_location)) // if area includes saddle
		return;

	// Here is an example of how to load a model directly and plant it
	//	on the terrain.  Because it is not part of a vtStructure, the
	//	user won't be able to select and operate on it.
	vtTransform *table = LoadModel("Culture/picnictable.3ds");
	if (table)
	{
		// model is at .1 inch per unit
		float scale = .1f * 2.54f / 100;
		scale *= 10;	// Exaggerate its size to make it easier to find
		table->Scale3(scale, scale, scale);
		// Must rotate by 90 degrees for 3DS MAX -> OpenGL
		table->Rotate2(FPoint3(1.0f, 0.0f, 0.0f), -PID2f);
		PlantModelAtPoint(table, park_location);
		AddNodeToStructGrid(table);
	}

	// An example of how to add the content definitions from a content
	//	file (vtco) to the global content manager.
	try
	{
		s_Content.ReadXML("../Data/kai.vtco");
	}
	catch (xh_io_exception &e)
	{
		string str = e.getFormattedMessage();
		VTLOG(str.c_str());
	}

	// Here is an example of how to create a structure instance which
	//	references a content item.  It is planted automatically at the
	//	desired location on the terrain.
	int index = m_pSA->GetSize();
	vtStructInstance *inst = m_pSA->AddNewInstance();
	inst->SetValue("itemname", "Riesenbuehl");
	inst->m_p = park_location;
	bool bSuccess = CreateStructure(m_pSA, index);

#if 0
	// Here is an example of how to directly create a content item and
	//	plant it manually on the terrain.
	vtItem *item = s_Content.FindItemByName("Riesenbuehl");
	if (item)
	{
		// create it
		vtGroup *group = s_Content.CreateInstanceOfItem(item);

		// plant it
		vtTransform *xform = new vtTransform();
		xform->AddChild(group);
		PlantModelAtPoint(xform, park_location);
		m_pLodGrid->AppendToGrid(xform);
	}
#endif
}

void IslandTerrain::set_detail_texture()
{
	const char *fname = "GeoTypical/grass_repeat2_512.jpg";

	vtString path = FindFileOnPaths(s_DataPaths, fname);
	vtDIB *dib = new vtDIB;

	if (!dib->ReadBMP((const char *) path))
		return;

	m_pDetailMats = new vtMaterialArray();

	vtImage *pDetailTexture = new vtImage(dib);
	int index = m_pDetailMats->AddTextureMaterial(pDetailTexture,
					 true,	// culling
					 false,	// lighting
					 true,	// transp: blend
					 false,	// additive
					 0.0f, 1.0f,	// ambient, diffuse
					 0.5f, 0.0f,	// alpha, emmisive
					 true, false,	// texgen, clamp
					 true);			// mipmap
	m_pDetailMat = m_pDetailMats->GetAt(index);

	FRECT r = m_pHeightField->m_WorldExtents;
	float width_meters = r.Width();
	m_pDynGeom->SetDetailMaterial(m_pDetailMat, 0.025f * width_meters);
}


vtGeom *IslandTerrain::make_test_cone()
{
	vtMaterialArray *looks = new vtMaterialArray();
	looks->AddRGBMaterial1(RGBf(1.0f, 0.0f, 0.0f), false);

	////////////
	int res = 40;
	vtMesh *pMesh = new vtMesh(GL_TRIANGLE_STRIP, VT_Normals, res*res);

	FPoint3 tip(0, 2000, 0);
	double cone_radius = PId/4;
	double theta1 = PId * 1.3;
	double theta2 = PId * 1.8;
	double r1 = 700;
	double r2 = 1500;

	pMesh->CreateConicalSurface(tip, cone_radius, theta1, theta2, r1, r2, res);

	vtGeom *pGeom = new vtGeom();
	pGeom->SetMaterials(looks);
	looks->Release();	// pass ownership
	pGeom->AddMesh(pMesh, 0);
	pMesh->Release();	// pass ownership

	return pGeom;
}

vtGeom *IslandTerrain::make_red_cube()
{
	vtGeom *thebox = new vtGeom();
	float ws = 100.0f;	// meters

	//code to make it a Shape
	vtMesh *mesh = new vtMesh(GL_TRIANGLE_FAN, VT_Normals, 24);
	FPoint3 size(ws, ws, ws);
	FPoint3 half = size / 2;	// Block() will double the size
	mesh->CreateBlock(half);

	vtMaterialArray *looks = new vtMaterialArray();
	looks->AddRGBMaterial1(RGBf(1.0f, 0.0f, 0.0f), true);
	thebox->SetMaterials(looks);
	looks->Release();	// pass ownership
	thebox->AddMesh(mesh, 0);
	mesh->Release();	// pass ownership

	return thebox;
}

void IslandTerrain::create_airports()
{
	vtTransform *ITO = LoadModel("Culture/ITO.dsm");
	vtTransform *KOA = LoadModel("Culture/KOA.dsm");
	vtTransform *MUE = LoadModel("Culture/MUE.dsm");

	if (ITO)
	{
		PlantModelAtPoint(ITO, DPoint2(283575, 2181163));
 		AddNode(ITO);
	}
	if (KOA)
	{
		PlantModelAtPoint(KOA, DPoint2(180290, 2184990));
 		AddNode(KOA);
	}
	if (MUE)
	{
		PlantModelAtPoint(MUE, DPoint2(219990, 2213505));
 		AddNode(MUE);
	}
}


void IslandTerrain::create_building_manually()
{
	DPoint2 bound[7], c1;
	bound[0].Set(237257, 2219644);
	bound[1] = bound[0] + DPoint2(0.0, -96.64);
	bound[2] = bound[1] + DPoint2(82.5, 0.0);
	bound[3] = bound[1] + DPoint2(178.2, 0.0);
	bound[4] = bound[3] + DPoint2(0.0, 30.48);
	bound[5] = bound[4] + DPoint2(-178.2 + 37.44f, 0.0);
	bound[6] = bound[5] + DPoint2(0.0, 96.64f - 30.48);
	c1 = bound[2] + DPoint2(0.0, 12.2);

	//  8' =  2.4385 m
	// 12' =  3.6576 m
	// 28' =  8.5344 m
	// 36' = 10.9728 m
	// 40' = 12.1920 m
	// 48' = 14.6304 m
	// test dynamic creation of a complicated building
	vtBuilding *bld = m_pSA->AddNewBuilding();

	DPoint2 c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12;
	c2 = c1 + DPoint2(10.9728, 0.0);
	c3 = c1 + DPoint2(14.6304, 0.0);
	c4 = c1 + DPoint2(14.6304, 12.1920);
	c5 = c1 + DPoint2(10.9728, 12.1920);
	c6 = c1 + DPoint2(0.0, 12.1920);
	c7 = c1 + DPoint2(3.6576, 0.0);
	c8 = c1 + DPoint2(3.6576, 3.6576);
	c9 = c1 + DPoint2(0.0, 3.6576);
	c10 = c1 + DPoint2(10.9728, 3.6576);
	c11 = c1 + DPoint2(10.9728, 8.534);
	c12 = c1 + DPoint2(3.6576, 8.534);
	DLine2 dl;
	vtLevel *pLev;
	vtEdge *pEdge;

	// basement/garage level (0)
	dl.Append(c2);
	dl.Append(c3);
	dl.Append(c4);
	dl.Append(c5);
	pLev = bld->CreateLevel(dl);
	pLev->m_fStoryHeight = 2.4385f;
	pLev->m_iStories = 1;

	pEdge = pLev->GetEdge(0);
	pLev->SetEdgeColor(RGBi(128, 128, 128));
	pEdge->m_Features.clear();
	int feat1 = pEdge->NumFeatures();
	pEdge->AddFeature(WFC_GAP);

	pEdge = pLev->GetEdge(1);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WALL, -1);
	pEdge->AddFeature(WFC_WALL, -2, 0, 0.5);
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WALL, -2, 0, 0.5);
	pEdge->AddFeature(WFC_WALL, -1);
	pEdge->m_pMaterial = GetGlobalMaterials()->FindName(BMAT_NAME_WOOD);

	pEdge = pLev->GetEdge(2);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_GAP);

	pEdge = pLev->GetEdge(3);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WALL, -1, 0, 0.5);
	pEdge->AddFeature(WFC_WALL);
	pEdge->AddFeature(WFC_WALL, -1, 0, 0.5);
	pEdge->m_pMaterial = GetGlobalMaterials()->FindName(BMAT_NAME_CEMENT);

	// main floor level (1)
	dl.Empty();
	dl.Append(c7);
	dl.Append(c3);
	dl.Append(c4);
	dl.Append(c6);
	dl.Append(c9);
	dl.Append(c8);
	pLev = bld->CreateLevel(dl);
	pLev->m_fStoryHeight = 2.4385f;
	pLev->m_iStories = 1;
	pLev->SetEdgeMaterial(BMAT_NAME_WOOD);

	pEdge = pLev->GetEdge(0);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -8);
	pEdge->AddFeature(WFC_WINDOW, -8, 0.5f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -1);
	pEdge->AddFeature(WFC_DOOR, -3);
	pEdge->AddFeature(WFC_WALL, -8);

	pEdge = pLev->GetEdge(1);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -6);
	pEdge->AddFeature(WFC_WINDOW, -2, 0.4f, 0.8f);
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WINDOW, -4, 0.7f, 1.0f);

	pEdge = pLev->GetEdge(2);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WALL, -6);
	pEdge->AddFeature(WFC_WINDOW, -4, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -2);
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -8);
	pEdge->AddFeature(WFC_WINDOW, -4, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WINDOW, -4, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -8);

	pEdge = pLev->GetEdge(3);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WALL, -8);
	pEdge->AddFeature(WFC_WINDOW, -4, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);
	pEdge->AddFeature(WFC_WALL, -4);

	pEdge = pLev->GetEdge(4);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_WALL, -4);
	pEdge->AddFeature(WFC_WINDOW, -8, 0.3f, 1.0f);

	pEdge = pLev->GetEdge(5);
	pEdge->m_Features.clear();
	pEdge->AddFeature(WFC_DOOR, -4);
	pEdge->AddFeature(WFC_WALL, -8);

	//////////////////////////////
	// first roof level (2)
	dl.Empty();
	dl.Append(c1);
	dl.Append(c3);
	dl.Append(c4);
	dl.Append(c6);
	pLev = bld->CreateLevel(dl);
	pLev->m_iStories = 1;
	pLev->SetEdgeMaterial(BMAT_NAME_PLAIN);
	pLev->SetEdgeColor(RGBi(90, 75, 75));
	bld->SetRoofType(ROOF_HIP, 14, 2);
	pLev->m_fStoryHeight = 0.9144f;	// 3 ft
	pLev->SetEaveLength(1.0f);

	//////////////////////////////
	// second roof level (3)
	dl.Empty();
	dl.Append(c8);
	dl.Append(c10);
	dl.Append(c11);
	dl.Append(c12);
	pLev = bld->CreateLevel(dl);
	pLev->m_iStories = 1;
	pLev->SetEdgeMaterial(BMAT_NAME_PLAIN);
	pLev->SetEdgeColor(RGBi(220, 220, 220));
	bld->SetRoofType(ROOF_GABLE, 33, 3);
	pLev->m_fStoryHeight = 1.6256f;	// 5 1/3 ft

	/////////////////////////////////////////
	CreateStructures(m_pSA);

	// Test XML writing
	//m_pSA->WriteXML("Ben's House.vtst");
}


///////////////////////////////////////

void IslandTerrain::do_test_code()
{
#if 0
	float x, y;
	DPoint3 p;
	int meter_height;
	char string[80];
	vtFeatures feat;
	feat.SetEntityType(SHPT_POINTZ);
	feat.AddField("Text", FTString);
	feat.GetAtProjection() = m_proj;
	vtString labels_path = FindFileOnPaths(s_DataPaths, "PointData/places.txt");
	FILE *fp = fopen(labels_path, "r");
	while( !feof(fp) )
	{
		int ret = fscanf(fp, "%f %f %d %s\n", &x, &y, &meter_height, string);
		if (!ret) break;
		p.x = x;
		p.y = y;
		p.z = meter_height;
		int rec = feat.AddPoint(p);
		feat.SetValue(rec, 0, string);
	}
	fclose(fp);
	feat.SaveToSHP("../Data/PointData/hawai`i.shp");
#endif

#if 0
	// test vtDIB JPEG
	vtDIB dib;
	bool result = dib.ReadJPEG("C:/APIs/jpeg-6b/8bit.jpg");
	if (result == true)
		dib.WriteBMP("C:/APIs/jpeg-6b/8bit.bmp");
#endif

#if 0
	// Matrix and Quaternion testing
	FMatrix4 mat;
	vtTransform *x = new vtTransform;
	x->Identity();
	x->Translate1(FPoint3(0, 2, 0));
	x->RotateLocal(FPoint3(1,0,0), 45.0f/180.0f*PIf);
	x->GetTransform1(mat);
	dump_mat(mat);

	x->Destroy();

/*	FPoint3 pos(0,2,0);
	FPoint3 forward(0, 1, -1);
	FPoint3 up(0,1,1);

	forward.Normalize();
	up.Normalize();

	mat.SetFromVectors(pos, forward, up);
	dump_mat(mat);
*/
	FMatrix3 m3;
	m3.AxisAngle(FPoint3(0,1,0), 45.0f/180.0f*PIf);
	dump_mat(m3);

	FQuat q;
//	q.SetAxisAngle(FPoint3(0,1,0), 45.0f/180.0f*PIf);
	q.SetFromMatrix(m3);
	q.GetMatrix(m3);
	dump_mat(m3);
#endif
}

void IslandTerrain::create_airplanes(float fSize, float fSpeed, bool bDoSound)
{
	// make some planes
	for (int i = 0; i < 6; i++)
		create_airplane(i, fSize, fSpeed, bDoSound);
}

void IslandTerrain::create_airplane(int i, float fSize, float fSpeed, bool bDoSound)
{
	RGBf red(1.0f, 1.0f, 0.0f);
	vtTransform *copy = CreateVehicle("747", red, fSize);
	AddNode(copy);

	// make it bigger and faster than real life
	float fSpeedExag = fSpeed;

	// attach engine
	AirportCodes code;
	code = KOA;

	PlaneEngine *pEng = new PlaneEngine(fSpeedExag, code);
	pEng->SetTarget(copy);
	pEng->SetHoop(i);
	AddEngine(pEng);

	if (bDoSound)
	{
#if 0
		//sound stuff
		vtSound3D* plane = new vtSound3D(m_strDataPath + "Vehicles/Airport-trim1.wav");
		plane->Initialize();
		plane->SetName2("Plane Sound");
		plane->SetModel(1,1,200,200);	//set limit of how far away sound can be heard
		plane->SetTarget(copy);			//set target
		plane->SetMute(true);			//mute the sound until later
		plane->Play(0, 0.0f);			//play the sound (muted)
		AddEngine(plane, pScene);
#endif
	}
}

void dump_mat(const FMatrix4 &mat)
{
	VTLOG("\nMat: %f %f %f %f\n"
		  "     %f %f %f %f\n"
		  "     %f %f %f %f\n"
		  "     %f %f %f %f\n",
		  mat(0,0), mat(1,0), mat(2,0), mat(3,0),
		  mat(0,1), mat(1,1), mat(2,1), mat(3,1),
		  mat(0,2), mat(1,2), mat(2,2), mat(3,2),
		  mat(0,3), mat(1,3), mat(2,3), mat(3,3));
}

void dump_mat(const FMatrix3 &mat)
{
	VTLOG("\nMat: %f %f %f\n"
		  "     %f %f %f\n"
		  "     %f %f %f\n",
		  mat(0,0), mat(1,0), mat(2,0),
		  mat(0,1), mat(1,1), mat(2,1),
		  mat(0,2), mat(1,2), mat(2,2));
}


