//
// Name:		Projection2Dlg.h
//
// Copyright (c) 2002-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef __Projection2Dlg_H__
#define __Projection2Dlg_H__

#if defined(__GNUG__) && !defined(__APPLE__)
	#pragma interface "Projection2Dlg.cpp"
#endif

#include "VTBuilder_wdr.h"
#include "vtui/AutoDialog.h"
#include "vtdata/Projections.h"

#ifndef __ProjectionDlg_H__
enum ProjType
{
	PT_ALBERS,
	PT_GEO,
	PT_LAMBERT,
	PT_NZMG,
	PT_OS,
	PT_PS,
	PT_SINUS,
	PT_STEREO,
	PT_TM,
	PT_UTM
};
#endif

// WDR: class declarations

//----------------------------------------------------------------------------
// Projection2Dlg
//----------------------------------------------------------------------------

class Projection2Dlg: public AutoDialog
{
public:
	// constructors and destructors
	Projection2Dlg( wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE );
	
	// WDR: method declarations for Projection2Dlg
	wxListCtrl* GetProjparam()  { return (wxListCtrl*) FindWindow( ID_PROJPARAM ); }
	wxChoice* GetHorizchoice()  { return (wxChoice*) FindWindow( ID_HORUNITS ); }
	wxChoice* GetZonechoice()  { return (wxChoice*) FindWindow( ID_ZONE ); }
	wxChoice* GetDatumchoice()  { return (wxChoice*) FindWindow( ID_DATUM ); }
	wxChoice* GetProjchoice()  { return (wxChoice*) FindWindow( ID_PROJ ); }
	void SetProjection(const vtProjection &proj);
	void GetProjection(vtProjection &proj);
	void SetUIFromProjection();
	void SetProjectionUI(ProjType type);
	void UpdateControlStatus();
	void DisplayProjectionSpecificParams();
	void AskStatePlane();
	void RefreshDatums();
	void UpdateDatumStatus();

	int	  m_iDatum;
	int	  m_iZone;
	vtProjection	m_proj;
	bool	m_bShowAllDatums;

private:
	// WDR: member variable declarations for Projection2Dlg
	ProjType	m_eProj;
	int	  m_iProj;
	int	  m_iUnits;
	wxListCtrl  *m_pParamCtrl;
	wxChoice	*m_pZoneCtrl;
	wxChoice	*m_pHorizCtrl;
	wxChoice	*m_pDatumCtrl;
	wxChoice	*m_pProjCtrl;

	bool m_bInitializedUI;

private:
	// WDR: handler declarations for Projection2Dlg
	void OnProjSave( wxCommandEvent &event );
	void OnProjLoad( wxCommandEvent &event );
	void OnDatum( wxCommandEvent &event );
	void OnItemRightClick( wxListEvent &event );
	void OnHorizUnits( wxCommandEvent &event );
	void OnZone( wxCommandEvent &event );
	void OnSetStatePlane( wxCommandEvent &event );
	void OnProjChoice( wxCommandEvent &event );
	void OnInitDialog(wxInitDialogEvent& event);
	void OnShowAllDatums( wxCommandEvent &event );

private:
	DECLARE_EVENT_TABLE()
};


#endif

