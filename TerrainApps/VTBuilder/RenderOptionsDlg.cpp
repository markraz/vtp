//
// Name: RenderOptionsDlg.cpp
//
// Copyright (c) 2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "RenderOptionsDlg.h"
#include "vtui/Helper.h"		// for AddFilenamesToChoice
#include "vtui/ColorMapDlg.h"
#include "vtdata/FilePath.h"

// WDR: class implementations

//----------------------------------------------------------------------------
// RenderOptionsDlg
//----------------------------------------------------------------------------

// WDR: event table for RenderOptionsDlg

BEGIN_EVENT_TABLE(RenderOptionsDlg,wxDialog)
	EVT_INIT_DIALOG (RenderOptionsDlg::OnInitDialog)
	EVT_BUTTON( ID_EDIT_COLORS, RenderOptionsDlg::OnEditColors )
	EVT_RADIOBUTTON( ID_RADIO_SHADING_NONE, RenderOptionsDlg::OnRadio )
	EVT_RADIOBUTTON( ID_RADIO_SHADING_QUICK, RenderOptionsDlg::OnRadio )
	EVT_RADIOBUTTON( ID_RADIO_SHADING_DOT, RenderOptionsDlg::OnRadio )
	EVT_CHOICE( ID_CHOICE_COLORS, RenderOptionsDlg::OnChoiceColors )
	EVT_CHECKBOX( ID_CHECK_SHADOWS, RenderOptionsDlg::OnRadio )
END_EVENT_TABLE()

RenderOptionsDlg::RenderOptionsDlg( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	AutoDialog( parent, id, title, position, size, style )
{
	// WDR: dialog function RenderOptionsDialogFunc for RenderOptionsDlg
	RenderOptionsDialogFunc( this, TRUE ); 

	m_opt.m_bShowElevation = true;
	m_bNoShading = false;
	m_opt.m_bShadingQuick = true;
	m_opt.m_bShadingDot = false;
	m_opt.m_bCastShadows = false;

	AddValidator(ID_CHOICE_COLORS, &m_strColorMap);

	AddValidator(ID_RADIO_SHADING_NONE, &m_bNoShading);
	AddValidator(ID_RADIO_SHADING_QUICK, &m_opt.m_bShadingQuick);
	AddValidator(ID_RADIO_SHADING_DOT, &m_opt.m_bShadingDot);
	AddValidator(ID_CHECK_SHADOWS, &m_opt.m_bCastShadows);

	AddValidator(ID_SPIN_CAST_ANGLE, &m_opt.m_iCastAngle);
	AddValidator(ID_SPIN_CAST_DIRECTION, &m_opt.m_iCastDirection);
}

void RenderOptionsDlg::SetOptions(ElevDrawOptions &opt)
{
	m_opt = opt;
	m_bNoShading = !(opt.m_bShadingQuick || opt.m_bShadingDot);
	UpdateEnables();
	TransferDataToWindow();
}

void RenderOptionsDlg::UpdateEnables()
{
	// must have dot-produce shading to add cast shadows
	if (!m_opt.m_bShadingDot)
		m_opt.m_bCastShadows = false;

	GetSpinCastAngle()->Enable(m_opt.m_bShadingDot);
	GetSpinCastDirection()->Enable(m_opt.m_bShadingDot);
	GetCastShadows()->Enable(m_opt.m_bShadingDot);
}

void RenderOptionsDlg::UpdateColorMapChoice()
{
	GetChoiceColors()->Clear();
	for (unsigned int i = 0; i < m_datapaths.size(); i++)
	{
		// fill the "colormap" control with available colormap files
		AddFilenamesToChoice(GetChoiceColors(), m_datapaths[i] + "GeoTypical", "*.cmt");
		int sel = GetChoiceColors()->FindString(m_strColorMap);
		if (sel != -1)
			GetChoiceColors()->SetSelection(sel);
		else
			GetChoiceColors()->SetSelection(0);
	}
}

// WDR: handler implementations for RenderOptionsDlg

void RenderOptionsDlg::OnChoiceColors( wxCommandEvent &event )
{
	TransferDataFromWindow();
	m_opt.m_strColorMapFile = m_strColorMap.mb_str(wxConvUTF8);
}

void RenderOptionsDlg::OnInitDialog(wxInitDialogEvent& event)
{
	wxDialog::OnInitDialog(event);

	UpdateColorMapChoice();
	TransferDataFromWindow();
	m_opt.m_strColorMapFile = m_strColorMap.mb_str(wxConvUTF8);
}

void RenderOptionsDlg::OnRadio( wxCommandEvent &event )
{
	TransferDataFromWindow();
	UpdateEnables();
}

void RenderOptionsDlg::OnEditColors( wxCommandEvent &event )
{
	TransferDataFromWindow();

	ColorMapDlg dlg(this, -1, _("ColorMap"));

	// Look on data paths, to give a complete path to the dialog
	if (m_strColorMap != _T(""))
	{
		vtString name = "GeoTypical/";
		name += m_strColorMap.mb_str(wxConvUTF8);
		name = FindFileOnPaths(m_datapaths, name);
		if (name == "")
		{
			wxMessageBox(_("Couldn't locate file."));
			return;
		}
		dlg.SetFile(name);
	}
	dlg.ShowModal();

	// They may have added or removed some color map files on the data path
	UpdateColorMapChoice();
}



