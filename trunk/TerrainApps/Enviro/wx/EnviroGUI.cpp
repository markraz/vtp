//
// EnviroGUI.cpp
// GUI-specific functionality of the Enviro class
//
// Copyright (c) 2003-2005 Virtual Terrain Project
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
#include "EnviroGUI.h"
#include "EnviroApp.h"
#include "EnviroFrame.h"
#include "canvas.h"
#include "LayerDlg.h"
#include "vtui/InstanceDlg.h"
#include "vtui/DistanceDlg.h"

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
}

void EnviroGUI::RefreshLayerView()
{
	LayerDlg *dlg = GetFrame()->m_pLayerDlg;
	dlg->RefreshTreeContents();
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
							 float fGround, float fVertical)
{
	GetFrame()->m_pDistanceDlg->SetPoints(p1, p2, false);
	GetFrame()->m_pDistanceDlg->SetGroundAndVertical(fGround, fVertical, true);
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
}

void EnviroGUI::SetTimeEngineToGUI(class TimeEngine *pEngine)
{
	GetFrame()->SetTimeEngine(pEngine);
}

//////////////////////////////////////////////////////////////////////

void EnviroGUI::SaveVegetation()
{
	// save current directory
	wxString path = wxGetCwd();

	EnableContinuousRendering(false);
	wxFileDialog saveFile(NULL, _("Save Vegetation Data"), _T(""), _T(""),
		_("Vegetation Files (*.vf)|*.vf"), wxSAVE);
	bool bResult = (saveFile.ShowModal() == wxID_OK);
	EnableContinuousRendering(true);
	if (!bResult)
	{
		wxSetWorkingDirectory(path);	// restore
		return;
	}
	wxString2 str = saveFile.GetPath();

	vtTerrain *pTerr = GetCurrentTerrain();
	vtPlantInstanceArray &pia = pTerr->GetPlantInstances();
	pia.WriteVF(str.mb_str());
}

void EnviroGUI::SaveStructures(bool bAskFilename)
{
	vtStructureArray3d *sa = GetCurrentTerrain()->GetStructures();
	vtString fname = sa->GetFilename();
	if (bAskFilename)
	{
		// save current directory
		wxString path = wxGetCwd();

		wxString2 default_file = StartOfFilename(fname);
		wxString2 default_dir = ExtractPath(fname);

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
		wxString2 str = saveFile.GetPath();

		sa->SetFilename(str.mb_str());
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

	wxString2 str2 = str;
	wxMessageBox(str2);

	EnableContinuousRendering(true);
}
