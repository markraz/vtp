//
// Name:		LinearStructureDlg3d.cpp
//
// Copyright (c) 2001-2002 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifdef __GNUG__
	#pragma implementation "LinearStructureDlg3d.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
#include "wx/image.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "vtlib/vtlib.h"
#include "LinearStructDlg3d.h"
#include "../Enviro.h"

// WDR: class implementations

//----------------------------------------------------------------------------
// LinearStructureDlg
//----------------------------------------------------------------------------

LinearStructureDlg3d::LinearStructureDlg3d( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	LinearStructureDlg( parent, id, title, position, size, style )
{
}

void LinearStructureDlg3d::OnSetOptions(LinStructOptions &opt)
{
	g_App.SetFenceOptions(opt.eType, opt.fHeight, opt.fSpacing);
}

