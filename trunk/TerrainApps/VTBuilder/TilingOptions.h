//
// TilingOptions.h
//
// Copyright (c) 2006-2007 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef TilingOptions_H
#define TilingOptions_H

#include "ElevDrawOptions.h"

enum TextureCompressionType { TC_OPENGL, TC_SQUISH_FAST, TC_SQUISH_SLOW };

struct TilingOptions
{
	int cols, rows;
	int lod0size;
	int numlods;
	vtString fname;

	// If this is an elevation tileset, then optionally a corresponding
	//  derived image tileset can be created.
	bool bCreateDerivedImages;
	vtString fname_images;
	ElevDrawOptions draw;

	bool bOmitFlatTiles;
	bool bUseTextureCompression;
	TextureCompressionType eCompressionType;
};

#endif // TilingOptions_H
