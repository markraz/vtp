//
// ImageLayer.h
//
// Copyright (c) 2002-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include "wx/image.h"
#include "Layer.h"

class vtBitmap;
class GDALDataset;
class GDALRasterBand;
class GDALColorTable;
class BuilderView;
class ImageGLCanvas;

// The following mechanism is for a small buffer, consisting of a small
//  number of scanlines, to cache the results of accessing large image
//  files out of memory (direct from disk).
struct Scanline
{
	RGBi *m_data;
	int m_y;
};
#define BUF_SCANLINES	4

//////////////////////////////////////////////////////////

class vtImageLayer : public vtLayer
{
public:
	vtImageLayer();
	vtImageLayer(const DRECT &area, int xsize, int ysize,
		const vtProjection &proj);
	virtual ~vtImageLayer();

	// overrides for vtLayer methods
	bool GetExtent(DRECT &rect);
	void DrawLayer(wxDC* pDC, vtScaledView *pView);
	bool TransformCoords(vtProjection &proj);
	bool OnSave();
	bool OnLoad();
	bool AppendDataFrom(vtLayer *pL);
	void GetProjection(vtProjection &proj);
	void SetProjection(const vtProjection &proj);
	void Offset(const DPoint2 &delta);

	// optional overrides
	bool SetExtent(const DRECT &rect);
	void GetPropertyText(wxString &str);

	DPoint2 GetSpacing() const;
	vtBitmap *GetBitmap() { return m_pBitmap; }

	void GetDimensions(int &xsize, int &ysize)
	{
		xsize = m_iXSize;
		ysize = m_iYSize;
	}
	bool GetFilteredColor(const DPoint2 &p, RGBi &rgb);
	void GetRGB(int x, int y, RGBi &rgb);

	bool ImportFromFile(const wxString &strFileName, bool progress_callback(int) = NULL);
	bool ReadPPM(const char *fname, bool progress_callback(int) = NULL);
	bool SaveToFile(const char *fname) const;
	bool ReadPNGFromMemory(unsigned char *buf, int len);
	void SetRGB(int x, int y, unsigned char r, unsigned char g, unsigned char b);
	void SetRGB(int x, int y, const RGBi &rgb);

	bool ReadFeaturesFromTerraserver(const DRECT &area, int iTheme,
		int iMetersPerPixel, int iUTMZone, const char *filename);
	bool WriteGridOfTilePyramids(const TilingOptions &opts, BuilderView *pView);
	bool WriteTile(const TilingOptions &opts, BuilderView *pView, vtString &dirname,
		DRECT &tile_area, DPoint2 &tile_dim, int col, int row, int lod,
		bool bCompress);

protected:
	void SetDefaults();
	bool LoadFromGDAL();
	void CleanupGDALUsage();
	vtProjection	m_proj;

	DRECT   m_Extents;
	int		m_iXSize;
	int		m_iYSize;

	vtBitmap	*m_pBitmap;

	// used when reading from a file with GDAL
	int iRasterCount;
	unsigned char *pScanline;
	unsigned char *pRedline;
	unsigned char *pGreenline;
	unsigned char *pBlueline;
	GDALRasterBand *pBand;
	GDALRasterBand *pRed;
	GDALRasterBand *pGreen;
	GDALRasterBand *pBlue;
	int nxBlocks, nyBlocks;
	int xBlockSize, yBlockSize;
	GDALColorTable *pTable;
	GDALDataset *pDataset;

	Scanline m_row[BUF_SCANLINES];
	int m_use_next;

	void ReadScanline(int y, int bufrow);
	RGBi *GetScanlineFromBuffer(int y);

	// Used during writing of tilesets
	int m_iTotal, m_iCompleted;
#if USE_OPENGL
	ImageGLCanvas *m_pCanvas;
#endif
};

// Helper
int GetBitDepthUsingGDAL(const char *fname);

#endif	// IMAGELAYER_H
