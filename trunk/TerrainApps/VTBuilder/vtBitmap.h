//
// vtBitmap.h
//
// Copyright (c) 2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef VTBITMAP_H
#define VTBITMAP_H

#include "wxBitmapSection.h"
#include "vtdata/MathTypes.h"

/**
 * This class provides an encapsulation of "bitmap" behavior, which can
 * either use the Win32 DIBSection methods, or the wxWindows Bitmap methods.
 *
 * Set USE_DIBSECTIONS to 1 to get the DIBSection functionality.
 */
class vtBitmap
{
public:
	vtBitmap();
	~vtBitmap();

	bool Allocate(int iXSize, int iYSize);
	void SetRGB(int x, int y, unsigned char r, unsigned char g, unsigned char b);
	void SetRGB(int x, int y, const RGBi &rgb)
	{
		SetRGB(x, y, rgb.r, rgb.g, rgb.b);
	}
	void GetRGB(int x, int y, RGBi &rgb);
	void ContentsChanged();

	wxBitmap	*m_pBitmap;

protected:

#if USE_DIBSECTIONS
	// A DIBSection is a special kind of bitmap, handled as a HBITMAP,
	//  created with special methods, and accessed as a giant raw
	//  memory array.
	unsigned char *m_pScanline;
	int m_iScanlineWidth;
#else
	// For non-Windows platforms, or Windows platforms if we're being more
	//  cautious, the Bitmap is device-dependent and therefore can't be
	//  relied upon to store data the way we expect.  Hence, we must have
	//  both a wxImage (portable and easy to use, but can't be directly
	//  rendered) and a wxBitmap (which can be drawn to the window).
	//
	// This is less memory efficient and slower.
	//
	wxImage		*m_pImage;
#endif
};

#endif // VTBITMAP_H

