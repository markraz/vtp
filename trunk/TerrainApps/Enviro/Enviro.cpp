//
// class Enviro: Main functionality of the Enviro application
//
// Copyright (c) 2001-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"
#include "vtlib/core/Fence3d.h"
#include "vtlib/core/Globe.h"
#include "vtlib/core/SkyDome.h"
#include "vtlib/core/Building3d.h"
#include "vtdata/vtLog.h"

#include "Enviro.h"
#include "Options.h"
#include "Hawaii.h"
#include "Nevada.h"
#include "SpecificTerrain.h"

#define ORTHO_HITHER	50	// 50m above highest point on terrain

int pwdemo = 0;


///////////////////////////////////////////////////////////

Enviro *Enviro::s_pEnviro = NULL;

Enviro::Enviro() : vtTerrainScene()
{
	s_pEnviro = this;

	m_mode = MM_NONE;
	m_state = AS_Initializing;
	m_iInitStep = 0;

	m_bActiveFence = false;
	m_pCurFence = NULL;
	m_FenceParams.Defaults();

	m_bOnTerrain = false;
	m_bEarthShade = false;

	m_pGlobeContainer = NULL;
	m_bGlobeFlat = false;
	m_fFlattening = 1.0f;
	m_fFlattenDir = 0.0f;
	m_bGlobeUnfolded = false;
	m_fFolding = 0.0f;
	m_fFoldDir = 0.0f;
	m_pIcoGlobe = NULL;
	m_pSpaceAxes = NULL;
	m_pEarthLines = NULL;

	m_bTopDown = false;
	m_pTopDownCamera = NULL;
	m_pTerrainPicker = NULL;
	m_pGlobePicker = NULL;
	m_pCursorMGeom = NULL;

	m_pArc = NULL;
	m_pArcMats = NULL;
	m_fArcLength = 0.0;
	m_fDistToolHeight = 5.0f;

	m_fMessageTime = 0.0f;
	m_pHUD = NULL;

	// plants
	m_pPlantList = NULL;
	m_bPlantsLoaded = false;
	m_PlantOpt.m_iMode = 0;
	m_PlantOpt.m_iSpecies = -1;
	m_PlantOpt.m_fHeight = 100.0f;
	m_PlantOpt.m_iVariance = 20;
	m_PlantOpt.m_fSpacing = 2.0f;

	m_bDragging = false;
	m_bSelectedStruct = false;
	m_bSelectedPlant = false;

	m_pLegendGeom = NULL;
	m_bCreatedLegend = false;

	m_pMapOverview = NULL;
}

Enviro::~Enviro()
{
}

void Enviro::Startup()
{
	VTSTARTLOG("debug.txt");
	VTLOG1("\nEnviro\nBuild:");
#if VTDEBUG
	VTLOG1(" Debug");
#else
	VTLOG1(" Release");
#endif
#if UNICODE
	VTLOG1(" Unicode");
#endif
	VTLOG("\n\n");

	// set up the datum list we will use
	SetupEPSGDatums();
}

void Enviro::Shutdown()
{
	VTLOG1("Shutdown.\n");
	delete m_pPlantList;
	if (m_pArcMats)
		m_pArcMats->Release();
	if (m_pTopDownCamera)
		m_pTopDownCamera->Release();

	// Clean up the rest of the TerrainScene container
	vtGetScene()->SetRoot(NULL);
	CleanupScene();

	delete m_pIcoGlobe;
}

void Enviro::StartupArgument(int i, const char *str)
{
	VTLOG("Command line %d: %s\n", i, str);

	if (!strcmp(str, "-p"))
		pwdemo = 1;

	else if (!strcmp(str, "-fullscreen"))
		g_Options.m_bFullscreen = true;

	else if(!strncmp(str, "-terrain=", 9))
		g_Options.m_strInitTerrain = str+9;

	else if(!strncmp(str, "-location=", 10))
	{
		g_Options.m_strInitLocation = str+10;
		// trim quotes
		g_Options.m_strInitLocation.Remove('\"');
	}

	else if(!strncmp(str, "-neutral", 8))
		g_Options.m_bStartInNeutral = true;
}

void Enviro::LoadTerrainDescriptions()
{
	VTLOG("LoadTerrainDescriptions...\n");

	for (unsigned int i = 0; i < g_Options.m_DataPaths.size(); i++)
	{
		int count = 0;
		VTLOG("  On path '%s':\n", (const char *) g_Options.m_DataPaths[i]);

		vtString directory = g_Options.m_DataPaths[i] + "Terrains";
		for (dir_iter it((const char *)directory); it != dir_iter(); ++it)
		{
			if (it.is_hidden() || it.is_directory())
				continue;

			std::string name1 = it.filename();
			vtString name = name1.c_str();

			// Only look for ".xml" files which describe a terrain
			vtString ext = GetExtension(name, false);
			if (ext.CompareNoCase(".xml") != 0)
				continue;

			// Some terrain .xml files want to use a different Terrain class
			int dot = name.Find('.');
			vtString before_dot;
			if (dot == -1)
				before_dot = name;
			else
				before_dot = name.Left(dot);

			// This is where you can tell Enviro to contruct your own terrain
			//  class, for a particular config file, rather than the default
			//  vtTerrain.
			vtTerrain *pTerr;
			if (before_dot == "Hawai`i" || before_dot == "Hawai'i" ||
				before_dot == "Hawaii" || before_dot == "Honoka`a" ||
				before_dot == "Kealakekua" || before_dot == "Kamuela")
				pTerr = new IslandTerrain;
			else if (before_dot == "Nevada")
				pTerr = new NevadaTerrain;
			else if (before_dot == "TransitTerrain")
				pTerr = new TransitTerrain;
			else if (before_dot == "Romania")
				pTerr = new Romania;
			else
				pTerr = new vtTerrain;

			if (pTerr->SetParamFile(directory + "/" + name))
				AppendTerrain(pTerr);
			count++;
		}
		VTLOG("\t%d terrains.\n", count);
	}
	VTLOG(" Done.\n");
}

void Enviro::StartControlEngine()
{
	VTLOG1("StartControlEngine\n");

	m_pControlEng = new ControlEngine();
	m_pControlEng->SetName2("Control Engine");
	vtGetScene()->AddEngine(m_pControlEng);
}

void Enviro::DoControl()
{
	if (m_fMessageTime != 0.0f)
	{
		if ((vtGetTime() - m_fMessageStart) > m_fMessageTime)
		{
			SetMessage("");
			m_fMessageTime = 0.0f;
		}
	}
	if (m_state == AS_Initializing)
	{
		m_iInitStep++;

		VTLOG("AS_Initializing initstep=%d\n", m_iInitStep);

		if (m_iInitStep == 1)
		{
			SetupScene1();
			return;
		}
		if (m_iInitStep == 2)
		{
			SetupScene2();
			return;
		}
		if (m_iInitStep == 3)
		{
			SetupScene3();
			return;
		}
		if (g_Options.m_bStartInNeutral)
		{
			m_state = AS_Neutral;
		}
		else if (g_Options.m_bEarthView)
		{
			FlyToSpace();
			return;
		}
		else
		{
			if (!SwitchToTerrain(g_Options.m_strInitTerrain))
			{
				SetMessage("Terrain not found");
				m_state = AS_Error;
			}
			return;
		}
	}
	if (m_state == AS_MovingIn)
	{
		m_iInitStep++;
		SetupTerrain(m_pTargetTerrain);
	}
	if (m_state == AS_MovingOut)
	{
		m_iInitStep++;
		SetupGlobe();
	}
	if (m_state == AS_Orbit)
		DoControlOrbit();
}

bool Enviro::SwitchToTerrain(const char *name)
{
	VTLOG("SwitchToTerrain (%s)\n", name);
	vtTerrain *pTerr = FindTerrainByName(name);
	if (!pTerr)
		return false;

	if (!IsAcceptable(pTerr))
		return false;

	if (pTerr)
	{
		SwitchToTerrain(pTerr);
		return true;
	}
	else
		return false;
}

void Enviro::SwitchToTerrain(vtTerrain *pTerr)
{
	VTLOG("SwitchToTerrain %lx\n", pTerr);
	if (m_state == AS_Orbit)
	{
		// hide globe
		if (m_pGlobeContainer != NULL)
		{
			m_pGlobeContainer->SetEnabled(false);
			m_pGlobePicker->SetEnabled(false);
		}

		// remember camera position
		vtCamera *pCam = vtGetScene()->GetCamera();
		pCam->GetTransform1(m_SpaceCamLocation);

		m_pTrackball->SetEnabled(false);
	}
	if (m_state == AS_Terrain)
	{
		// remember camera position
		vtTerrain *pT = GetCurrentTerrain();
		vtCamera *pCam = vtGetScene()->GetCamera();
		FMatrix4 mat;
		pCam->GetTransform1(mat);
		pT->SetCamLocation(mat);
	}
	vtTerrain *pT = GetCurrentTerrain();
	if (pT)
		pT->SaveRoute();

	// Load the species file and check which appearances are available
	LoadSpeciesList();

	m_state = AS_MovingIn;
	m_pTargetTerrain = pTerr;
	m_iInitStep = 0;
	FreeArc();

	// Layer view needs to update
	RefreshLayerView();
}

void Enviro::SetupTerrain(vtTerrain *pTerr)
{
	// Avoid trouble with '.' and ',' in Europe
	LocaleWrap normal_numbers(LC_NUMERIC, "C");

	VTLOG("SetupTerrain step %d\n", m_iInitStep);
	if (m_iInitStep == 1)
	{
		vtString str;
		str.Format("Creating Terrain '%s'", (const char*) pTerr->GetName());
		SetMessage(str);
	}
	else if (m_iInitStep == 2)
	{
		if (pTerr->IsCreated())
			m_iInitStep = 8;	// already made, skip ahead
		else
			SetMessage("Loading Elevation");
	}
	if (m_iInitStep == 3)
	{
		pTerr->SetPlantList(m_pPlantList);
		pTerr->CreateStep0();
		if (!pTerr->CreateStep1())
		{
			m_state = AS_Error;
			SetMessage(pTerr->GetLastError());
			return;
		}
		SetMessage("Loading/Chopping/Prelighting Textures");
	}
	else if (m_iInitStep == 4)
	{
		// Tell the skydome where on the planet we are
		DPoint2 geo = pTerr->GetCenterGeoLocation();
		m_pSkyDome->SetGeoLocation(geo);

		// Set time to that of the new terrain
		m_pSkyDome->SetTime(pTerr->GetInitialTime());

		if (!pTerr->CreateStep2(GetSunLight()))
		{
			m_state = AS_Error;
			SetMessage(pTerr->GetLastError());
			return;
		}
		SetMessage("Building Terrain");
	}
	else if (m_iInitStep == 5)
	{
		if (!pTerr->CreateStep3())
		{
			m_state = AS_Error;
			SetMessage(pTerr->GetLastError());
			return;
		}
		SetMessage("Building CLOD");
	}
	else if (m_iInitStep == 6)
	{
		if (!pTerr->CreateStep4())
		{
			m_state = AS_Error;
			SetMessage(pTerr->GetLastError());
			return;
		}
		SetMessage("Creating Culture");
	}
	else if (m_iInitStep == 7)
	{
		if (!pTerr->CreateStep5())
		{
			m_state = AS_Error;
			SetMessage(pTerr->GetLastError());
			return;
		}

		// Initial default location for camera for this terrain: Try center
		//  of heightfield, just above the ground
		vtHeightField3d *pHF = pTerr->GetHeightField();
		FPoint3 middle;
		FMatrix4 mat;

		VTLOG1(" Placing the camera at the center of the terrain:\n");
		VTLOG(" World extents: LRTB %f %f %f %f\n",
			pHF->m_WorldExtents.left,
			pHF->m_WorldExtents.right,
			pHF->m_WorldExtents.top,
			pHF->m_WorldExtents.bottom);
		pHF->GetCenter(middle);
		VTLOG(" Center: %f %f %f\n", middle.x, middle.y, middle.z);
		pHF->FindAltitudeAtPoint(middle, middle.y);
		VTLOG(" Altitude at that point: %f\n", middle.y);
		float minheight = pTerr->GetParams().GetValueFloat(STR_MINHEIGHT);
		middle.y += minheight;
		VTLOG(" plus minimum height (%f) is %f\n", minheight, middle.y);

		mat.Identity();
		mat.SetTrans(middle);
		pTerr->SetCamLocation(mat);
	}
	else if (m_iInitStep == 8)
	{
		SetMessage("Setting hither/yon");
		vtCamera *pCam = vtGetScene()->GetCamera();
		pCam->SetHither(pTerr->GetParams().GetValueFloat(STR_HITHER));
		pCam->SetYon(500000.0f);
	}
	else if (m_iInitStep == 9)
	{
		VTLOG("Setting Camera Location\n");
		m_pNormalCamera->SetTransform1(pTerr->GetCamLocation());
		SetMessage("Switching to Terrain");
	}
	else if (m_iInitStep == 10)
	{
		// make first terrain active
		SetTerrain(pTerr);

		// ensure that sunlight is active
		GetSunLight()->SetEnabled(true);

		m_pCurRoute=pTerr->GetLastRoute();	// Error checking needed here.

		m_pTerrainPicker->SetEnabled(true);
		SetMode(MM_NAVIGATE);
	}
	else if (m_iInitStep == 11)
	{
		m_state = AS_Terrain;
		vtString str;
		str.Format("Welcome to %s", (const char *)pTerr->GetName());
		SetMessage(str, 5.0f);

		// Layer view needs to update
		RefreshLayerView();
	}
}

void Enviro::FormatCoordString(vtString &str, const DPoint3 &coord, LinearUnits units, bool seconds)
{
	DPoint3 pos = coord;
	if (units == LU_DEGREES)
	{
		int deg1 = (int) pos.x;
		pos.x -= (deg1);
		int min1 = (int) (pos.x * 60);
		pos.x -= (min1 / 60.0);
		int sec1 = (int) (pos.x * 3600);
		if (deg1 < 0) deg1 = -deg1;
		if (min1 < 0) min1 = -min1;
		if (sec1 < 0) sec1 = -sec1;
		char ew = coord.x > 0.0f ? 'E' : 'W';

		int deg2 = (int) pos.y;
		pos.y -= (deg2);
		int min2 = (int) (pos.y * 60);
		pos.y -= (min2 / 60.0);
		int sec2 = (int) (pos.y * 3600);
		if (deg2 < 0) deg2 = -deg2;
		if (min2 < 0) min2 = -min2;
		if (sec2 < 0) sec2 = -sec2;
		char ns = coord.y > 0.0f ? 'N' : 'S';

		if (seconds)
			str.Format("%3d:%02d:%02d %c, %3d:%02d:%02d %c", deg1, min1, sec1, ew, deg2, min2, sec2, ns);
		else
			str.Format("%3d:%02d %c, %3d:%02d %c", deg1, min1, ew, deg2, min2, ns);
	}
	else
	{
		str.Format("%7.1d, %7.1d", (int) coord.x, (int) coord.y);
	}
}


//
// Check the terrain under the 3D cursor (for Terrain View).
//
void Enviro::DoCursorOnTerrain()
{
	m_bOnTerrain = false;
	DPoint3 earthpos;
	vtString str;

	if (m_pTerrainPicker != NULL)
		m_bOnTerrain = m_pTerrainPicker->GetCurrentEarthPos(earthpos);
	if (m_bOnTerrain)
	{
		m_EarthPos = earthpos;

		// Attempt to scale the 3d cursor, for ease of use.
		// Rather than keeping it the same size in world space (it would
		// be too small in the distance) or the same size in screen space
		// (would look confusing without the spatial distance cue) we
		// compromise and scale it based on the square root of distance.
		FPoint3 gpos;
		if (m_pTerrainPicker->GetCurrentPoint(gpos))
		{
			FPoint3 campos = vtGetScene()->GetCamera()->GetTrans();
			float distance = (gpos - campos).Length();
			float sc = (float) sqrt(distance) / 1.0f;
			FPoint3 pos = m_pCursorMGeom->GetTrans();
			m_pCursorMGeom->Identity();
			m_pCursorMGeom->Scale3(sc, sc, sc);
			m_pCursorMGeom->SetTrans(pos);
		}

		// Inform GUI, in case it cares.
		EarthPosUpdated();
	}
}


void Enviro::SetupScene1()
{
	VTLOG1("SetupScene1\n");

	// Set some global properties
	SetDataPath(g_Options.m_DataPaths);
	m_fCatenaryFactor = g_Options.m_fCatenaryFactor;
	vtMaterial::s_bTextureCompression = g_Options.m_bTextureCompression;
	vtNode::s_bDisableMipmaps = g_Options.m_bDisableModelMipmaps;

	vtScene *pScene = vtGetScene();
	vtCamera *pCamera = pScene->GetCamera();
	if (pCamera) pCamera->SetName2("Standard Camera");

	m_pRoot = BeginTerrainScene();
	pScene->SetRoot(m_pRoot);
}

void Enviro::SetupScene2()
{
	VTLOG1("SetupScene2\n");

	m_pNavEngines = new vtEngine;
	m_pNavEngines->SetName2("Navigation Engines");
	vtGetScene()->GetRootEngine()->AddChild(m_pNavEngines);

	// Make navigation engines
	m_pOrthoFlyer = new vtOrthoFlyer(1.0f);
	m_pOrthoFlyer->SetName2("Orthographic View Flyer");
	m_pOrthoFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pOrthoFlyer);

	m_pQuakeFlyer = new QuakeFlyer(1.0f);
	m_pQuakeFlyer->SetName2("Quake-Style Flyer");
	m_pQuakeFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pQuakeFlyer);

	m_pVFlyer = new VFlyer(1.0f);
	m_pVFlyer->SetName2("Velocity-Gravity Flyer");
	m_pVFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pVFlyer);

	m_pTFlyer = new vtTerrainFlyer(1.0f);
	m_pTFlyer->SetName2("Terrain-following Flyer");
	m_pTFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pTFlyer);

	m_pGFlyer = new GrabFlyer(1.0f);
	m_pGFlyer->SetName2("Grab-Pivot Flyer");
	m_pGFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pGFlyer);

	m_pFlatFlyer = new FlatFlyer();
	m_pFlatFlyer->SetName2("Flat Flyer");
	m_pFlatFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pFlatFlyer);

	m_pPanoFlyer = new vtPanoFlyer(1.0f);
	m_pPanoFlyer->SetName2("Panoramic Flyer");
	m_pPanoFlyer->SetEnabled(false);
	m_pNavEngines->AddChild(m_pPanoFlyer);

	m_nav = NT_Normal;

	// create picker object and picker engine
	vtGeom *pCursor = Create3DCursor(1.0, g_Options.m_fCursorThickness);
	m_pCursorMGeom = new vtMovGeom(pCursor);
	m_pCursorMGeom->SetName2("Cursor");

	GetTop()->AddChild(m_pCursorMGeom);
	m_pTerrainPicker = new TerrainPicker();
	m_pTerrainPicker->SetName2("TerrainPicker");
	vtGetScene()->AddEngine(m_pTerrainPicker);

	m_pTerrainPicker->SetTarget(m_pCursorMGeom);
	m_pTerrainPicker->SetEnabled(false); // turn off at startup

	// Connect to the GrabFlyer
	m_pGFlyer->SetTerrainPicker(m_pTerrainPicker);

	VTLOG("Setting up Cameras\n");
	m_pNormalCamera = vtGetScene()->GetCamera();

	// Create second camera (for Top-Down view)
	if (m_pTopDownCamera == NULL)
	{
		VTLOG("Creating Top-Down Camera\n");
		m_pTopDownCamera = new vtCamera;
		m_pTopDownCamera->SetOrtho(true);
		m_pTopDownCamera->SetName2("Top-Down Camera");
		m_pOrthoFlyer->SetTarget(m_pTopDownCamera);
	}

	m_pQuakeFlyer->SetTarget(m_pNormalCamera);
	m_pVFlyer->SetTarget(m_pNormalCamera);
	m_pTFlyer->SetTarget(m_pNormalCamera);
	m_pGFlyer->SetTarget(m_pNormalCamera);
	m_pFlatFlyer->SetTarget(m_pNormalCamera);
	m_pPanoFlyer->SetTarget(m_pNormalCamera);

	// An engine to keep the camera above the terrain, comes after the other
	//  engines which could move the camera.
	m_pHeightEngine = new vtHeightConstrain(1.0f);
	m_pHeightEngine->SetName2("Height Constrain Engine");
	m_pHeightEngine->SetTarget(m_pNormalCamera);
	vtGetScene()->GetRootEngine()->AddChild(m_pHeightEngine);

	// This HUD group will contain geometry such as the legend
	m_pHUD = new vtHUD;
	m_pRoot->AddChild(m_pHUD);
}

//
// Load the species file and check which appearances are available
//
void Enviro::LoadSpeciesList()
{
	if (m_bPlantsLoaded)
		return;

	VTLOG1("LoadSpeciesList\n");

	// First look for species.xml with terrain name prepended, otherwise fall
	//  back on just "species.xml"
	vtString species_fname = "PlantData/" + g_Options.m_strInitTerrain + "-species.xml";
	vtString species_path = FindFileOnPaths(g_Options.m_DataPaths, species_fname);
	if (species_path == "")
		species_path = FindFileOnPaths(g_Options.m_DataPaths, "PlantData/species.xml");

	if (species_path == "")
	{
		VTLOG1(" not found.\n");
		return;
	}

	vtSpeciesList pl;
	vtString errmsg;
	if (pl.ReadXML(species_path, &errmsg))
	{
		VTLOG(" Using species file: '%s'\n", (const char *) species_path);
		m_pPlantList = new vtSpeciesList3d;
		*m_pPlantList = pl;

		// global options
		vtPlantAppearance3d::s_fPlantScale = g_Options.m_fPlantScale;
		vtPlantAppearance3d::s_bPlantShadows = g_Options.m_bShadows;

		// Don't load all the plant appearances now, just check which are available
//			m_pPlantList->CreatePlantSurfaces();
		int available = m_pPlantList->CheckAvailability();
		VTLOG(" %d plant appearances available.\n", available);

		m_bPlantsLoaded = true;
	}
	else
	{
		VTLOG1("Error loading species: ");
		VTLOG1(errmsg);
		VTLOG1("\n");
	}
}

void Enviro::SetCurrentNavigator(vtTerrainFlyer *pE)
{
	if (m_pCurrentFlyer != NULL)
	{
		const char *name = m_pCurrentFlyer->GetName2();
		VTLOG("Disabling '%s'\n", name);

		m_pCurrentFlyer->SetEnabled(false);
	}
	m_pCurrentFlyer = pE;
	if (m_pCurrentFlyer == NULL)
		VTLOG1("No navigator now enabled.\n");
	else
	{
		const char *name = m_pCurrentFlyer->GetName2();
		VTLOG(" Enabling '%s'\n", name);

		m_pCurrentFlyer->SetEnabled(true);
	}
}

void Enviro::EnableFlyerEngine(bool bEnable)
{
	if (bEnable && !m_bTopDown)
	{
		// enable normal navigation
		if (m_nav == NT_Normal)
			SetCurrentNavigator(m_pTFlyer);
		if (m_nav == NT_Velo)
			SetCurrentNavigator(m_pVFlyer);
		if (m_nav == NT_Grab)
			SetCurrentNavigator(m_pGFlyer);
//		if (m_nav == NT_Quake)
//			SetCurrentNavigator(m_pQuakeFlyer);
		if (m_nav == NT_Pano || m_nav == NT_Dummy)
			SetCurrentNavigator(m_pPanoFlyer);
	}
	else
		SetCurrentNavigator(NULL);

	if (m_bTopDown)
	{
		VTLOG("Enable OrthoFlyer: %d\n", bEnable);
		m_pOrthoFlyer->SetEnabled(bEnable);
	}
}

void Enviro::SetNavType(NavType nav)
{
	if (m_mode == MM_NAVIGATE)
		EnableFlyerEngine(false);
	m_nav = nav;
	if (m_mode == MM_NAVIGATE)
		EnableFlyerEngine(true);
}

void Enviro::SetMaintain(bool bOn)
{
	m_pHeightEngine->SetMaintain(bOn);
	m_pHeightEngine->SetMaintainHeight(0);
}

bool Enviro::GetMaintain()
{
	if (m_pHeightEngine == NULL)
		return false;
	return m_pHeightEngine->GetMaintain();
}


void Enviro::SetTerrain(vtTerrain *pTerrain)
{
	VTLOG("Enviro::SetTerrain '%s'\n",
		pTerrain ? (const char *) pTerrain->GetName() : "none");

	if (m_pCurrentTerrain)
	{
		vtGroup *pOverlay = m_pCurrentTerrain->GetOverlay();
		if (pOverlay)
			m_pHUD->RemoveChild(pOverlay);
	}

	// Inform the container that this new terrain is current
	SetCurrentTerrain(pTerrain);

	// safety check
	if (!pTerrain)
	{
		ShowMapOverview(false);
		return;
	}
	vtHeightField3d *pHF = pTerrain->GetHeightField();
	if (!pHF)
		return;

	// Inform the UI that this new terrain is current
	TParams &param = pTerrain->GetParams();
	SetNavType((enum NavType) param.GetValueInt(STR_NAVSTYLE));

	EnableFlyerEngine(true);

	// Inform the terrain's location saver of the camera
	pTerrain->GetLocSaver()->SetTransform(m_pNormalCamera);

	// inform the navigation engine of the new terrain
	float speed = param.GetValueFloat(STR_NAVSPEED);
	if (m_pCurrentFlyer != NULL)
	{
		m_pCurrentFlyer->SetTarget(m_pNormalCamera);
		m_pCurrentFlyer->SetEnabled(true);
		m_pCurrentFlyer->SetExag(param.GetValueBool(STR_ACCEL));
	}
	m_pTFlyer->SetSpeed(speed);
	m_pVFlyer->SetSpeed(speed);
	m_pPanoFlyer->SetSpeed(speed);
	m_pOrthoFlyer->SetSpeed(speed);

	// TODO: a more elegant way of keeping all nav engines current
	m_pQuakeFlyer->SetHeightField(pHF);
	m_pVFlyer->SetHeightField(pHF);
	m_pTFlyer->SetHeightField(pHF);
	m_pGFlyer->SetHeightField(pHF);
	m_pPanoFlyer->SetHeightField(pHF);
	// also the height constraint engine
	m_pHeightEngine->SetHeightField(pHF);
	m_pHeightEngine->SetMinGroundOffset(param.GetValueFloat(STR_MINHEIGHT));

	bool bAllowRoll = param.GetValueBool(STR_ALLOW_ROLL);
	m_pTFlyer->SetDOF(vtFlyer::DOF_ROLL, bAllowRoll);

	// Set the top-down viewpoint to a point over the center of the new
	//  terrain, with near and far planes derived from the height extents.
	m_pTopDownCamera->Identity();
	FPoint3 middle;
	pHF->GetCenter(middle);		// Gets XZ center

	float fMin, fMax;
	pHF->GetHeightExtents(fMin, fMax);
	fMax *= param.GetValueFloat(STR_VERTICALEXAG);
	fMax += 1;	// beware flat terrain: safety buffer of 1 meter
	middle.y = fMax + ORTHO_HITHER;		// highest value + hither
	m_pTopDownCamera->SetTrans(middle);

	// point it straight down
	m_pTopDownCamera->RotateLocal(TRANS_XAxis, -PID2f);
	m_pTopDownCamera->SetHither(ORTHO_HITHER);
	m_pTopDownCamera->SetYon(fMax - fMin + ORTHO_HITHER + ORTHO_HITHER);

	// pick an arbitrary amount of detail to show initially:
	//  1/4 the terrain width, with proportional speed
	m_pTopDownCamera->SetWidth(middle.x / 2);
	m_pOrthoFlyer->SetSpeed(middle.x / 5);

	if (m_pTerrainPicker != NULL)
		m_pTerrainPicker->SetHeightField(pHF);

	m_fDistToolHeight = param.GetValueFloat(STR_DIST_TOOL_HEIGHT);

	vtGroup *pOverlay = pTerrain->GetOverlay();
	if (pOverlay)
		m_pHUD->AddChild(pOverlay);

	// Inform the GUI that the terrain has changed
	SetTerrainToGUI(pTerrain);

	// Inform the map overview
	if (m_pMapOverview)
		m_pMapOverview->SetTerrain(pTerrain);
}


//
// Display a message as a text sprite in the middle of the window.
//
// The fTime argument lets you specify how long the message should
// appear, in seconds.
//
void Enviro::SetMessage(const vtString &msg, float fTime)
{
	VTLOG("  SetMessage: '%s'\n", (const char *) msg);

#if 0
	if (m_pMessageSprite == NULL)
	{
		m_pMessageSprite = new vtSprite;
		m_pMessageSprite->SetName2("MessageSprite");
		m_pRoot->AddChild(m_pMessageSprite);
	}
	if (msg == "")
		m_pMessageSprite->SetEnabled(false);
	else
	{
		m_pMessageSprite->SetEnabled(true);
		m_pMessageSprite->SetText(msg);
		int len = msg.GetLength();
		m_pMessageSprite->SetWindowRect(0.5f - (len * 0.01f), 0.45f,
										0.5f + (len * 0.01f), 0.55f);
	}
#endif
	if (msg != "" && fTime != 0.0f)
	{
		m_fMessageStart = vtGetTime();
		m_fMessageTime = fTime;
	}
	m_strMessage = msg;
}

void Enviro::SetFlightSpeed(float speed)
{
	if (m_bTopDown && m_pOrthoFlyer != NULL)
		m_pOrthoFlyer->SetSpeed(speed);
	else if (m_pCurrentFlyer != NULL)
		m_pCurrentFlyer->SetSpeed(speed);
}

float Enviro::GetFlightSpeed()
{
	if (m_bTopDown && m_pOrthoFlyer != NULL)
		return m_pOrthoFlyer->GetSpeed();
	else if (m_pCurrentFlyer != NULL)
		return m_pCurrentFlyer->GetSpeed();
	else
		return 0.0f;
}

void Enviro::SetFlightAccel(bool bAccel)
{
	if (m_pCurrentFlyer)
		m_pCurrentFlyer->SetExag(bAccel);
}

bool Enviro::GetFlightAccel()
{
	if (m_pCurrentFlyer)
		return m_pCurrentFlyer->GetExag();
	else
		return false;
}

void Enviro::SetMode(MouseMode mode)
{
	VTLOG("SetMode %d\n", mode);

	if (m_pCursorMGeom)
	{
		switch (mode)
		{
		case MM_NAVIGATE:
			m_pCursorMGeom->SetEnabled(false);
			EnableFlyerEngine(true);
			break;
		case MM_SELECT:
			m_pCursorMGeom->SetEnabled(!g_Options.m_bDirectPicking);
			EnableFlyerEngine(false);
			break;
		case MM_FENCES:
		case MM_BUILDINGS:
		case MM_ROUTES:
		case MM_PLANTS:
		case MM_INSTANCES:
		case MM_MOVE:
		case MM_MEASURE:
			m_pCursorMGeom->SetEnabled(true);
			EnableFlyerEngine(false);
			break;
		}
	}
	m_bActiveFence = false;
	m_mode = mode;
}

void Enviro::SetTopDown(bool bTopDown)
{
	static bool bWas;

	m_bTopDown = bTopDown;

	if (bTopDown)
	{
		vtGetScene()->SetCamera(m_pTopDownCamera);
		bWas = m_pSkyDome->GetEnabled();
		m_pSkyDome->SetEnabled(false);
	}
	else
	{
		vtGetScene()->SetCamera(m_pNormalCamera);
		m_pSkyDome->SetEnabled(bWas);
		m_pOrthoFlyer->SetEnabled(false);
	}

	// set mode again, to put everything in the right state
	SetMode(m_mode);

	// inform the UI that we have a switched cameras
	CameraChanged();
}

void Enviro::DumpCameraInfo()
{
	vtCamera *cam = m_pNormalCamera;
	FPoint3 pos = cam->GetTrans();
	FPoint3 dir = cam->GetDirection();
	VTLOG("Camera: pos %f %f %f, dir %f %f %f\n",
		pos.x, pos.y, pos.z, dir.x, dir.y, dir.z);
}

void Enviro::SetSpeed(float x)
{
	if (m_state == AS_Orbit && m_pGlobeTime)
		m_pGlobeTime->SetSpeed(x);
	else if (m_state == AS_Terrain)
		vtGetTS()->GetTimeEngine()->SetSpeed(x);
}

float Enviro::GetSpeed()
{
	if (m_state == AS_Orbit && m_pGlobeTime)
		return m_pGlobeTime->GetSpeed();
	else if (m_state == AS_Terrain)
		return vtGetTS()->GetTimeEngine()->GetSpeed();
	return 0;
}

void Enviro::OnMouse(vtMouseEvent &event)
{
	// check for what is under the 3D cursor
	if (m_state == AS_Orbit)
		DoCursorOnEarth();
	else if (m_state == AS_Terrain &&
		(m_pCursorMGeom->GetEnabled() || g_Options.m_bDirectPicking))
		DoCursorOnTerrain();

	// give the child classes first chance to take this event
	bool bCancel = OnMouseEvent(event);
	if (bCancel)
		return;

	if (event.type == VT_DOWN)
	{
		if (event.button == VT_LEFT)
		{
			if (m_state == AS_Terrain)
				OnMouseLeftDownTerrain(event);
			else if (m_state == AS_Orbit)
				OnMouseLeftDownOrbit(event);
		}
		else if (event.button == VT_RIGHT)
			OnMouseRightDown(event);
	}
	if (event.type == VT_MOVE)
	{
		OnMouseMove(event);
	}
	if (event.type == VT_UP)
	{
		if (event.button == VT_LEFT)
			OnMouseLeftUp(event);
		if (event.button == VT_RIGHT)
			OnMouseRightUp(event);
	}
}

void Enviro::OnMouseLeftDownTerrain(vtMouseEvent &event)
{
	if (m_mode != MM_SELECT && !m_bOnTerrain)
		return;

	vtTerrain *pTerr = GetCurrentTerrain();

	// Build fences on click
	if (m_mode == MM_FENCES)
	{
		if (!m_bActiveFence)
		{
			start_new_fence();
			m_bActiveFence = true;
		}
		pTerr->AddFencepoint(m_pCurFence, DPoint2(m_EarthPos.x, m_EarthPos.y));
	}
	if (m_mode == MM_BUILDINGS)
		OnMouseLeftDownBuildings();

	if (m_mode == MM_ROUTES)
	{
		if (!m_bActiveRoute)
		{
			start_new_route();
			m_bActiveRoute = true;
		}
		pTerr->add_routepoint_earth(m_pCurRoute,
			DPoint2(m_EarthPos.x, m_EarthPos.y), m_sStructType);
	}
	if (m_mode == MM_PLANTS)
	{
		// try planting a tree there
		if (pTerr->GetProjection().IsGeographic())
			VTLOG("Create a plant at %.8lf,%.8lf:", m_EarthPos.x, m_EarthPos.y);
		else
			VTLOG("Create a plant at %.2lf,%.2lf:", m_EarthPos.x, m_EarthPos.y);
		bool success = PlantATree(DPoint2(m_EarthPos.x, m_EarthPos.y));
		VTLOG(" %s.\n", success ? "yes" : "no");
	}
	if (m_mode == MM_INSTANCES)
		PlantInstance();

	if (m_mode == MM_SELECT)
		OnMouseLeftDownTerrainSelect(event);

	if (m_mode == MM_MEASURE)
	{
		m_EarthPosDown = m_EarthPos;
		m_bDragging = true;
		DPoint2 g1(m_EarthPosDown.x, m_EarthPosDown.y);
		SetTerrainMeasure(g1, g1);
		ShowDistance(g1, g1, 0, 0);
	}
}

void Enviro::OnMouseLeftDownBuildings()
{
	SetupArcMesh();
	DPoint2 g1(m_EarthPos.x, m_EarthPos.y);

	// Create a marker pole for this corner of the new building
	int matidx = 2;
	float fHeight = 10.0f;
	float fRadius = 0.2f;
	vtGeom *geom = CreateCylinderGeom(m_pArcMats, matidx, VT_Normals, fHeight,
		fRadius, 10, true, false, false, 1);
	vtTransform *trans = new vtTransform;
	trans->AddChild(geom);
	m_Markers.push_back(trans);
	vtTerrain *pTerr = GetCurrentTerrain();
	pTerr->PlantModelAtPoint(trans, g1);
	pTerr->AddNode(trans);

	if (m_bLineDrawing)
	{
		// continue existing line
		m_NewLine.Append(g1);

		vtTerrain *pTerr = GetCurrentTerrain();
		vtMeshFactory mf(m_pArc, vtMesh::LINE_STRIP, 0, 30000, 1);
		pTerr->AddSurfaceLineToMesh(&mf, m_NewLine, m_fDistToolHeight, true);
	}
	else
	{
		// start new line
		m_bLineDrawing = true;
		m_NewLine.Empty();
		m_NewLine.Append(g1);
	}
}

void Enviro::OnMouseSelectRayPick(vtMouseEvent &event)
{
	VTLOG("Click, raypick at %d %d, ", event.pos.x, event.pos.y);

	vtTerrain *pTerr = GetCurrentTerrain();
	vtStructureArray3d *pActiveStructures = pTerr->GetStructures();

	if (!(event.flags & VT_CONTROL) && (pActiveStructures != NULL))
	{
		pActiveStructures->VisualDeselectAll();
		m_bSelectedStruct = false;
	}

	vtPlantInstanceArray3d &Plants = pTerr->GetPlantInstances();
	Plants.VisualDeselectAll();
	m_bSelectedPlant = false;

	vtRouteMap &Routes = pTerr->GetRouteMap();
	m_bSelectedUtil = false;

	// Get ray intersection with near and far planes
	FPoint3 Near, Dir;
	vtGetScene()->CameraRay(event.pos, Near, Dir);

	// Dir is unit-length direction vector, so scale it up to the
	//  distance we want to test.
	Dir *= 10000.0f;	// 10km should be enough for visible objects

	vtHitList HitList;
	int iNumHits = vtIntersect(pTerr->GetTopGroup(), Near, Near+Dir, HitList);
	if (iNumHits == 0)
	{
		VTLOG("no hits\n");
		return;
	}
	else
		VTLOG("%d hits\n", iNumHits);

	// Check for structures
	int iSet, iOffset;
	if (pTerr->GetStructureSet().FindStructureFromNode(HitList.front().node, iSet, iOffset))
	{
		VTLOG("  Found structure ");
		vtStructureArray3d *pSelectedStructures = pTerr->GetStructureSet().GetAt(iSet);
		vtBuilding3d *pBuilding = pSelectedStructures->GetBuilding(iOffset);
		vtStructInstance3d *pInstance = pSelectedStructures->GetInstance(iOffset);
		vtFence3d *pFence = pSelectedStructures->GetFence(iOffset);

		if (NULL != pBuilding)
		{
			// Found a procedural building
			VTLOG("(building)\n");
			pBuilding->ToggleSelect();
			if (pBuilding->IsSelected())
			{
				pBuilding->ShowBounds(true);
				m_bDragging = true;
			}
			else
				pBuilding->ShowBounds(false);
		}
		else if (NULL != pInstance)
		{
			// Found a structure instance
			VTLOG("(instance)\n");
			pInstance->ToggleSelect();
			if (pInstance->IsSelected())
			{
				pInstance->ShowBounds(true);
				if ((event.flags & VT_SHIFT) != 0)
				{
					m_StartRotation = pInstance->GetRotation();
					m_bRotating = true;
				}
				else
					m_bDragging = true;
			}
			else
				pInstance->ShowBounds(false);
		}
		else if (NULL != pFence)
		{
			// Found a linear structure
			VTLOG("(fence)\n");
			pFence->ToggleSelect();
			if (pFence->IsSelected())
				pFence->ShowBounds(true);
			else
				pFence->ShowBounds(false);
		}
		else
			VTLOG("(unknown)\n");
		if (pTerr->GetStructureIndex() != iSet)
		{
			// Switching to a different structure set
			pActiveStructures->VisualDeselectAll();
			pTerr->SetStructureIndex(iSet);
			ShowLayerView();
			UpdateLayerView();
		}
		// This is inefficient it would be better to maintain a live count if possible
		if (pTerr->GetStructures()->NumSelected())
			m_bSelectedStruct = true;
		else
			m_bSelectedStruct = false;
	}
	// Check for plants
	else if (Plants.FindPlantFromNode(HitList.front().node, iOffset))
	{
		VTLOG("  Found plant\n");
		Plants.VisualSelect(iOffset);
		m_bDragging = true;
		m_bSelectedPlant = true;
	}
	// Check for routes
	else if (Routes.FindRouteFromNode(HitList.front().node, iOffset))
	{
		VTLOG("  Found route\n");
		m_bDragging = true;
		m_bSelectedUtil = true;
		m_pSelRoute = Routes.GetAt(iOffset);
	}
	else
		VTLOG("  Unable to identify node\n");

	if (m_bDragging)
		VTLOG("Now dragging.\n");
	if (m_bRotating)
		VTLOG("Now rotating.\n");
}

void Enviro::OnMouseSelectCursorPick(vtMouseEvent &event)
{
	VTLOG("Click, cursor pick, ");

	// See if camera ray intersects a structure?  NO, it's simpler and
	//  easier for the user to just test whether the ground cursor is
	//  near a structure's origin.
	DPoint2 gpos(m_EarthPos.x, m_EarthPos.y);

	double dist1, dist2, dist3;
	vtTerrain *pTerr = GetCurrentTerrain();
	vtStructureArray3d *structures = pTerr->GetStructures();
	if (!(event.flags & VT_CONTROL) && structures != NULL)
		structures->VisualDeselectAll();

	// SelectionCutoff is in meters, but the picking functions work in
	//  Earth coordinates.  Try to convert it to earth horiz units.
	DPoint2 eoffset;
	g_Conv.ConvertVectorToEarth(g_Options.m_fSelectionCutoff, 0, eoffset);
	double epsilon = eoffset.x;
	VTLOG("epsilon %lf, ", epsilon);

	// Check Structures
	int structure;		// index of closest structure
	bool result1 = pTerr->FindClosestStructure(gpos, epsilon, structure, dist1,
		g_Options.m_fMaxPickableInstanceRadius);
	if (result1)
		VTLOG("structure at dist %lf, ", dist1);
	m_bSelectedStruct = false;

	// Check Plants
	vtPlantInstanceArray3d &plants = pTerr->GetPlantInstances();
	plants.VisualDeselectAll();
	m_bSelectedPlant = false;

	// find index of closest plant
	int plant = plants.FindClosestPoint(gpos, epsilon);
	bool result2 = (plant != -1);
	if (result2)
	{
		dist2 = (gpos - plants.GetPoint(plant)).Length();
		VTLOG("plant at dist %lf, ", dist2);
	}
	else
		dist2 = 1E9;

	// Check Routes
	vtRouteMap &routes = pTerr->GetRouteMap();
	m_bSelectedUtil = false;
	bool result3 = routes.FindClosestUtilNode(gpos, epsilon, m_pSelRoute,
		m_pSelUtilNode, dist3);

	// Check Vehicles
	float dist4;
	FPoint3 wpos;
	g_Conv.ConvertFromEarth(m_EarthPos, wpos);
	m_Vehicles.VisualDeselectAll();
	int vehicle = m_Vehicles.FindClosestVehicle(wpos, dist4);
	if (dist4 > g_Options.m_fSelectionCutoff)
		vehicle = -1;

	bool click_struct = (result1 && dist1 < dist2 && dist1 < dist3);
	bool click_plant = (result2 && dist2 < dist1 && dist2 < dist3);
	bool click_route = (result3 && dist3 < dist1 && dist3 < dist2);
	bool click_vehicle = (vehicle!=-1 && dist4 < dist1 && dist4 < dist2 && dist4 < dist3);

	if (click_struct)
	{
		VTLOG(" struct is closest.\n");
		vtStructureArray3d *structures_picked = pTerr->GetStructures();
		vtStructure *str = structures_picked->GetAt(structure);
		vtStructure3d *str3d = structures_picked->GetStructure3d(structure);
		if (str->GetType() != ST_INSTANCE && str3d->GetGeom() == NULL)
		{
			VTLOG("  Warning: unconstructed structure.\n");
		}
		else
		{
			str->Select(true);
			str3d->ShowBounds(true);
			vtStructInstance *inst = str->GetInstance();
			vtFence *fen = str->GetFence();
			if (inst != NULL && (event.flags & VT_SHIFT) != 0)
			{
				m_StartRotation = inst->GetRotation();
				m_bRotating = true;
			}
			else if (fen != NULL)
			{
				// perhaps we have clicked on a fence control point
				double dist;
				int idx = fen->GetNearestPointIndex(gpos, dist);
				if (idx != -1 && dist < 2.0f)	// distance cutoff
				{
					m_pDraggingFence = dynamic_cast<vtFence3d*>(str3d);
					m_iDraggingFencePoint = idx;	// grab
					m_bDragging = true;
				}
				else
				{
					m_pDraggingFence = NULL;
					m_iDraggingFencePoint = -1;		// no grab
				}
			}
			else
				m_bDragging = true;
			m_bSelectedStruct = true;
		}
		if (structures_picked != structures)
		{
			// active structure set (layer) has changed due to picking
			structures->VisualDeselectAll();
			ShowLayerView();
			UpdateLayerView();
		}
	}
	else if (click_plant)
	{
		VTLOG(" plant is closest.\n");
		plants.VisualSelect(plant);
		m_bDragging = true;
		m_bSelectedPlant = true;
	}
	else if (click_route)
	{
		m_bDragging = true;
		m_bSelectedUtil = true;
	}
	else if (click_vehicle)
	{
		VTLOG(" vehicle is closest.\n");
		m_Vehicles.VisualSelect(vehicle);
	}
	else
		VTLOG(" nothing.\n");
}

void Enviro::OnMouseLeftDownTerrainSelect(vtMouseEvent &event)
{
	if (g_Options.m_bDirectPicking)
		OnMouseSelectRayPick(event);
	else
		OnMouseSelectCursorPick(event);

	m_EarthPosDown = m_EarthPosLast = m_EarthPos;
	m_MouseDown = event.pos;
}

void Enviro::OnMouseLeftUp(vtMouseEvent &event)
{
	m_bDragging = m_bRotating = false;

	if (m_state == AS_Orbit && m_mode == MM_MEASURE && m_bDragging)
		UpdateEarthArc();
}

void Enviro::OnMouseRightDown(vtMouseEvent &event)
{
	if (false == m_bLineDrawing)
		return;

	m_bLineDrawing = false;
	if (m_mode == MM_BUILDINGS)
	{
		// Close and create new building in the current structure array
		vtTerrain *pTerr = GetCurrentTerrain();
		vtStructureArray3d *pbuildingarray = pTerr->GetStructures(); 
		vtBuilding3d *pbuilding = (vtBuilding3d*) pbuildingarray->AddNewBuilding();
		pbuilding->SetFootprint(0, m_NewLine);

		// Hide the temporary markers which showed the vertices
		SetupArcMesh();
		for (unsigned int i = 0; i < m_Markers.size(); i++)
		{
			pTerr->RemoveNode(m_Markers[i]);
			m_Markers[i]->Release();
		}
		m_Markers.clear();

		// Describe the appearance of the new building
		pbuilding->SetStories(2);
		pbuilding->SetRoofType(ROOF_HIP);
		pbuilding->SetColor(BLD_BASIC, RGBi(255,255,255));
		pbuilding->SetColor(BLD_ROOF, RGBi(230,200,170));

		// Construct it and add it to the terrain
		pbuilding->CreateNode(pTerr);
		pTerr->AddNodeToStructGrid(pbuilding->GetContainer());
		RefreshLayerView();
	}
}

void Enviro::OnMouseRightUp(vtMouseEvent &event)
{
	if (m_state == AS_Terrain)
	{
		// close off the fence if we have one
		if (m_mode == MM_FENCES)
			close_fence();
		if (m_mode == MM_ROUTES)
			close_route();
		if (m_mode == MM_SELECT)
		{
			vtTerrain *t = GetCurrentTerrain();
			vtStructureArray3d *sa = t->GetStructures();
			vtPlantInstanceArray3d &plants = t->GetPlantInstances();

			if (sa->NumSelected() != 0 || plants.NumSelected() != 0 ||
				m_Vehicles.GetSelected() != -1)
				ShowPopupMenu(event.pos);
		}
	}
}

void Enviro::OnMouseMove(vtMouseEvent &event)
{
	if (m_state == AS_Terrain)
		OnMouseMoveTerrain(event);

	if (m_state == AS_Orbit && m_mode == MM_MEASURE && m_bDragging)
		UpdateEarthArc();
}

void Enviro::OnMouseMoveTerrain(vtMouseEvent &event)
{
	if (m_mode == MM_SELECT && (m_bDragging || m_bRotating))
	{
		DPoint3 delta = m_EarthPos - m_EarthPosLast;
		DPoint2 ground_delta(delta.x, delta.y);

		//VTLOG("ground_delta %f, %f\n", delta.x, delta.y);

		vtTerrain *pTerr = GetCurrentTerrain();
		if (m_bSelectedStruct)
		{
			vtStructureArray3d *structures = pTerr->GetStructures();

			if (m_bDragging)
			{
				if (m_pDraggingFence != NULL)
				{
					// Dragging a linear structure point
					DLine2 &pts = m_pDraggingFence->GetFencePoints();
					pts[m_iDraggingFencePoint] += ground_delta;
					m_pDraggingFence->CreateNode(pTerr);
				}
				else
				{
					// Moving a whole structure (building or instance)
					structures->OffsetSelectedStructures(ground_delta);
				}
			}
			else if (m_bRotating)
			{
				for (int sel = structures->GetFirstSelected(); sel != -1; sel = structures->GetNextSelected())
				{
					vtStructInstance *inst = structures->GetAt(sel)->GetInstance();
					vtStructInstance3d *str3d = structures->GetInstance(sel);

					inst->SetRotation(m_StartRotation + (event.pos.x - m_MouseDown.x) / 100.0f);
					str3d->UpdateTransform(pTerr->GetHeightField());
				}
			}
		}
		if (m_bDragging)
		{
			if (m_bSelectedPlant)
			{
				vtPlantInstanceArray3d &plants = pTerr->GetPlantInstances();
				plants.OffsetSelectedPlants(ground_delta);
			}
			if (m_bSelectedUtil)
			{
				vtRouteMap &routemap = pTerr->GetRouteMap();
				m_pSelUtilNode->Offset(ground_delta);
				m_pSelRoute->Dirty();
				routemap.BuildGeometry(pTerr->GetHeightField());
			}
		}

		m_EarthPosLast = m_EarthPos;
	}
	if (m_mode == MM_SELECT && m_pTerrainPicker != NULL)
	{
		vtTerrain *ter = GetCurrentTerrain();
		if (ter && ter->GetShowPOI())
		{
			ter->HideAllPOI();
			DPoint2 epos(m_EarthPos.x, m_EarthPos.y);
			vtPointOfInterest *poi = ter->FindPointOfInterest(epos);
			if (poi)
				ter->ShowPOI(poi, true);
		}
	}
	if (m_mode == MM_MEASURE && m_bDragging && m_bOnTerrain)
	{
//		VTLOG("MouseMove, MEASURE & Drag & OnTerrain: %.1lf, %.1lf\n", m_EarthPos.x, m_EarthPos.y);
		DPoint2 g1(m_EarthPosDown.x, m_EarthPosDown.y);
		DPoint2 g2(m_EarthPos.x, m_EarthPos.y);
		SetTerrainMeasure(g1, g2);
		ShowDistance(g1, g2, m_fArcLength, m_EarthPos.z - m_EarthPosDown.z);
	}
}

void Enviro::SetupArcMesh()
{
	if (!m_pArcMats)
	{
		m_pArcMats = new vtMaterialArray;
		m_pArcMats->AddRGBMaterial1(RGBf(1, 1, 0), false, false); // yellow
		m_pArcMats->AddRGBMaterial1(RGBf(1, 0, 0), false, false); // red
		m_pArcMats->AddRGBMaterial1(RGBf(1, 0.5f, 0), true, true); // orange lit
	}
	// create geometry container, if needed
	if (!m_pArc)
	{
		m_pArc = new vtGeom;
		if (m_state == AS_Orbit)
			m_pIcoGlobe->GetTop()->AddChild(m_pArc);
		else if (m_state == AS_Terrain)
			GetCurrentTerrain()->GetTopGroup()->AddChild(m_pArc);
		m_pArc->SetMaterials(m_pArcMats);
	}

	// re-create mesh if not the first time
	FreeArcMesh();
}

void Enviro::FreeArc()
{
	FreeArcMesh();
	if (m_pArc)
	{
		m_pArc->Release();
		m_pArc = NULL;
	}
}

void Enviro::FreeArcMesh()
{
	if (m_pArc)
	{
		for (int i = m_pArc->GetNumMeshes()-1; i >= 0; i--)
			m_pArc->RemoveMesh(m_pArc->GetMesh(i));
	}
}

void Enviro::SetTerrainMeasure(const DPoint2 &g1, const DPoint2 &g2)
{
	// place the arc for the distance measuring tool on the terrain
	SetupArcMesh();

	DLine2 dline;
	dline.Append(g1);
	dline.Append(g2);

	vtTerrain *pTerr = GetCurrentTerrain();
	vtMeshFactory mf(m_pArc, vtMesh::LINE_STRIP, 0, 30000, 1);
	m_fArcLength = pTerr->AddSurfaceLineToMesh(&mf, dline, m_fDistToolHeight, true);
}


////////////////////////////////////////////////////////////////
// Fences

void Enviro::start_new_fence()
{
	vtFence3d *fence = new vtFence3d;
	fence->SetParams(m_FenceParams);
	if (GetCurrentTerrain()->AddFence(fence))
	{
		m_pCurFence = fence;

		// update count shown in layer view
		RefreshLayerView();
	}
	else
		delete fence;
}

void Enviro::finish_fence()
{
	m_bActiveFence = false;
}

void Enviro::close_fence()
{
	if (m_bActiveFence && m_pCurFence)
	{
		DLine2 &pts = m_pCurFence->GetFencePoints();
		if (pts.GetSize() > 2)
		{
			DPoint2 FirstFencePoint = pts.GetAt(0);
			m_pCurFence->AddPoint(FirstFencePoint);
			GetCurrentTerrain()->RedrawFence(m_pCurFence);
		}
	}
	m_bActiveFence = false;
}

void Enviro::SetFenceOptions(const vtLinearParams &param)
{
	m_FenceParams = param;

	vtTerrain *pTerr = GetCurrentTerrain();
	if (!pTerr)
		return;

	if (m_bActiveFence)
	{
//		finish_fence();
		m_pCurFence->SetParams(param);
		m_pCurFence->CreateNode(pTerr);	// re-create
	}

	vtStructureArray3d *structures = pTerr->GetStructures();
	for (unsigned int i = 0; i < structures->GetSize(); i++)
	{
		vtStructure *str = structures->GetAt(i);
		if (!str->IsSelected() || str->GetType() != ST_LINEAR)
			continue;
		vtFence3d *pFence = structures->GetFence(i);
		pFence->SetParams(param);
		pFence->CreateNode(pTerr);	// re-create
	}
}


////////////////////////////////////////////////////////////////
// Route

void Enviro::start_new_route()
{
	vtRoute *route = new vtRoute(GetCurrentTerrain());
	GetCurrentTerrain()->AddRoute(route);
	m_pCurRoute = route;
}

void Enviro::finish_route()
{
	m_bActiveRoute = false;
}

void Enviro::close_route()
{
	if (m_bActiveRoute && m_pCurRoute)
	{
		GetCurrentTerrain()->SaveRoute();
	}
	m_bActiveRoute = false;
}

void Enviro::SetRouteOptions(const vtString &sStructType)
{
	m_sStructType = sStructType;
}


////////////////////////////////////////////////
// Plants

void Enviro::SetPlantOptions(PlantingOptions &opt)
{
	m_PlantOpt = opt;
	if (m_mode == MM_SELECT)
	{
		vtPlantInstanceArray3d &pia = GetCurrentTerrain()->GetPlantInstances();
		for (unsigned int i = 0; i < pia.GetNumEntities(); i++)
		{
			if (pia.IsSelected(i))
			{
				pia.SetPlant(i, opt.m_fHeight, opt.m_iSpecies);
				pia.CreatePlantNode(i);
			}
		}
	}
}

/**
 * Plant a tree at the given location (in earth coordinates)
 */
bool Enviro::PlantATree(const DPoint2 &epos)
{
	if (!m_pPlantList)
		return false;

	vtTerrain *pTerr = GetCurrentTerrain();
	if (!pTerr)
		return false;

	// check distance from other plants
	vtPlantInstanceArray &pia = pTerr->GetPlantInstances();
	int size = pia.GetNumEntities();
	double len, closest = 1E8;
	DPoint2 diff;

	bool bPlant = true;
	if (m_PlantOpt.m_fSpacing > 0.0f)
	{
		// Spacing is in meters, but the picking functions work in
		//  Earth coordinates.  Try to convert it to earth horiz units.
		DPoint2 eoffset;
		g_Conv.ConvertVectorToEarth(m_PlantOpt.m_fSpacing, 0, eoffset);
		double epsilon = eoffset.x;

		for (int i = 0; i < size; i++)
		{
			diff = epos - pia.GetPoint(i);
			len = diff.Length();

			if (len < closest) closest = len;
		}
		if (closest < epsilon)
			bPlant = false;
		VTLOG(" closest plant %.2fm,%s planting..", closest, bPlant ? "" : " not");
	}
	if (!bPlant)
		return false;

	float height = m_PlantOpt.m_fHeight;
	float variance = m_PlantOpt.m_iVariance / 100.0f;
	height *= (1.0f + random(variance*2) - variance);
	if (!pTerr->AddPlant(epos, m_PlantOpt.m_iSpecies, height))
		return false;

	// If there is a GUI, let it update to show one more plant
	UpdateLayerView();
	return true;
}


//// Instances

void Enviro::PlantInstance()
{
#if 0	// test code
	#include "CreateWedge.cpp"
	return;
#endif

	VTLOG("Plant Instance: ");
	vtTagArray *tags = GetInstanceFromGUI();
	if (!tags)
		return;

	// create a new Instance object
	vtTerrain *pTerr = GetCurrentTerrain();
	vtStructureArray3d *structs = pTerr->GetStructures();
	vtStructInstance3d *inst = (vtStructInstance3d *) structs->NewInstance();
	inst->CopyTagsFrom(*tags);
	inst->SetPoint(DPoint2(m_EarthPos.x, m_EarthPos.y));
	VTLOG("  at %.7g, %.7g: ", m_EarthPos.x, m_EarthPos.y);

	int index = structs->Append(inst);
	bool success = pTerr->CreateStructure(structs, index);
	if (success)
	{
		VTLOG(" succeeded.\n");
		RefreshLayerView();
	}
	else
	{
		// creation failed
		VTLOG(" failed.\n");
		ShowMessage("Could not create instance.");
		inst->Select(true);
		structs->DeleteSelected();
		return;
	}
}

void Enviro::DescribeCoordinatesTerrain(vtString &str)
{
	DPoint3 epos;
	vtString str1;

	str = "";

#if 0
	// give location of camera and cursor
	str = "Camera: ";
	// get camera pos
	vtScene *scene = vtGetScene();
	vtCamera *camera = scene->GetCamera();
	FPoint3 campos = camera->GetTrans();

	// Find corresponding earth coordinates
	g_Conv.ConvertToEarth(campos, epos);

	FormatCoordString(str1, epos, g_Conv.GetUnits());
	str += str1;
	str1.Format(" elev %.1f", epos.z);
	str += str1;
	str += ", ";
#endif

	// ground cursor
	str += "Cursor:";
	bool bOn = m_pTerrainPicker->GetCurrentEarthPos(epos);
	if (bOn)
	{
		vtString str1;
		FormatCoordString(str1, epos, g_Conv.GetUnits(), true);
		str += str1;
	}
	else
		str += " Not on ground";
}

void Enviro::DescribeCLOD(vtString &str)
{
	str = "";

	if (m_state != AS_Terrain) return;
	vtTerrain *t = GetCurrentTerrain();
	if (!t) return;
	vtDynTerrainGeom *dtg = t->GetDynTerrain();
	if (!dtg) return;

	//
	// McNally CLOD algo uses a triangle count target, all other current
	// implementations use a floating point factor relating to error/detail
	//
	LodMethodEnum method = t->GetParams().GetLodMethod();
	if (method == LM_MCNALLY || method == LM_ROETTGER)
	{
		str.Format("CLOD: target %d, drawn %d ", dtg->GetPolygonTarget(),
			dtg->GetNumDrawnTriangles());
	}
}

vtString Enviro::GetStatusString(int which)
{
	vtScene *scene = vtGetScene();

	vtString str;
	if (which == 0)
	{
		// Fps: get framerate
		float fps = scene->GetFrameRate();

		// only show 3 significant digits
		if (fps < 10)
			str.Format("fps %1.2f", fps);
		else if (fps < 80)
			str.Format("fps %2.1f", fps);
		else
			str.Format("fps %3.0f", fps);

		return str;
	}
	if (which == 1)
	{
		if (m_state == AS_Orbit)
			DescribeCoordinatesEarth(str);
		else if (m_state == AS_Terrain)
			DescribeCoordinatesTerrain(str);
	}
	if (which == 2)
	{
		DPoint3 epos;

		if (m_state == AS_Orbit)
		{
			m_pGlobePicker->GetCurrentEarthPos(epos);
			vtTerrain *pTerr = FindTerrainOnEarth(DPoint2(epos.x, epos.y));
			if (pTerr)
				str = pTerr->GetName();
		}
		else if (m_state == AS_Terrain)
		{
			bool bOn = m_pTerrainPicker->GetCurrentEarthPos(epos);
			if (bOn)
			{
				float exag;
				{
					// Avoid trouble with '.' and ',' in Europe
					LocaleWrap normal_numbers(LC_NUMERIC, "C");
					// Report true elevation, without vertical exaggeration
					exag = GetCurrentTerrain()->GetVerticalExag();
				}
				epos.z /= exag;
				str.Format("Elev: %.1f", epos.z);
			}
			else
				str += "Not on ground";
		}
	}
	return str;
}

// Handle the map overview option
void Enviro::ShowMapOverview(bool bShow)
{
	if (bShow && !m_pMapOverview)
		CreateMapOverview();
	if (m_pMapOverview)
		m_pMapOverview->ShowMapOverview(bShow);
}

bool Enviro::GetShowMapOverview()
{
	if (m_pMapOverview)
		return m_pMapOverview->GetShowMapOverview();
	return false;
}

void Enviro::CreateMapOverview()
{
	// setup the mapoverview engine
	if (!m_pMapOverview)
	{
		m_pMapOverview = new MapOverviewEngine;
		m_pMapOverview->SetName2("Map overview engine");
		vtGetScene()->AddEngine(m_pMapOverview);
	}
}

void Enviro::TextureHasChanged()
{
	// Texture has changed, so update the overview
	if (m_pMapOverview && m_pMapOverview->GetShowMapOverview())
	{
		m_pMapOverview->SetTerrain(GetCurrentTerrain());
	}
}

void Enviro::ShowElevationLegend(bool bShow)
{
	if (bShow && !m_bCreatedLegend)
		CreateElevationLegend();
	if (m_pLegendGeom)
		m_pLegendGeom->SetEnabled(bShow);
}

bool Enviro::GetShowElevationLegend()
{
	if (m_pLegendGeom)
		return m_pLegendGeom->GetEnabled();
	return false;
}

void Enviro::CreateElevationLegend()
{
	// Must have a color-mapped texture on the terrain to show a legend
	ColorMap *cmap = GetCurrentTerrain()->GetTextureColors();
	if (!cmap)
		return;

	// Define the size and shape of the legend: input values
	const int ticks = 8;
	const IPoint2 border(10, 18);
	const IPoint2 base(10, 10);
	const IPoint2 size(140, 230);
	const int fontsize = 16;

	// Derived values
	const IPoint2 in_base = base + border;
	const IPoint2 in_size(size.x - (border.x*2), size.y - (border.y*2));
	const int vert_space = in_size.y / (ticks-1);
	const int cbar_left = in_base.x + (in_size.x * 6 / 10);
	const int cbar_right = in_base.x + in_size.x;

	int i, idx;
	vtMaterialArray *pMats = new vtMaterialArray;
	pMats->AddRGBMaterial1(RGBf(1, 1, 1), false, false); // white
	pMats->AddRGBMaterial1(RGBf(.2, .2, .2), false, false); // dark grey

	m_pLegendGeom = new vtGeom;
	m_pLegendGeom->SetMaterials(pMats);
	pMats->Release();

	// Solid rectangle behind it
	vtMesh *mesh4 = new vtMesh(vtMesh::QUADS, 0, 4);
	mesh4->AddRectangleXY(base.x, base.y, size.x, size.y, -1.0f);
	m_pLegendGeom->AddMesh(mesh4, 1);
	mesh4->Release();

	// Big band of color
	std::vector<RGBi> table;
	cmap->GenerateColors(table, in_size.y, 0, 1);
	vtMesh *mesh1 = new vtMesh(vtMesh::TRIANGLE_STRIP, VT_Colors, (in_size.y + 1)*2);
	for (i = 0; i < in_size.y + 1; i++)
	{
		FPoint3 p1(cbar_left,  in_base.y + i, 0.0f);
		FPoint3 p2(cbar_right, in_base.y + i, 0.0f);
		idx = mesh1->AddLine(p1, p2);
		mesh1->SetVtxColor(idx, (RGBf) table[i]);
		mesh1->SetVtxColor(idx+1, (RGBf) table[i]);
	}
	mesh1->AddStrip2((in_size.y + 1)*2, 0);
	m_pLegendGeom->AddMesh(mesh1, 0);
	mesh1->Release();

	// Small white tick marks
	vtMesh *mesh2 = new vtMesh(vtMesh::LINES, 0, ticks*2);
	for (i = 0; i < ticks; i++)
	{
		FPoint3 p1(cbar_left-border.x*2, in_base.y + i*vert_space, 0.0f);
		FPoint3 p2(cbar_left,			 in_base.y + i*vert_space, 0.0f);
		mesh2->AddLine(p1, p2);
	}
	m_pLegendGeom->AddMesh(mesh2, 0);
	mesh2->Release();

	float fMin, fMax;
	GetCurrentTerrain()->GetHeightField()->GetHeightExtents(fMin, fMax);

	// Text labels
	vtFont *font = new vtFont;
	vtString font_path = FindFileOnPaths(vtGetDataPath(), "Fonts/Arial.ttf");
	if (font_path != "")
		font->LoadFont(font_path);
	for (i = 0; i < ticks; i++)
	{
		vtTextMesh *mesh3 = new vtTextMesh(font, fontsize, false);
		vtString str;
		str.Format("%4.1f", fMin + (fMax - fMin) / (ticks-1) * i);
		mesh3->SetText(str);
		FPoint3 p1(in_base.x, in_base.y + i*vert_space - (fontsize*1/3), 0.0f);
		mesh3->SetPosition(p1);

		m_pLegendGeom->AddTextMesh(mesh3, 0);
		mesh3->Release();
	}
	delete font;

	m_pHUD->AddChild(m_pLegendGeom);
	m_bCreatedLegend = true;
}


////////////////////////////////////////////////////////////////////////
#include "CarEngine.h"

void Enviro::CreateTestVehicle()
{
	// Create test vehicle
	vtTerrain *pTerr = GetCurrentTerrain();
	if (!pTerr)
		return;

	DPoint3 epos;
	bool bOn = m_pTerrainPicker->GetCurrentEarthPos(epos);
	if (!bOn)
		return;

	vtTransform *car = m_VehicleManager.CreateVehicle("Bronco", RGBf(1,1,1));
	if (!car)
		return;
	pTerr->AddNode(car);

	pTerr->PlantModelAtPoint(car, DPoint2(epos.x, epos.y));

	float speed = 0.0f;		// kmph
	float wheel_radius = 0.25f;
	CarEngine *pE1 = new CarEngine(pTerr->GetHeightField(), speed, wheel_radius, car->GetTrans());
	pE1->SetName2("drive");
	pE1->SetTarget(car);
	if (pE1->FindWheelTransforms())
	{
		pTerr->AddEngine(pE1);
		m_Vehicles.AddEngine(pE1);
	}
}

void Enviro::CreateSomeTestVehicles(vtTerrain *pTerrain, int iNum, float fSpeed)
{
	vtRoadMap3d *pRoadMap = pTerrain->GetRoadMap();

	// How many four-wheel land vehicles are there in the content catalog?
	vtStringArray vnames;
	vtContentManager3d &con = vtGetContent();
	for (unsigned int i = 0; i < con.NumItems(); i++)
	{
		vtItem *item = con.GetItem(i);
		if (vtString("ground vehicle") == item->GetValueString("type") &&
			4 == item->GetValueInt("num_wheels"))
		{
			vnames.push_back(item->m_name);
		}
	}
	unsigned int numv = vnames.size();

	// add some test vehicles
	NodeGeom *road_node = NULL;
	for (unsigned int i = 0; i < iNum; i++)
	{
		if (road_node == NULL)
			road_node = pRoadMap->GetFirstNode();

		RGBf color;
		switch (i % 8)
		{
		case 0: color.Set(1.0f, 1.0f, 1.0f); break;	// white
		case 1: color.Set(1.0f, 1.0f, 0.0f); break; // yellow
		case 2: color.Set(0.3f, 0.6f, 1.0f); break; // medium blue
		case 3: color.Set(1.0f, 0.3f, 0.3f); break; // red
		case 4: color.Set(0.5f, 1.0f, 0.5f); break; // medium green
		case 5: color.Set(0.2f, 0.2f, 0.2f); break; // black/dk.grey
		case 6: color.Set(0.1f, 0.6f, 0.1f); break; // dk.green
		case 7: color.Set(1.0f, 0.8f, 0.6f); break; // tan
		}

		// Create some of each land vehicle type
		int vnum = i % numv;
		vtTransform *car = m_VehicleManager.CreateVehicle(vnames[vnum], color);
		if (car)
		{
			pTerrain->AddNode(car);
			pTerrain->PlantModelAtPoint(car, road_node->m_p);

			float fSpeed = 60.0f;	// kmph

			CarEngine *pE1;
			if (road_node == NULL)
			{
				pE1 = new CarEngine(pTerrain->GetHeightField(), fSpeed, .25f,
					car->GetTrans());
			}
			else
			{
				pE1 = new CarEngine(pTerrain->GetHeightField(), fSpeed, .25f,
					road_node, 1);
			}
			pE1->SetName2("drive");
			pE1->SetTarget(car);
			if (pE1->FindWheelTransforms())
				pTerrain->AddEngine(pE1);
			else
				delete pE1;
			m_Vehicles.AddEngine(pE1);
		}
		road_node = (NodeGeom*) road_node->m_pNext;
	}
}


////////////////////////////////////////////////////////////////////////

void ControlEngine::Eval()
{
	Enviro::s_pEnviro->DoControl();
}

////////////////////////////////////////////////////////////////////////

vtTerrain *GetCurrentTerrain()
{
	return Enviro::s_pEnviro->GetCurrentTerrain();
}
