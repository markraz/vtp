//
// Name:		StyleDlg.cpp
//
// Copyright (c) 2004-2006 Virtual Terrain Project.
// Free for all uses, see license.txt for details.
//

#ifdef __GNUG__
	#pragma implementation "StyleDlg.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "wx/colordlg.h"

#include "StyleDlg.h"
#include "vtlib/vtlib.h"
#include "vtlib/core/TerrainScene.h"
#include "vtdata/Features.h"
#include "vtui/Helper.h"

// WDR: class implementations

//----------------------------------------------------------------------------
// StyleDlg
//----------------------------------------------------------------------------

// WDR: event table for StyleDlg

BEGIN_EVENT_TABLE(StyleDlg,AutoDialog)
	EVT_INIT_DIALOG (StyleDlg::OnInitDialog)
	EVT_BUTTON( ID_GEOM_COLOR, StyleDlg::OnGeomColor )
	EVT_BUTTON( ID_LABEL_COLOR, StyleDlg::OnLabelColor )
	EVT_CHECKBOX( ID_GEOMETRY, StyleDlg::OnCheck )
	EVT_CHECKBOX( ID_TEXT_LABELS, StyleDlg::OnCheck )
END_EVENT_TABLE()

StyleDlg::StyleDlg( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	AutoDialog( parent, id, title, position, size, style )
{
	// WDR: dialog function StyleDialogFunc for StyleDlg
	StyleDialogFunc( this, TRUE );

	m_pFeatureSet = NULL;
	m_bGeometry = true;
	m_GeomColor.Set(255,255,255);

	m_bTextLabels = false;
	m_LabelColor.Set(255,255,255);
	m_iTextField = 0;
	m_iColorField = 0;
	m_fLabelHeight = 0.0f;
	m_fLabelSize = 0.0f;

	AddValidator(ID_FEATURE_TYPE, &m_strFeatureType);

	AddValidator(ID_GEOMETRY, &m_bGeometry);
	AddNumValidator(ID_GEOM_HEIGHT, &m_fGeomHeight);
	AddNumValidator(ID_LINE_WIDTH, &m_fLineWidth);
	AddValidator(ID_TESSELLATE, &m_bTessellate);

	AddValidator(ID_TEXT_LABELS, &m_bTextLabels);
	AddValidator(ID_TEXT_FIELD, &m_iTextField);
	AddValidator(ID_COLOR_FIELD, &m_iColorField);
	AddNumValidator(ID_LABEL_HEIGHT, &m_fLabelHeight);
	AddNumValidator(ID_LABEL_SIZE, &m_fLabelSize);
}

void StyleDlg::OnInitDialog(wxInitDialogEvent& event)
{
	RefreshFields();
	UpdateEnabling();
	UpdateColorButtons();
	wxDialog::OnInitDialog(event);
}

void StyleDlg::SetOptions(const vtStringArray &datapaths, const vtTagArray &Layer)
{
	if (m_pFeatureSet)
	{
		m_type = m_pFeatureSet->GetGeomType();
	}
	else
	{
		// without a featureset, we need the actual file location
		vtString strFilename = Layer.GetValueString("Filename");
		m_strResolved = strFilename;
		m_strResolved = FindFileOnPaths(datapaths, m_strResolved);
		if (m_strResolved == "")
		{
			vtString path = "PointData/";
			m_strResolved = path + strFilename;
			m_strResolved = FindFileOnPaths(datapaths, m_strResolved);
		}

		m_type = GetFeatureGeomType(m_strResolved);
		if (m_DummyFeatures.LoadFieldInfoFromDBF(m_strResolved))
			m_pFeatureSet = &m_DummyFeatures;
	}

	m_bGeometry = Layer.GetValueBool("Geometry");
	if (!Layer.GetValueRGBi("GeomColor", m_GeomColor))
		m_GeomColor.Set(255,255,255);
	if (!Layer.GetValueFloat("GeomHeight", m_fGeomHeight))
		m_fGeomHeight = 1;
	if (!Layer.GetValueFloat("LineWidth", m_fLineWidth))
		m_fLineWidth = 1;
	m_bTessellate = Layer.GetValueBool("Tessellate");

	m_bTextLabels = Layer.GetValueBool("Labels");
	if (!Layer.GetValueRGBi("LabelColor", m_LabelColor))
		m_LabelColor.Set(255,255,255);

	if (!Layer.GetValueInt("TextFieldIndex", m_iTextField))
		m_iTextField = -1;
	if (!Layer.GetValueInt("ColorFieldIndex", m_iColorField))
		m_iColorField = -1;

	if (!Layer.GetValueFloat("Elevation", m_fLabelHeight))
		m_fLabelHeight= 0;
	if (!Layer.GetValueFloat("LabelSize", m_fLabelSize))
		m_fLabelSize = 20;

}

void StyleDlg::GetOptions(vtTagArray &pLayer)
{
	pLayer.SetValueBool("Geometry", m_bGeometry, true);
	if (m_bGeometry)
	{
		pLayer.SetValueRGBi("GeomColor", m_GeomColor, true);
		if (!GeometryTypeIs3D(m_type))
			pLayer.SetValueFloat("GeomHeight", m_fGeomHeight);
		pLayer.SetValueFloat("LineWidth", m_fLineWidth);
	}
	else
	{
		pLayer.RemoveTag("GeomColor");
		pLayer.RemoveTag("GeomHeight");
		pLayer.RemoveTag("LineWidth");
	}

	if (m_bGeometry && m_type != wkbPoint && m_type != wkbPoint25D)
		pLayer.SetValueBool("Tessellate", m_bTessellate);
	else
		pLayer.RemoveTag("Tessellate");

	pLayer.SetValueBool("Labels", m_bTextLabels, true);
	if (m_bTextLabels)
	{
		pLayer.SetValueRGBi("LabelColor", m_LabelColor, true);
		pLayer.SetValueInt("TextFieldIndex", m_iTextField, true);
		pLayer.SetValueInt("ColorFieldIndex", m_iColorField, true);
		pLayer.SetValueFloat("Elevation", m_fLabelHeight, true);
		pLayer.SetValueFloat("LabelSize", m_fLabelSize, true);
	}
	else
	{
		pLayer.RemoveTag("LabelColor");
		pLayer.RemoveTag("TextFieldIndex");
		pLayer.RemoveTag("ColorFieldIndex");
		pLayer.RemoveTag("Elevation");
		pLayer.RemoveTag("LabelSize");
	}
}

void StyleDlg::RefreshFields()
{
	GetTextField()->Clear();
	GetColorField()->Clear();
	GetColorField()->Append(_("(none)"));

	m_strFeatureType = wxString(OGRGeometryTypeToName(m_type), wxConvUTF8);
	int i, num = m_pFeatureSet->GetNumFields();
	for (i = 0; i < num; i++)
	{
		const Field *field = m_pFeatureSet->GetField(i);
		wxString field_name(field->m_name, wxConvUTF8);
		GetTextField()->Append(field_name);
		GetColorField()->Append(field_name);
	}
	if (num)
	{
		if (m_iTextField < 0)
			m_iTextField = 0;
		if (m_iTextField > num-1)
			m_iTextField = num-1;
		if (m_iColorField < 0)
			m_iColorField = 0;
		if (m_iColorField > num-1)
			m_iColorField = num-1;
	}
}

void StyleDlg::UpdateEnabling()
{
	GetGeomColor()->Enable(m_bGeometry);
	GetGeomHeight()->Enable(m_bGeometry && !GeometryTypeIs3D(m_type));
	GetLineWidth()->Enable(m_bGeometry);
	GetTessellate()->Enable(m_bGeometry && m_type != wkbPoint && m_type != wkbPoint25D);

	GetLabelColor()->Enable(m_bTextLabels);
	GetTextField()->Enable(m_bTextLabels);
	GetColorField()->Enable(m_bTextLabels && m_pFeatureSet->GetNumFields() > 1);
	GetLabelSize()->Enable(m_bTextLabels);
	GetLabelHeight()->Enable(m_bTextLabels);
}

void StyleDlg::UpdateColorButtons()
{
	FillWithColor(GetGeomColor(), m_GeomColor);
	FillWithColor(GetLabelColor(), m_LabelColor);
}

RGBi StyleDlg::AskColor(const RGBi &input)
{
	m_Colour.Set(input.r, input.g, input.b);
	m_ColourData.SetChooseFull(true);
	m_ColourData.SetColour(m_Colour);

	wxColourDialog dlg(this, &m_ColourData);
	if (dlg.ShowModal() == wxID_OK)
	{
		m_ColourData = dlg.GetColourData();
		m_Colour = m_ColourData.GetColour();
		return RGBi(m_Colour.Red(), m_Colour.Green(), m_Colour.Blue());
	}
	else
		return RGBi(-1,-1,-1);
}

// WDR: handler implementations for StyleDlg

void StyleDlg::OnCheck( wxCommandEvent &event )
{
	TransferDataFromWindow();
	UpdateEnabling();
}

void StyleDlg::OnGeomColor( wxCommandEvent &event )
{
	RGBi result = AskColor(m_GeomColor);
	if (result.r == -1)
		return;
	m_GeomColor = result;
	UpdateColorButtons();
}

void StyleDlg::OnLabelColor( wxCommandEvent &event )
{
	RGBi result = AskColor(m_LabelColor);
	if (result.r == -1)
		return;
	m_LabelColor = result;
	UpdateColorButtons();
}

