//
// Options.h
//
// Copyright (c) 2001-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef OPTIONSH
#define OPTIONSH

#include "vtdata/FilePath.h"

class EnviroOptions
{
public:
	EnviroOptions();
	~EnviroOptions();

	bool ReadXML(const char *szFilename);
	bool WriteXML();

	// older method was .ini files
	bool ReadINI(const char *szFilename);

	vtStringArray m_DataPaths;
	bool		m_bEarthView;
	vtString	m_strEarthImage;
	vtString	m_strInitTerrain;
	vtString	m_strInitLocation;

	bool	m_bStartInNeutral;

	// display options, window location and size
	bool	m_bFullscreen;
	bool	m_bStereo;
	int		m_iStereoMode;
	IPoint2	m_WinPos, m_WinSize;
	bool	m_bLocationInside;

	bool	m_bHtmlpane;
	bool	m_bFloatingToolbar;
	bool	m_bTextureCompression;
	bool	m_bDisableModelMipmaps;

	bool	m_bDirectPicking;
	float	m_fSelectionCutoff;
	float	m_fMaxPickableInstanceRadius;
	float	m_fCursorThickness;

	float	m_fPlantScale;
	bool	m_bShadows;
	bool	m_bOnlyAvailableSpecies;

	float	m_fCatenaryFactor;

	vtString	m_strContentFile;

	// filename (with path) from which ini was read
	vtString m_strFilename;

	// look for all data here
};

extern EnviroOptions g_Options;

#endif	// OPTIONSH
