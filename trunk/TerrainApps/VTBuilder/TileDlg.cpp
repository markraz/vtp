//
// Name: TileDlg.cpp
//
// Copyright (c) 2005 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "TileDlg.h"
#include "FileFilters.h"	// for FSTRING_INI
#include "BuilderView.h"

// WDR: class implementations

//----------------------------------------------------------------------------
// TileDlg
//----------------------------------------------------------------------------

// WDR: event table for TileDlg

BEGIN_EVENT_TABLE(TileDlg,AutoDialog)
	EVT_BUTTON( ID_DOTDOTDOT, TileDlg::OnDotDotDot )
	EVT_TEXT( ID_COLUMNS, TileDlg::OnSize )
	EVT_TEXT( ID_ROWS, TileDlg::OnSize )
	EVT_TEXT( ID_TEXT_TO_FOLDER, TileDlg::OnFilename )
	EVT_CHOICE( ID_CHOICE_LOD0_SIZE, TileDlg::OnLODSize )
END_EVENT_TABLE()

TileDlg::TileDlg( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	AutoDialog( parent, id, title, position, size, style )
{
	// WDR: dialog function TileDialogFunc for TileDlg
	TileDialogFunc( this, TRUE );

	m_fEstX = -1;
	m_fEstY = -1;
	m_iColumns = 1;
	m_iRows = 1;

	m_bSetting = false;
	m_pView = NULL;

	AddValidator(ID_TEXT_TO_FOLDER, &m_strToFile);
	AddNumValidator(ID_COLUMNS, &m_iColumns);
	AddNumValidator(ID_ROWS, &m_iRows);
	AddValidator(ID_CHOICE_LOD0_SIZE, &m_iLODChoice);
	AddValidator(ID_SPIN_NUM_LODS, &m_iNumLODs);

	// informations
	AddNumValidator(ID_TOTALX, &m_iTotalX);
	AddNumValidator(ID_TOTALY, &m_iTotalY);

	AddNumValidator(ID_AREAX, &m_fAreaX);
	AddNumValidator(ID_AREAY, &m_fAreaY);

	AddNumValidator(ID_ESTX, &m_fEstX);
	AddNumValidator(ID_ESTY, &m_fEstY);

	AddNumValidator(ID_CURX, &m_fCurX);
	AddNumValidator(ID_CURY, &m_fCurY);

	UpdateEnables();
}

void TileDlg::SetElevation(bool bElev)
{
	// Elevation is handled as grid corners, not center, so grid size are differnt
	m_bElev = bElev;

	GetChoiceLod0Size()->Clear();
	if (m_bElev)
	{
		GetChoiceLod0Size()->Append(_T("32 + 1"));
		GetChoiceLod0Size()->Append(_T("64 + 1"));
		GetChoiceLod0Size()->Append(_T("128 + 1"));
		GetChoiceLod0Size()->Append(_T("256 + 1"));
		GetChoiceLod0Size()->Append(_T("512 + 1"));
		GetChoiceLod0Size()->Append(_T("1024 + 1"));
		GetChoiceLod0Size()->Append(_T("2048 + 1"));
		GetChoiceLod0Size()->Append(_T("4096 + 1"));
	}
	else
	{
		GetChoiceLod0Size()->Append(_T("32"));
		GetChoiceLod0Size()->Append(_T("64"));
		GetChoiceLod0Size()->Append(_T("128"));
		GetChoiceLod0Size()->Append(_T("256"));
		GetChoiceLod0Size()->Append(_T("512"));
		GetChoiceLod0Size()->Append(_T("1024"));
		GetChoiceLod0Size()->Append(_T("2048"));
		GetChoiceLod0Size()->Append(_T("4096"));
	}
}

void TileDlg::SetTilingOptions(TilingOptions &opt)
{
	m_iColumns = opt.cols;
	m_iRows = opt.rows;
	m_iLOD0Size = opt.lod0size;
	m_iNumLODs = opt.numlods;
	m_strToFile = opt.fname;

	m_iLODChoice = vt_log2(m_iLOD0Size)-5;

	UpdateInfo();
}

void TileDlg::GetTilingOptions(TilingOptions &opt) const
{
	opt.cols = m_iColumns;
	opt.rows = m_iRows;
	opt.lod0size = m_iLOD0Size;
	opt.numlods = m_iNumLODs;
	opt.fname = m_strToFile.mb_str();
}

void TileDlg::SetArea(const DRECT &area)
{
	m_area = area;

	UpdateInfo();
}

void TileDlg::UpdateInfo()
{
	m_iTotalX = m_iLOD0Size * m_iColumns;
	m_iTotalY = m_iLOD0Size * m_iRows;
	if (m_bElev)
	{
		// Elevation is handled as grid corners, imagery is handled as
		//  centers, so grid sizes are differnt
		m_iTotalX ++;
		m_iTotalY ++;
	}

	m_fAreaX = m_area.Width();
	m_fAreaY = m_area.Height();

	if (m_bElev)
	{
		m_fCurX = m_fAreaX / (m_iTotalX - 1);
		m_fCurY = m_fAreaY / (m_iTotalY - 1);
	}
	else
	{
		m_fCurX = m_fAreaX / m_iTotalX;
		m_fCurY = m_fAreaY / m_iTotalY;
	}

	m_bSetting = true;
	TransferDataToWindow();
	m_bSetting = false;
}

void TileDlg::UpdateEnables()
{
	FindWindow(wxID_OK)->Enable(m_strToFile != _T(""));
}

// WDR: handler implementations for TileDlg

void TileDlg::OnFilename( wxCommandEvent &event )
{
	if (m_bSetting)
		return;

	TransferDataFromWindow();
	UpdateEnables();
}

void TileDlg::OnLODSize( wxCommandEvent &event )
{
	if (m_bSetting)
		return;

	TransferDataFromWindow();
	m_iLOD0Size = 1 << (m_iLODChoice + 5);
	UpdateInfo();
}

void TileDlg::OnSize( wxCommandEvent &event )
{
	if (m_bSetting)
		return;

	TransferDataFromWindow();
	UpdateInfo();

	if (m_pView)
		m_pView->ShowGridMarks(m_area, m_iColumns, m_iRows, -1, -1);
}

void TileDlg::OnDotDotDot( wxCommandEvent &event )
{
	// ask the user for a filename
	wxString filter;
	filter += FSTRING_INI;
	wxFileDialog saveFile(NULL, _T(".Ini file"), _T(""), _T(""), filter, wxSAVE);
	bool bResult = (saveFile.ShowModal() == wxID_OK);
	if (!bResult)
		return;

	// update controls
	m_strToFile = saveFile.GetPath();

	TransferDataToWindow();
	UpdateEnables();
}

