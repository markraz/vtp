//
// Name: EphemDlg.cpp
//
// Copyright (c) 2007 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include <wx/colordlg.h>

#include "vtlib/vtlib.h"
#include "vtlib/core/Terrain.h"
#include "vtlib/core/SkyDome.h"
#include "vtdata/vtLog.h"
#include "EphemDlg.h"
#include "EnviroGUI.h"	  // for g_App
#include "vtui/Helper.h"	// for FillWithColor

// WDR: class implementations

//----------------------------------------------------------------------------
// EphemDlg
//----------------------------------------------------------------------------

// WDR: event table for EphemDlg

BEGIN_EVENT_TABLE(EphemDlg,AutoDialog)
	EVT_INIT_DIALOG (EphemDlg::OnInitDialog)
	EVT_CHECKBOX( ID_OCEANPLANE, EphemDlg::OnCheckBox )
	EVT_CHECKBOX( ID_SKY, EphemDlg::OnCheckBox )
	EVT_COMBOBOX( ID_SKYTEXTURE, EphemDlg::OnSkyTexture )
	EVT_CHECKBOX( ID_HORIZON, EphemDlg::OnCheckBox )
	EVT_CHECKBOX( ID_FOG, EphemDlg::OnCheckBox )
	EVT_BUTTON( ID_BGCOLOR, EphemDlg::OnBgColor )
	EVT_TEXT( ID_FOG_DISTANCE, EphemDlg::OnFogDistance )
	EVT_TEXT( ID_TEXT_WIND_DIRECTION, EphemDlg::OnWindDirection )
	EVT_TEXT( ID_TEXT_WIND_SPEED, EphemDlg::OnWindSpeed )
	EVT_SLIDER( ID_SLIDER_FOG_DISTANCE, EphemDlg::OnSliderFogDistance )
	EVT_SLIDER( ID_SLIDER_WIND_DIRECTION, EphemDlg::OnSliderWindDirection )
	EVT_SLIDER( ID_SLIDER_WIND_SPEED, EphemDlg::OnSliderWindSpeed)
END_EVENT_TABLE()

EphemDlg::EphemDlg( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	AutoDialog( parent, id, title, position, size, style )
{
	// WDR: dialog function EphemDialogFunc for EphemDlg
	EphemDialogFunc( this, TRUE ); 

	m_bSetting = false;
	m_iWindDirSlider = 0;
	m_iWindSpeedSlider = 0;
	m_iWindDir = 0;
	m_fWindSpeed = 0;

	AddValidator(ID_SKY, &m_bSky);
	AddValidator(ID_SKYTEXTURE, &m_strSkyTexture);

	AddValidator(ID_OCEANPLANE, &m_bOceanPlane);
	AddNumValidator(ID_OCEANPLANEOFFSET, &m_fOceanPlaneLevel);
	AddValidator(ID_HORIZON, &m_bHorizon);

	AddValidator(ID_FOG, &m_bFog);
	AddNumValidator(ID_FOG_DISTANCE, &m_fFogDistance);
	AddValidator(ID_SLIDER_FOG_DISTANCE, &m_iFogDistance);

	AddNumValidator(ID_TEXT_WIND_DIRECTION, &m_iWindDir);
	AddValidator(ID_SLIDER_WIND_DIRECTION, &m_iWindDirSlider);

	AddNumValidator(ID_TEXT_WIND_SPEED, &m_fWindSpeed);
	AddValidator(ID_SLIDER_WIND_SPEED, &m_iWindSpeedSlider);
}

void EphemDlg::UpdateEnableState()
{
	GetOceanPlaneOffset()->Enable(m_bOceanPlane);
	GetSkyTexture()->Enable(m_bSky);
	GetFogDistance()->Enable(m_bFog);
	GetSliderFogDistance()->Enable(m_bFog);
}

#define DIST_MIN 1.0f	// 10 m
#define DIST_MAX 4.69897000433f	// 50 km
#define DIST_RANGE (DIST_MAX-DIST_MIN)

void EphemDlg::ValuesToSliders()
{
	m_iWindDirSlider = m_iWindDir / 2;
	m_iWindSpeedSlider = (int) (m_fWindSpeed / 4 * 15);
	m_iFogDistance = (int) ((log10f(m_fFogDistance) - DIST_MIN) / DIST_RANGE * 100);
}

void EphemDlg::SlidersToValues()
{
	m_iWindDir = m_iWindDirSlider * 2;
	m_fWindSpeed = (float) m_iWindSpeedSlider * 4 / 15;
	m_fFogDistance = powf(10, (DIST_MIN + m_iFogDistance * DIST_RANGE / 100));
}

void EphemDlg::SetSliderControls()
{
	FindWindow(ID_SLIDER_WIND_DIRECTION)->GetValidator()->TransferToWindow();
	FindWindow(ID_SLIDER_WIND_SPEED)->GetValidator()->TransferToWindow();
	FindWindow(ID_SLIDER_FOG_DISTANCE)->GetValidator()->TransferToWindow();
}

void EphemDlg::UpdateColorControl()
{
	FillWithColor(GetColorBitmap(), m_BgColor);
}

void EphemDlg::SetToScene()
{
	vtTerrainScene *ts = vtGetTS();
	vtTerrain *terr = GetCurrentTerrain();
	TParams &param = terr->GetParams();
	vtSkyDome *sky = ts->GetSkyDome();

	param.SetValueBool(STR_SKY, m_bSky);
	param.SetValueString(STR_SKYTEXTURE, (const char *) m_strSkyTexture.mb_str(wxConvUTF8));
	ts->UpdateSkydomeForTerrain(terr);
	terr->SetFeatureVisible(TFT_OCEAN, m_bOceanPlane);
	terr->SetWaterLevel(m_fOceanPlaneLevel);
	terr->SetFeatureVisible(TFT_HORIZON, m_bHorizon);
	terr->SetFog(m_bFog);
	terr->SetFogDistance(m_fFogDistance);
	RGBi col(m_BgColor.Red(), m_BgColor.Green(), m_BgColor.Blue());
	terr->SetBgColor(col);
	vtGetScene()->SetBgColor(col);
	g_App.SetWind(m_iWindDir, m_fWindSpeed);
}

// WDR: handler implementations for EphemDlg

void EphemDlg::OnInitDialog(wxInitDialogEvent& event)
{
	VTLOG("EphemDlg::OnInitDialog\n");
	m_bSetting = true;

	vtStringArray &paths = vtGetDataPath();
	unsigned int i;
	int sel;

	for (i = 0; i < paths.size(); i++)
	{
		// fill in Sky files
		AddFilenamesToComboBox(GetSkyTexture(), paths[i] + "Sky", "*.bmp");
		AddFilenamesToComboBox(GetSkyTexture(), paths[i] + "Sky", "*.png");
		AddFilenamesToComboBox(GetSkyTexture(), paths[i] + "Sky", "*.jpg");
		sel = GetSkyTexture()->FindString(m_strSkyTexture);
		if (sel != -1)
			GetSkyTexture()->SetSelection(sel);
	}
	UpdateColorControl();
	wxWindow::OnInitDialog(event);
	UpdateEnableState();
	m_bSetting = false;
}

void EphemDlg::OnSkyTexture( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	SetToScene();
}

void EphemDlg::OnSliderFogDistance( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	SlidersToValues();
	SetToScene();
	m_bSetting = true;
	TransferDataToWindow();
	m_bSetting = false;
}

void EphemDlg::OnSliderWindSpeed( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	SlidersToValues();
	g_App.SetWind(m_iWindDir, m_fWindSpeed);
	m_bSetting = true;
	TransferDataToWindow();
	m_bSetting = false;
}

void EphemDlg::OnSliderWindDirection( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	SlidersToValues();
	g_App.SetWind(m_iWindDir, m_fWindSpeed);
	m_bSetting = true;
	TransferDataToWindow();
	m_bSetting = false;
}

void EphemDlg::OnWindSpeed( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	ValuesToSliders();
	g_App.SetWind(m_iWindDir, m_fWindSpeed);
	SetSliderControls();
}

void EphemDlg::OnWindDirection( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	ValuesToSliders();
	g_App.SetWind(m_iWindDir, m_fWindSpeed);
	SetSliderControls();
}

void EphemDlg::OnFogDistance( wxCommandEvent &event )
{
	if (m_bSetting)
		return;
	TransferDataFromWindow();
	ValuesToSliders();
	SetToScene();
	SetSliderControls();
}

void EphemDlg::OnBgColor( wxCommandEvent &event )
{
	wxColourData data;
	data.SetChooseFull(true);
	data.SetColour(m_BgColor);

	wxColourDialog dlg(this, &data);
	if (dlg.ShowModal() == wxID_OK)
	{
		wxColourData data2 = dlg.GetColourData();
		m_BgColor = data2.GetColour();
		UpdateColorControl();
		SetToScene();
	}
}

void EphemDlg::OnCheckBox( wxCommandEvent &event )
{
	TransferDataFromWindow();
	UpdateEnableState();
	SetToScene();
}



