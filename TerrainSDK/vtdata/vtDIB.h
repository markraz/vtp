//
// vtDIB.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef VTDATA_DIBH
#define VTDATA_DIBH

#include "MathTypes.h"

// for non-Win32 systems (or code which doesn't include the Win32 headers),
// define some Microsoft types used by the DIB code
#ifdef _WINGDI_
	typedef DWORD dword;
#else
	typedef unsigned long dword;
	typedef unsigned short word;
	typedef unsigned char byte;
	typedef struct tagBITMAPINFOHEADER BITMAPINFOHEADER;
	#define RGB(r,g,b)		  ((dword)(((byte)(r)|((word)((byte)(g))<<8))|(((dword)(byte)(b))<<16)))
	#define GetRValue(rgb)	  ((byte)(rgb))
	#define GetGValue(rgb)	  ((byte)(((word)(rgb)) >> 8))
	#define GetBValue(rgb)	  ((byte)((rgb)>>16))
#endif

/**
 * A DIB is a Device-Independent Bitmap.  It is a way of representing a
 * bitmap in memory which has its origins in early MS Windows usage, but
 * is entirely applicable to normal bitmap operations.
 */
class vtDIB
{
public:
	vtDIB();
	vtDIB(void *pDIB);	// wrap an existing DIB
	~vtDIB();

	bool Create(int width, int height, int bitdepth, bool create_palette = false);
	bool Create24From8bit(const vtDIB &from);

	bool Read(const char *fname);
	bool ReadBMP(const char *fname);
	bool WriteBMP(const char *fname);
	bool ReadJPEG(const char *fname);
	bool WriteJPEG(const char *fname, int quality);
	bool ReadPNG(const char *fname);
	bool WritePNG(const char *fname);
	bool WriteTIF(const char *fname);

	unsigned int GetPixel24(int x, int y) const;
	void GetPixel24(int x, int y, RGBi &rgb) const;
	void GetPixel24From8bit(int x, int y, RGBi &rgb) const;
	void SetPixel24(int x, int y, dword color);
	void SetPixel24(int x, int y, const RGBi &rgb);

	void GetPixel32(int x, int y, RGBAi &rgba) const;
	void SetPixel32(int x, int y, const RGBAi &rgba);

	unsigned char GetPixel8(int x, int y) const;
	void SetPixel8(int x, int y, byte color);

	bool GetPixel1(int x, int y) const;
	void SetPixel1(int x, int y, bool color);

	void SetColor(const RGBi &rgb);

	unsigned int GetWidth() const { return m_iWidth; }
	unsigned int GetHeight() const { return m_iHeight; }
	unsigned int GetDepth() const { return m_iBitCount; }

	void *GetHandle() const { return m_pDIB; }
	BITMAPINFOHEADER *GetDIBHeader() const { return m_Hdr; }
	void *GetDIBData() const { return m_Data; }

	void Lock();
	void Unlock();
	void LeaveInternalDIB(bool bLeaveIt);

	bool	m_bLoadedSuccessfully;

private:
	void _ComputeByteWidth();

	bool	m_bLeaveIt;

	// When locked, these two fields point to the header and data
	BITMAPINFOHEADER *m_Hdr;
	void	*m_Data;

	void	*m_pDIB;
	unsigned int	m_iWidth, m_iHeight, m_iBitCount, m_iByteCount;
	unsigned int	m_iByteWidth;
	unsigned int	m_iPaletteSize;
};

#endif	// VTDATA_DIBH

