//
// Name:		LinearStructureDlg3d.h
//
// Copyright (c) 2001-2002 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef __LinearStructureDlg3d_H__
#define __LinearStructureDlg3d_H__

#ifdef __GNUG__
	#pragma interface "LinearStructureDlg3d.cpp"
#endif

#include "vtui/LinearStructDlg.h"

//----------------------------------------------------------------------------
// FenceDlg
//----------------------------------------------------------------------------

class LinearStructureDlg3d: public LinearStructureDlg
{
public:
	// constructors and destructors
	LinearStructureDlg3d( wxWindow *parent, wxWindowID id, const wxString &title,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE );

	virtual void OnSetOptions(LinStructOptions &opt);
};

#endif
