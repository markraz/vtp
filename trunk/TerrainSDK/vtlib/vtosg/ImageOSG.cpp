//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "vtlib/vtlib.h"
#include "vtdata/vtString.h"
#include <osgDB/ReadFile>

//
// Set any of these definitions to use OSG's own support for the various
// image file formats instead of our own.
//
#define USE_OSG_FOR_PNG		1
#define USE_OSG_FOR_BMP		1
#define USE_OSG_FOR_JPG		1


vtImage::vtImage(const char *fname, int internalformat) : vtImageBase(fname)
{
	ref();

	m_pPngData = NULL;
	m_internalformat = internalformat;
	m_strFilename = fname;

#if !USE_OSG_FOR_BMP
	if (!stricmp(fname + strlen(fname) - 3, "bmp"))
	{
		vtDIB pDIB;
		if (pDIB.ReadBMP(fname))
		{
			_CreateFromDIB(&pDIB);
			m_bLoaded = true;
		}
	}
	else
#endif

#if !USE_OSG_FOR_JPG
	if (!stricmp(fname + strlen(fname) - 3, "jpg"))
	{
		vtDIB pDIB;
		if (pDIB.ReadJPEG(fname))
		{
			_CreateFromDIB(&pDIB);
			m_bLoaded = true;
		}
	}
	else
#endif

#if !USE_OSG_FOR_PNG
	if (!stricmp(fname + strlen(fname) - 3, "png"))
	{
		_ReadPNG(fname);
	}
	else
#endif

	// try to load with OSG (osgPlugins libraries)
	{
//		std::string fname2 = "../Data/PlantModels/koahaole1_v2_256.png";
//		fname2 = fname;
		m_pOsgImage = osgDB::readImageFile(fname);
		if (m_pOsgImage != NULL)
			m_bLoaded = true;
	}
}

vtImage::vtImage(vtDIB *pDIB, int internalformat)
{
	ref();

	m_pPngData = NULL;
	m_internalformat = internalformat;

	_CreateFromDIB(pDIB);
}

vtImage::~vtImage()
{
	m_pOsgImage = NULL;	// dereference
	if (m_pPngData) free(m_pPngData);
}

void vtImage::Release()
{
	unref();
}

void vtImage::_CreateFromDIB(vtDIB *pDIB)
{
	m_pOsgImage = new osg::Image();

	int i, w, h, bpp;
	char *data;

	w   = pDIB->GetWidth();
	h   = pDIB->GetHeight();
	bpp = pDIB->GetDepth();
	data = (char *) pDIB->GetDIBData();

	int SizeImage = w * h * (bpp / 8);
	int SizeRow = w * (bpp / 8);

	GLubyte *image = new GLubyte[SizeImage];

#if 1
	memcpy(image, data, SizeImage);
#else
	// Flip the orientation of the image: a DIB is stored bottom-up, but
	// OSG and OpenGL expect the bitmap to be top-down.
	// (Why is this not required?? It should be, but it works correctly
	//  if we don't do the flip.)
	for (i = 0; i < h; i++)
		memcpy(image + i * SizeRow, data + (h-1-i) * SizeRow, SizeRow);
#endif

	int z;
	int pixelFormat = GL_RGB;
	GLubyte tmp;
	if ( bpp == 24 )
	{
		z = 3;

		/* BGR --> RGB */
		for (i = 0; i < w * h; i++)
		{
			tmp = image [ 3 * i ];
			image [ 3 * i ] = image [ 3 * i + 2 ];
			image [ 3 * i + 2 ] = tmp;
		}
		pixelFormat = GL_RGB;
	}
	else if ( bpp == 32 )
	{
		z = 4;

		/* BGRA --> RGBA */
		for (i = 0; i < w * h; i++ )
		{
			tmp = image [ 4 * i ];
			image [ 4 * i ] = image [ 4 * i + 2 ];
			image [ 4 * i + 2 ] = tmp;
		}
		pixelFormat = GL_RGBA;
	}
	else if ( bpp == 8 )
	{
		pixelFormat = GL_LUMINANCE;
	}
	int internalFormat;

	if (m_internalformat == -1)
		internalFormat = pixelFormat;	// use default
	else
		internalFormat = m_internalformat;	// use specific

	m_pOsgImage->setImage(w, h, 1,	// s, t, r
	   internalFormat,		// int internalFormat,
	   pixelFormat,			// unsigned int pixelFormat,
	   GL_UNSIGNED_BYTE, 	// unsigned int dataType,
	   image,
	   osg::Image::USE_NEW_DELETE);
}


//////////////////////////

#if USE_OSG_FOR_PNG

bool vtImage::_ReadPNG(const char *filename)
{
	return false;
}

#else

#include "png.h"

bool vtImage::_ReadPNG(const char *filename)
{
	m_pOsgImage = new osg::Image();

	FILE *fp = NULL;

	unsigned char header[8];
	png_structp png;
	png_infop   info;
	png_infop   endinfo;
	png_bytep  *row_p;

	png_uint_32 width, height;
	int depth, color;

	png_uint_32 i;
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png)
	{
		// We compiled against the headers of one version of libpng, but
		// linked against the libraries from another version.  If you get
		// this, fix the paths in your development environment.
		return false;
	}
	info = png_create_info_struct(png);
	endinfo = png_create_info_struct(png);

	fp = fopen(filename, "rb");
	if (fp && fread(header, 1, 8, fp) && png_check_sig(header, 8))
		png_init_io(png, fp);
	else
	{
		png_destroy_read_struct(&png, &info, &endinfo);
		return false;
	}
	png_set_sig_bytes(png, 8);

	png_read_info(png, info);
	png_get_IHDR(png, info, &width, &height, &depth, &color, NULL, NULL, NULL);

	if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	// never strip alpha
//	{
//		png_set_strip_alpha(png);
//		color &= ~PNG_COLOR_MASK_ALPHA;
//	}

	// Always expand paletted images
	if (color == PNG_COLOR_TYPE_PALETTE)
		png_set_expand(png);

	/*--GAMMA--*/
//	checkForGammaEnv();
	double screenGamma = 2.2 / 1.0;
#if 0
	// Getting the gamma from the PNG file is disabled here, since
	// PhotoShop writes bizarre gamma values like .227 (PhotoShop 5.0)
	// or .45 (newer versions)
	double	fileGamma;
	if (png_get_gAMA(png, info, &fileGamma))
		png_set_gamma(png, screenGamma, fileGamma);
	else
#endif
		png_set_gamma(png, screenGamma, 1.0/2.2);

	png_read_update_info(png, info);

	m_pPngData = (png_bytep) malloc(png_get_rowbytes(png, info)*height);
	row_p = (png_bytep *) malloc(sizeof(png_bytep)*height);

	bool StandardOrientation = true;
	for (i = 0; i < height; i++) {
		if (StandardOrientation)
			row_p[height - 1 - i] = &m_pPngData[png_get_rowbytes(png, info)*i];
		else
			row_p[i] = &m_pPngData[png_get_rowbytes(png, info)*i];
	}

	png_read_image(png, row_p);
	free(row_p);

	int iBitCount;

	switch (color)
	{
		case PNG_COLOR_TYPE_GRAY:
		case PNG_COLOR_TYPE_RGB:
		case PNG_COLOR_TYPE_PALETTE:
			iBitCount = 24;
			break;

		case PNG_COLOR_TYPE_GRAY_ALPHA:
		case PNG_COLOR_TYPE_RGB_ALPHA:
			iBitCount = 32;
			break;

		default:
			return false;
	}

	png_read_end(png, endinfo);
	png_destroy_read_struct(&png, &info, &endinfo);

	// Don't free the data, we're going to pass it to OSG
//	free(m_pPngData);

	if (fp)
		fclose(fp);

	int pixelFormat;
	unsigned int internalFormat;

	if (iBitCount == 24)
		pixelFormat = GL_RGB;
	else if (iBitCount == 32)
		pixelFormat = GL_RGBA;

	if (m_internalformat == -1)
		internalFormat = pixelFormat;	// use default
	else
		internalFormat = m_internalformat;	// use specific

	m_pOsgImage->setImage(width, height, 1,
	   internalFormat,		// int internalFormat,
	   pixelFormat,			// unsigned int pixelFormat
	   GL_UNSIGNED_BYTE,	// unsigned int dataType
	   m_pPngData,
	   osg::Image::USE_MALLOC_FREE);

	m_bLoaded = true;

	return true;
}

#endif	// USE_OSG_FOR_PNG

