/////////////////////////////////////////////////////////////////////////////
// Name:        RawDlg.h
// Author:      XX
// Created:     XX/XX/XX
// Copyright:   XX
/////////////////////////////////////////////////////////////////////////////

#ifndef __RawDlg_H__
#define __RawDlg_H__

#ifdef __GNUG__
    #pragma interface "RawDlg.cpp"
#endif

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "VTBuilder_wdr.h"
#include "AutoDialog.h"

// WDR: class declarations

//----------------------------------------------------------------------------
// RawDlg
//----------------------------------------------------------------------------

class RawDlg: public AutoDialog
{
public:
    // constructors and destructors
    RawDlg( wxWindow *parent, wxWindowID id, const wxString &title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE );
    
    // WDR: method declarations for RawDlg
	void OnInitDialog(wxInitDialogEvent& event);

	int m_iBytes;
	int m_iWidth;
	int m_iHeight;
	bool m_bUTM;
	bool m_bFloating;
	float m_fVUnits;
	float m_fSpacing;
    
private:
    // WDR: member variable declarations for RawDlg
    
private:
    // WDR: handler declarations for RawDlg

private:
    DECLARE_EVENT_TABLE()
};




#endif