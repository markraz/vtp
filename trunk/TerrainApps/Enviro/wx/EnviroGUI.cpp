//
// EnviroGUI.cpp
// GUI-specific functionality of the Enviro class
//
// Copyright (c) 2003-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "vtlib/vtlib.h"
#include "vtlib/core/TerrainScene.h"
#include "vtlib/core/Terrain.h"
#include "vtdata/vtLog.h"
#include "EnviroGUI.h"
#include "EnviroApp.h"
#include "EnviroFrame.h"
#include "canvas.h"
#include "LayerDlg.h"
#include "vtui/InstanceDlg.h"
#include "vtui/DistanceDlg.h"
#include "vtui/ProfileDlg.h"
#include "vtui/Joystick.h"

DECLARE_APP(EnviroApp);

//
// This is a 'singleton', the only instance of the global application object
//
EnviroGUI g_App;

// helper
EnviroFrame *GetFrame()
{
	return dynamic_cast<EnviroFrame *>(wxGetApp().GetTopWindow());
}

EnviroGUI::EnviroGUI()
{
	m_pJFlyer = NULL;
}

EnviroGUI::~EnviroGUI()
{
}

void EnviroGUI::ShowPopupMenu(const IPoint2 &pos)
{
	GetFrame()->ShowPopupMenu(pos);
}

void EnviroGUI::SetTerrainToGUI(vtTerrain *pTerrain)
{
	GetFrame()->SetTerrainToGUI(pTerrain);

	if (pTerrain && m_pJFlyer)
	{
		float speed = pTerrain->GetParams().GetValueFloat(STR_NAVSPEED);
		m_pJFlyer->SetSpeed(speed);
	}
}

void EnviroGUI::SetFlightSpeed(float speed)
{
	if (m_pJFlyer)
		m_pJFlyer->SetSpeed(speed);
	Enviro::SetFlightSpeed(speed);
}

void EnviroGUI::RefreshLayerView()
{
	LayerDlg *dlg = GetFrame()->m_pLayerDlg;
	dlg->RefreshTreeContents();
}

void EnviroGUI::UpdateLayerView()
{
	LayerDlg *dlg = GetFrame()->m_pLayerDlg;
	dlg->UpdateTreeTerrain();
}

void EnviroGUI::ShowLayerView()
{
	LayerDlg *dlg = GetFrame()->m_pLayerDlg;
	dlg->Show(true);
}

void EnviroGUI::CameraChanged()
{
	GetFrame()->CameraChanged();
}

void EnviroGUI::EarthPosUpdated()
{
	GetFrame()->EarthPosUpdated(m_EarthPos);
}

void EnviroGUI::ShowDistance(const DPoint2 &p1, const DPoint2 &p2,
							 double fGround, double fVertical)
{
	GetFrame()->m_pDistanceDlg->SetPoints(p1, p2, false);
	GetFrame()->m_pDistanceDlg->SetGroundAndVertical(fGround, fVertical, true);

	if (GetFrame()->m_pProfileDlg)
		GetFrame()->m_pProfileDlg->SetPoints(p1, p2);
}

vtTagArray *EnviroGUI::GetInstanceFromGUI()
{
	return GetFrame()->m_pInstanceDlg->GetTagArray();
}

bool EnviroGUI::OnMouseEvent(vtMouseEvent &event)
{
	return GetFrame()->OnMouseEvent(event);
}

void EnviroGUI::SetupScene3()
{
	GetFrame()->Setup3DScene();

#if wxUSE_JOYSTICK || WIN32
	m_pJFlyer = new vtJoystickEngine;
	m_pJFlyer->SetName2("Joystick");
	vtGetScene()->AddEngine(m_pJFlyer);
	m_pJFlyer->SetTarget(m_pNormalCamera);
#endif
}

void EnviroGUI::SetTimeEngineToGUI(class TimeEngine *pEngine)
{
	GetFrame()->SetTimeEngine(pEngine);
}

//////////////////////////////////////////////////////////////////////

void EnviroGUI::SaveVegetation(bool bAskFilename)
{
	vtTerrain *pTerr = GetCurrentTerrain();
	vtPlantInstanceArray &pia = pTerr->GetPlantInstances();

	vtString fname = pia.GetFilename();

	if (bAskFilename)
	{
		// save current directory
		wxString path = wxGetCwd();

		wxString default_file(StartOfFilename(fname), wxConvUTF8);
		wxString default_dir(ExtractPath(fname), wxConvUTF8);

		EnableContinuousRendering(false);
		wxFileDialog saveFile(NULL, _("Save Vegetation Data"), default_dir,
			default_file, _("Vegetation Files (*.vf)|*.vf"), wxSAVE);
		bool bResult = (saveFile.ShowModal() == wxID_OK);
		EnableContinuousRendering(true);
		if (!bResult)
		{
			wxSetWorkingDirectory(path);	// restore
			return;
		}
		wxString str = saveFile.GetPath();
		fname = str.mb_str(wxConvUTF8);
		pia.SetFilename(fname);
	}
	pia.WriteVF(fname);
}

void EnviroGUI::SaveStructures(bool bAskFilename)
{
	vtStructureArray3d *sa = GetCurrentTerrain()->GetStructures();
	vtString fname = sa->GetFilename();
	if (bAskFilename)
	{
		// save current directory
		wxString path = wxGetCwd();

		wxString default_file(StartOfFilename(fname), wxConvUTF8);
		wxString default_dir(ExtractPath(fname), wxConvUTF8);

		EnableContinuousRendering(false);
		wxFileDialog saveFile(NULL, _("Save Built Structures Data"),
			default_dir, default_file, _("Structure Files (*.vtst)|*.vtst"),
			wxSAVE | wxOVERWRITE_PROMPT);
		bool bResult = (saveFile.ShowModal() == wxID_OK);
		EnableContinuousRendering(true);
		if (!bResult)
		{
			wxSetWorkingDirectory(path);	// restore
			return;
		}
		wxString str = saveFile.GetPath();
		fname = str.mb_str(wxConvUTF8);
		sa->SetFilename(fname);
	}
	sa->WriteXML(fname);
}

bool EnviroGUI::IsAcceptable(vtTerrain *pTerr)
{
	return GetFrame()->IsAcceptable(pTerr);
}

void EnviroGUI::ShowMessage(const vtString &str)
{
	EnableContinuousRendering(false);

	wxString str2(str, wxConvUTF8);
	wxMessageBox(str2);

	EnableContinuousRendering(true);
}

///////////////////////////////////////////////////////////////////////

vtJoystickEngine::vtJoystickEngine()
{
	m_fSpeed = 1.0f;
	m_fLastTime = 0.0f;

	m_pStick = new wxJoystick;
	if (!m_pStick->IsOk())
	{
		delete m_pStick;
		m_pStick = NULL;
	}
}
void vtJoystickEngine::Eval()
{
	if (!m_pStick)
		return;

	float fTime = vtGetTime(), fElapsed = fTime - m_fLastTime;

	vtTransform *pTarget = (vtTransform*) GetTarget();
	if (pTarget)
	{
		wxPoint p = m_pStick->GetPosition();
		int buttons = m_pStick->GetButtonState();
		float dx = ((float)p.x / 32768) - 1.0f;
		float dy = ((float)p.y / 32768) - 1.0f;

		// use a small dead zone to avoid drift
		const float dead_zone = 0.04f;

		if (buttons & wxJOY_BUTTON2)
		{
			// move up down left right
			if (fabs(dx) > dead_zone)
				pTarget->TranslateLocal(FPoint3(dx * m_fSpeed * fElapsed, 0.0f, 0.0f));
			if (fabs(dy) > dead_zone)
				pTarget->Translate1(FPoint3(0.0f, dy * m_fSpeed * fElapsed, 0.0f));
		}
		else if (buttons & wxJOY_BUTTON3)
		{
			// pitch up down, yaw left right
			if (fabs(dx) > dead_zone)
				pTarget->RotateParent(FPoint3(0,1,0), -dx * fElapsed);
			if (fabs(dy) > dead_zone)
				pTarget->RotateLocal(FPoint3(1,0,0), dy * fElapsed);
		}
		else
		{
			// move forward-backward, turn left-right
			if (fabs(dy) > dead_zone)
				pTarget->TranslateLocal(FPoint3(0.0f, 0.0f, dy * m_fSpeed * fElapsed));
			if (fabs(dx) > dead_zone)
				pTarget->RotateParent(FPoint3(0,1,0), -dx * fElapsed);
		}
	}
	m_fLastTime = fTime;
}
