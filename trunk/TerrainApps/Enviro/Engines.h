//
// Engines.h
// header file for Engines.cpp
//

#ifndef ENVIRO_ENGINESH
#define ENVIRO_ENGINESH

#include "vtlib/core/Engine.h"
#include "vtlib/core/NavEngines.h"

class NodeGeom;
class LinkGeom;
class vtRoute;
class vtHeightField;

enum AirportCodes {ITO, KOA, MUE, UPP};


///////////////////////////////////////////////////

class BgColorEngine : public vtEngine
{
public:
	void Eval();
};

///////////////////////////////////////////////////

class PlaneEngine : public vtEngine
{
public:
	PlaneEngine(float fSpeedExag, AirportCodes code);
	void Eval();
	void SetHoop(int i);

	FPoint3	m_hoop_pos[10];
	float	m_hoop_speed[10];
	int		m_hoops;
	int		m_hoop;

	FPoint3	m_pos, m_dir;
	float	m_fSpeed;		// in meters/sec
	float	m_fSpeedExag;	// this many times normal speed
	float	m_fLastTime;
};

///////////////////////////////////////////////////

class RoadFollowEngine : public vtEngine
{
public:
	RoadFollowEngine(NodeGeom *pStartNode);
	void Eval();

	void	PickRoad(LinkGeom *last_road);

	float	fAmount;
//	RoadMap	*m_pRoadMap;
	NodeGeom	*m_pLastNode;
	LinkGeom	*m_pCurrentRoad;
	float	m_pCurrentRoadLength;
	bool	forwards;
};

//////////////////////////////////////////////////

// Follow the current route
class RouteFollowerEngine : public vtEngine
{
public:
	RouteFollowerEngine(vtRoute *route);
	void Eval();

	vtHeightField3d *m_pHeightField;
	vtRoute *m_pRoute;

	int m_cur, m_next;
	float m_inc;
};

//////////////////////////////////////////////////

class FollowerEngine : public vtEngine
{
public:
	FollowerEngine(vtTransform* model, vtCamera* camera);
	void Eval();

	vtTransform *m_model;
	vtCamera *m_pCamera;
};

class SimpleBBEngine : public vtEngine
{
public:
	SimpleBBEngine(vtTransform* model, vtCamera* camera);
	void Eval();

	vtTransform *m_model;
	vtCamera *m_pCamera;
	float m_fAngle;
};

///////////////////////////////////////////////////

class TerrainPicker : public vtLastMouse
{
public:
	TerrainPicker();
	void Eval();
	void SetHeightField(vtHeightField3d *pHeight) { m_pHeightField = pHeight; }

	bool GetCurrentPoint(FPoint3 &p);
	bool GetCurrentEarthPos(DPoint3 &p);

protected:
	FPoint3			m_GroundPoint;
	vtHeightField3d *m_pHeightField;
	DPoint3			m_EarthPos;
	bool			m_bOnTerrain;
};

///////////////////////////////////////////////////

class GlobePicker : public vtLastMouse
{
public:
	GlobePicker();
	void Eval();
	void SetRadius(double fRadius) { m_fRadius = fRadius; }
	void SetGlobeMGeom(vtTransform *pMG) { m_pGlobeXForm = pMG; }

	bool GetCurrentPoint(FPoint3 &p);
	bool GetCurrentEarthPos(DPoint3 &p);

protected:
	FPoint3		m_GroundPoint;
	DPoint3		m_EarthPos;
	bool		m_bOnTerrain;
	double		m_fRadius;
	vtTransform	*m_pGlobeXForm;
};

///////////////////////////////////////////////////

class GrabFlyer : public vtTerrainFlyer
{
public:
	GrabFlyer(float fSpeed, float fHeightAboveTerrain, bool bMin);
	void Eval();
	void OnMouse(vtMouseEvent &event);

	void SetTerrainPicker(TerrainPicker *pTP) { m_pTP = pTP; }

protected:
	bool m_bDrag;
	bool m_bPivot;
	bool m_bZoom;
	IPoint2 m_startpos;

	TerrainPicker *m_pTP;
	FPoint3 m_start_wp;
	FMatrix4 m_start_eye;

	// for dragging
	float m_fHeight;
};

#endif	// ENVIRO_ENGINESH

