//
// Helper.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef HELPERH
#define HELPERH

#include "vtdata/Projections.h"
#include "vtdata/MathTypes.h"
#include "vtdata/Minidatabuf.h"

/////

void IncreaseRect(wxRect &rect, int adjust);
void DrawRectangle(wxDC* pDC, const wxRect &rect);

/////

void AddType(wxString &str, const wxString &filter);

/////

vtStringArray &GetDataPaths();

/////

// Display a message to the user, and also send it to the log file.
void DisplayAndLog(const char *pFormat, ...);
#if SUPPORT_WSTRING
void DisplayAndLog(const wchar_t *pFormat, ...);
#endif

/////

#if WIN32
/**
 * Win32 allows us to do a real StrectBlt operation, although it still won't
 * do a StrectBlt with a mask.
 */
class wxDC2 : public wxDC
{
public:
	void StretchBlit(const wxBitmap &bmp, wxCoord x, wxCoord y,
		wxCoord width, wxCoord height, wxCoord src_x, wxCoord src_y,
		wxCoord src_width, wxCoord src_height);
};
#endif	// WIN32

/////////////////

void DoTextureCompress(unsigned char *rgb_bytes, MiniDatabuf &output_buf,
					   unsigned int &iTex);

#endif	// HELPERH

