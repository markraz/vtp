/////////////////////////////////////////////////////////////////////////////
// Name:        RawDlg.cpp
// Author:      XX
// Created:     XX/XX/XX
// Copyright:   XX
/////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
    #pragma implementation "RawDlg.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "RawDlg.h"

// WDR: class implementations

//----------------------------------------------------------------------------
// RawDlg
//----------------------------------------------------------------------------

// WDR: event table for RawDlg

BEGIN_EVENT_TABLE(RawDlg,AutoDialog)
END_EVENT_TABLE()

RawDlg::RawDlg( wxWindow *parent, wxWindowID id, const wxString &title,
        const wxPoint& pos, const wxSize& size, long style ) :
	AutoDialog(parent, id, title, pos, size, style)
{
    RawDialogFunc( this, TRUE );
}

// WDR: handler implementations for RawDlg

void RawDlg::OnInitDialog(wxInitDialogEvent& event)
{
	AddNumValidator(ID_BYTES, &m_iBytes);
	AddNumValidator(ID_WIDTH, &m_iWidth);
	AddNumValidator(ID_HEIGHT, &m_iHeight);
	AddValidator(ID_UTM, &m_bUTM);
	AddValidator(ID_FLOATING, &m_bFloating);
	AddNumValidator(ID_VUNITS, &m_fVUnits);
	AddNumValidator(ID_SPACING, &m_fSpacing);

    wxDialog::OnInitDialog(event);  // calls TransferDataToWindow()
}

