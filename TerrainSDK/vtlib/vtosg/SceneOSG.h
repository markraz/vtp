//
// SceneOSG.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef VTOSG_SCENEH
#define VTOSG_SCENEH

#include "../core/FrameTimer.h"
#include <osg/Timer>

/**
 * A Scene is the all-encompassing container for all 3D objects
 * that are to be managed and drawn by the scene graph / graphics
 * pipeline functionality of vtlib.
 *
 * A Scene currently encapsulates:
 *	- A scene graph
 *	- A set of engines (vtEngine)
 *  - A window
 *  - A current camera (vtCamera)
 */
class vtScene : public vtSceneBase
{
public:
	vtScene();
	~vtScene();

	void SetBgColor(RGBf color);
	void SetAmbient(RGBf color);
	void SetRoot(vtGroup *pRoot);

	void SetGlobalWireframe(bool bWire);
	bool GetGlobalWireframe();

	bool Init();
	void Shutdown();

	void DoUpdate();
	float GetFrameRate()
	{
		return 1.0 / _timer.delta_s(_lastFrameTick,_frameTick);
	}
	void DrawFrameRateChart();

	bool HasWinInfo() { return m_bWinInfo; }
	void SetWinInfo(void *handle, void *context) { m_bWinInfo = true; }

	float GetTime()
	{
		return _timer.delta_s(_initialTick,_frameTick);
	}
	float GetFrameTime()
	{
		return _timer.delta_s(_lastFrameTick,_frameTick);
	}

	// View methods
	bool CameraRay(const IPoint2 &win, FPoint3 &pos, FPoint3 &dir);
	FPlane *GetCullPlanes() { return m_cullPlanes; }

protected:
	// OSG-specific implementation
	osg::ref_ptr<osgUtil::SceneView>	m_pOsgSceneView;

	// for culling
	void CalcCullPlanes();
	FPlane		m_cullPlanes[6];

	osg::ref_ptr<osg::Group>	m_pOsgSceneRoot;

	osg::Timer   _timer;
	osg::Timer_t _initialTick;
	osg::Timer_t _lastFrameTick;
	osg::Timer_t _frameTick;

	bool	m_bWinInfo;
	bool	m_bInitialized;
	bool	m_bWireframe;
};

// global
vtScene *vtGetScene();
float vtGetTime();
float vtGetFrameTime();

#endif	// VTOSG_SCENEH

