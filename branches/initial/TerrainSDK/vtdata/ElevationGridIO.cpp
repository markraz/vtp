//
// vtElevationGridIO.cpp
//
// This modules contains the implementations of the file I/O methods of
// the class vtElevationGrid.
//
// Copyright (c) 2001 Virtual Terrain Project.
// Free for all uses, see license.txt for details.
//

#include <stdlib.h>
#include <string.h>
#include <fstream>
using namespace std;

#include "ElevationGrid.h"
#include "ByteOrder.h"

extern "C" {
#include "netcdf.h"
}

// GDAL
#include "gdal_priv.h"


// ************** DConvert - DEM Helper fn ****************

double DConvert(FILE *fp, int nmax)
{
	int  np;
	char cChar;
	char szCharString [64];

	np=0;
	while ((cChar=fgetc(fp)) == ' ')
	{
		szCharString[np] = cChar;
		np++;
	}

	szCharString[np] = cChar;
	np++;

	while ((cChar=fgetc(fp)) != ' ')
	{
		if (cChar=='D') cChar='E';
		szCharString[np] = cChar;
		np++;
		if (np==nmax) break;
	}
	szCharString[np] = 0;

	return atof(szCharString);
}


/** Loads elevation from a USGS DEM file.
 * If the data from DEM is in meters, then values are stored as shorts.
 * If DEM data is in feet, then height data will be stored in float, to
 * preserve the precision of the original data.
 * Some non-standard variations of the DEM format are supported.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromDEM(const char *szFileName,
								  void progress_callback(int))
{
	int		i, j;
	FILE	*fp;
	long	lygap;
	int		njunk, nCPoints;
	double	djunk, dxStart, dyStart;
	int		nRow, nColumn;
	int		nElev;
	int		nVUnit, nGUnit;
	double	flRows;
	double 	dxdelta, dydelta, dzdelta;
	double	dElevMax, dElevMin;
	bool	bNewFormat;
	int		nCoordSystem;
	int		nProfiles;
	char	szName[41];
	char	szDateBuffer[5];
	DPoint2	corners[4];			// SW, NW, NE, SE
	DPoint2	extent_min, extent_max;
	double	fGMeters;			// vertical units
	int		iDataStartOffset;
	double	fVRes;
	bool	bUTM;
	int		iUTMZone;
	DATUM	eDatum;

	if (progress_callback != NULL) progress_callback(0);

	if (NULL == (fp = fopen(szFileName,"rb")))
	{
		// Cannot Open File
		return false;
	}

	// check for version of DEM format
	fseek(fp, 864, 0);			 			// Read DEM into matrix
	fscanf(fp, "%d", &nRow);
	fscanf(fp, "%d", &nColumn);
	bNewFormat = ((nRow!=1)||(nColumn!=1));
	if (bNewFormat)
	{
		fseek(fp, 1024, 0); 	// New Format
		fscanf(fp, "%d", &i);
		fscanf(fp, "%d", &j);
		if ((i!=1)||(j!=1))			// File OK?
		{
			fseek(fp, 893, 0); 	// Undocumented Format
			fscanf(fp, "%d", &i);
			fscanf(fp, "%d", &j);
			if ((i!=1)||(j!=1))			// File OK?
			{
				// Not a DEM file
				fclose(fp);
				return false;
			}
			else
				iDataStartOffset = 893;
		}
		else
			iDataStartOffset = 1024;
	}
	else
		iDataStartOffset = 864;

	// Read the embedded DEM name
	fseek(fp, 0, 0);
	fgets(szName, 41, fp);
	int len = strlen(szName);	// trim trailing whitespace
	while (len > 0 && szName[len-1] == ' ')
	{
		szName[len-1] = 0;
		len--;
	}
	strcpy(m_szOriginalDEMName, szName);

	fseek(fp, 156, 0);
	fscanf(fp, "%d", &nCoordSystem);
	fscanf(fp, "%d", &iUTMZone);

	if (nCoordSystem == 0)	// geographic (lat-lon)
	{
		bUTM = false;
		iUTMZone = -1;
	}
	if (nCoordSystem == 1)	// utm
		bUTM = true;

	fseek(fp, 528, 0);
	fscanf(fp, "%d", &nGUnit);
	fscanf(fp, "%d", &nVUnit);

	// Ground Units in meters
	switch (nGUnit)
	{
	case 1: fGMeters = 0.3048;	break;	// 1 = feet
	case 2: fGMeters = 1.0;		break;	// 2 = meters
	case 3: fGMeters = 30.922;	break;	// 3 = arc-seconds
	}

	// Vertical Units in meters
	switch (nVUnit)
	{
	case 1:  m_fVMeters = 0.3048f; break;	// feet to meter conversion
	case 2:  m_fVMeters = 1.0f;	   break;	// meters == meters
	default: m_fVMeters = 1.0f;	   break;	// anything else, assume meters
	}

	fseek(fp, 816, 0);
	dxdelta = DConvert(fp, 12);
	dydelta = DConvert(fp, 12);
	dzdelta = DConvert(fp, 12);

	fVRes = dzdelta;

	if (nVUnit==1 || fVRes < 1.0)
		m_bFloatMode = true;	//store elevation values as floats.
	else
		m_bFloatMode = false;

	// Read the coordinates of the 4 corners
	fseek(fp, 546, 0);
	for (i = 0; i < 4; i++)
	{
		corners[i].x = DConvert(fp, 48);
		corners[i].y = DConvert(fp, 48);
	}

	// find absolute extents of raw vales
	extent_min.x = min(corners[0].x, corners[1].x);
	extent_max.x = max(corners[2].x, corners[3].x);
	extent_min.y = min(corners[0].y, corners[3].y);
	extent_max.y = max(corners[1].y, corners[2].y);

	if (bUTM)	// UTM
	{
		for (i = 0; i < 4; i++)
			m_Corners[i] = corners[i];
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			// convert arcseconds to degrees
			m_Corners[i].x = corners[i].x / 3600.0;
			m_Corners[i].y = corners[i].y / 3600.0;
		}
	}

	dElevMin = DConvert(fp, 48);
	dElevMax = DConvert(fp, 48);

	fseek(fp, 858, 0);
	fscanf(fp, "%d", &nProfiles);

	eDatum = NAD27;	// default

	// OLD format header ends at byte 864
	if (bNewFormat)
	{
		// year of data compilation
		fseek(fp, 876, 0);
		fread(szDateBuffer, 4, 1, fp);
		szDateBuffer[4] = 0;

		// Horizontal datum
		// 1=North American Datum 1927 (NAD 27)
		// 2=World Geodetic System 1972 (WGS 72)
		// 3=WGS 84
		// 4=NAD 83
		// 5=Old Hawaii Datum
		// 6=Puerto Rico Datum
		int datum;
		fseek(fp, 890, 0);
		fscanf(fp, "%d", &datum);
		switch (datum) {
			case 1: eDatum = NAD27; break;
			case 2: eDatum = WGS_72; break;
			case 3: eDatum = WGS_84; break;
			case 4:	eDatum = NAD83; break;
			case 5: eDatum = OLD_HAWAIIAN_MEAN; break;
			case 6: eDatum = PUERTO_RICO; break;
		}
	}

	// Set up the projection
	m_proj.SetProjectionSimple(bUTM, iUTMZone, eDatum);

	ComputeExtentsFromCorners();

	// Compute number of columns and rows
	m_iColumns = nProfiles;
	if (bUTM)	// UTM
	{
		flRows = (m_area.top - m_area.bottom)/dydelta;
		m_iRows = (int)(flRows + 0.9999999);	// round up to the nearest integer
	}
	else
	{
		flRows = (float)(m_area.top - m_area.bottom) * 1200.0f;
		m_iRows = (int)flRows + 1;	// 1 more than you might expect
	}

	AllocateArray();

	// jump to start of actual data
	fseek(fp, iDataStartOffset, 0);

	bool bad = false;
	for (i = 0; i < nProfiles; i++)
	{
		if (progress_callback != NULL) progress_callback(i*100/m_iColumns);
		fscanf(fp, "%d", &njunk);
		fscanf(fp, "%d", &njunk);
		fscanf(fp, "%d", &nCPoints);
		fscanf(fp, "%d", &njunk);
		dxStart = DConvert(fp, 48);
		dyStart = DConvert(fp, 48);
		djunk = DConvert(fp, 48);
		djunk = DConvert(fp, 48);
		djunk = DConvert(fp, 48);

		lygap = (long)((dyStart-extent_min.y)/dydelta);

		for (j=(int)lygap; j<(nCPoints+(int)lygap); j++)
		{
			fscanf(fp, "%d", &nElev);
			if (j < 0 || j > m_iRows-1)
				bad = true;
			else
				SetFValue(i, j, (float)(nElev*m_fVMeters*fVRes));
		}
	}
	fclose(fp);

	ComputeHeightExtents();

	return true;
}


/** Loads from a netCDF file.
 * Elevation values are assumed to be integer meters.  Projection is
 * assumed to be geographic.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromCDF(const char *szFileName,
								void progress_callback(int))
{
#if SUPPORT_NETCDF
	int id;

	/* open existing netCDF dataset */
	int status = nc_open(szFileName, NC_NOWRITE, &id);
	if (status != NC_NOERR)
		return false;

	if (progress_callback != NULL) progress_callback(0);

	// get dimension IDs
	int id_side, id_xysize;
	nc_inq_dimid(id, "side", &id_side);
	nc_inq_dimid(id, "xysize", &id_xysize);
	size_t xysize_length;
	nc_inq_dimlen(id, id_xysize, &xysize_length);

	// get variable IDs
	int id_xrange, id_yrange, id_zrange;
	int id_spacing, id_dimension, id_z;
	nc_inq_varid(id, "x_range", &id_xrange);
	nc_inq_varid(id, "y_range", &id_yrange);
	nc_inq_varid(id, "z_range", &id_zrange);
	nc_inq_varid(id, "spacing", &id_spacing);
	nc_inq_varid(id, "dimension", &id_dimension);
	nc_inq_varid(id, "z", &id_z);

	// get values of variables
	double xrange[2], yrange[2], zrange[2], spacing[2];
	int dimension[2];
	nc_get_var_double(id, id_xrange, xrange);
	nc_get_var_double(id, id_yrange, yrange);
	nc_get_var_double(id, id_zrange, zrange);
	nc_get_var_double(id, id_spacing, spacing);
	nc_get_var_int(id, id_dimension, dimension);

	double *z = new double[xysize_length];
	if (progress_callback != NULL) progress_callback(20);

	nc_get_var_double(id, id_z, z);
	if (progress_callback != NULL) progress_callback(60);

	nc_close(id);				// close netCDF dataset

	// Now copy the values into the vtElevationGrid object
	m_iColumns = dimension[0];
	m_iRows = dimension[1];

	m_bFloatMode = false;
	AllocateArray();
	if (progress_callback != NULL) progress_callback(80);

	int i, j;
	for (i = 0; i < m_iColumns; i++)
	{
		for (j = 0; j < m_iRows; j++)
		{
			SetValue(i, m_iRows-1-j, (short)z[j*m_iColumns+i]);
		}
	}
	if (progress_callback != NULL) progress_callback(90);

	m_proj.SetProjectionSimple(false, 0, WGS_84);

	m_area.left = xrange[0];
	m_area.right = xrange[1];
	m_area.top = yrange[1];
	m_area.bottom = yrange[0];

	ComputeCornersFromExtents();

	// delete temporary storage
	delete z;

	return true;
#else
	// no support for netCDF
	return false;
#endif
}


/** Loads from a Arc/Info compatible ASCII grid file.
 * Projection is read from a corresponding .prj file.
 *
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromASC(const char *szFileName,
								void progress_callback(int))
{
	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
		return false;

	if (progress_callback != NULL) progress_callback(0);
	char buf[4000];

	// get dimension IDs
	fgets(buf, 4000, fp);

	int ncols, nrows, xllcorner, yllcorner, cellsize, nodata;
	int result = sscanf(buf, "ncols %d", &ncols);
	if (result != 1)
		return false;	// not an ASC file

	fgets(buf, 4000, fp);
	sscanf(buf, "nrows %d", &nrows);

	fgets(buf, 4000, fp);
	sscanf(buf, "xllcorner %d", &xllcorner);

	fgets(buf, 4000, fp);
	sscanf(buf, "yllcorner %d", &yllcorner);

	fgets(buf, 4000, fp);
	sscanf(buf, "cellsize %d", &cellsize);

	fgets(buf, 4000, fp);
	sscanf(buf, "NODATA_value %d", &nodata);

	m_iColumns = ncols;
	m_iRows = nrows;

	m_proj.SetProjectionSimple(true, 1, WGS_84);

	m_bFloatMode = false;

	m_area.left = xllcorner;
	m_area.right = xllcorner + (ncols - 1) * cellsize;
	m_area.top = yllcorner + (nrows - 1) * cellsize;
	m_area.bottom = yllcorner;

	ComputeCornersFromExtents();
	AllocateArray();

	int i, j, z;
	for (i = 0; i < nrows; i++)
	{
		if (progress_callback != NULL) progress_callback(i*100/nrows);
		for (j = 0; j < ncols; j++)
		{
			fscanf(fp, "%d", &z);
			if (z != nodata)
				SetValue(j, nrows-1-i, (short)z);
		}
	}
	fclose(fp);
	return true;
}


/** Loads from a Terragen Terrain file.
 *
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromTerragen(const char *szFileName,
								void progress_callback(int))
{
	char buf[8];
	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
		return false;

	// get file identifier
	fread(buf, 8, 1, fp);
	if (strncmp(buf, "TERRAGEN", 8))
		return false;
	fread(buf, 8, 1, fp);
	if (strncmp(buf, "TERRAIN ", 8))
		return false;

	if (progress_callback != NULL) progress_callback(0);

	m_proj.SetProjectionSimple(true, 1, WGS_84);

	m_bFloatMode = false;

	short svalue, dummy;
	FPoint3 scale(30.0f, 30.0f, 30.0f);
	float fvalue;
	int i, j;

	while (fread(buf, 4, 1, fp) == 1)
	{
		if (!strncmp(buf, "SIZE", 4))
		{
			fread(&svalue, 2, 1, fp);
			fread(&dummy, 2, 1, fp);
			m_iRows = m_iColumns = svalue + 1;
		}
		else if (!strncmp(buf, "XPTS", 4))
		{
			fread(&svalue, 2, 1, fp);
			fread(&dummy, 2, 1, fp);
			m_iColumns = svalue;
		}
		else if (!strncmp(buf, "YPTS", 4))
		{
			fread(&svalue, 2, 1, fp);
			fread(&dummy, 2, 1, fp);
			m_iRows = svalue;
		}
		else if (!strncmp(buf, "SCAL", 4))
		{
			fread(&scale.x, 4, 1, fp);
			fread(&scale.y, 4, 1, fp);
			fread(&scale.z, 4, 1, fp);
		}
		else if (!strncmp(buf, "CRAD", 4))
		{
			// radius of planet
			fread(&fvalue, 4, 1, fp);
		}
		else if (!strncmp(buf, "CRVM", 4))
		{
			// "curve mode"
			fread(&svalue, 2, 1, fp);
			fread(&dummy, 2, 1, fp);
		}
		else if (!strncmp(buf, "ALTW", 4))
		{
			short HeightScale;
			short BaseHeight;
			fread(&HeightScale, 2, 1, fp);
			fread(&BaseHeight, 2, 1, fp);

			AllocateArray();
			for (j = 0; j < m_iRows; j++)
			{
				if (progress_callback != NULL) progress_callback(j*100/m_iRows);
				for (i = 0; i < m_iColumns; i++)
				{
					fread(&svalue, 2, 1, fp);
					SetValue(i, j, scale.z * (BaseHeight + ((float)svalue * HeightScale / 65536.0f)));
				}
			}
		}
		else if (!strncmp(buf, "EOF ", 4))
		{
			// do nothing
		}
	}
	fclose(fp);

	// make up some extents, based on the scaling
	m_area.left = 0;
	m_area.right = (m_iColumns - 1) * scale.x;
	m_area.top = (m_iRows - 1) * scale.y;
	m_area.bottom = 0;
	ComputeCornersFromExtents();

	return true;
}


//
// Helper for DTED reader: read an 8-byte number in the
// form DDDMMSSH, degrees-minute-seconds-hemisphere
//
float get_dms8(FILE *fp)
{
	float f;

	char buf[8];
	fread(buf, 8, 1, fp);

	buf[3] = 0;
	f = (float)atof(buf);
	if (buf[7] == 'W') f = -f;
	return f;
}

/** Loads from a DTED file.
 * \par
 * Should support DTED0, DTED1 and DTED2 files, although it has only been
 * tested on DTED0.  Projection is assumed to be geographic and elevation
 * is integer meters.
 * \returns \c true if the file was successfully opened and read.
 */
//
// DTED chunks/bytes:
//  UHL 80
//  DSI 648
//  ACC 2700
//  Data (variable)
//
bool vtElevationGrid::LoadFromDTED(const char *szFileName,
								 void progress_callback(int))
{
	FILE *fp;
	char buf[80];

	if (!(fp = fopen(szFileName, "rb")))
	{
		// Cannot Open File
		return false;
	}

	// all DTEDs are geographic and in integer meters
	// datum is always WGS84
	m_proj.SetProjectionSimple(false, 0, WGS_84);
	m_bFloatMode = false;

	// check for correct format
	fread(buf, 4, 1, fp);
	if (strncmp(buf, "UHL1", 4))
	{
		// Not a DTED file
		fclose(fp);
		return false;
	}
	m_Corners[0].x = get_dms8(fp);
	m_Corners[0].y = get_dms8(fp);

	// imply other corners
	m_Corners[1].x = m_Corners[0].x;
	m_Corners[1].y = m_Corners[0].y + 1.0f;
	m_Corners[2].x = m_Corners[0].x + 1.0f;
	m_Corners[2].y = m_Corners[0].y + 1.0f;
	m_Corners[3].x = m_Corners[0].x + 1.0f;
	m_Corners[3].y = m_Corners[0].y;
	ComputeExtentsFromCorners();

	// get dimensions
	fseek(fp, 47, 0);
	fread(buf, 4, 1, fp);
	buf[4] = 0;
	m_iColumns = atoi(buf);
	fread(buf, 4, 1, fp);
	m_iRows = atoi(buf);

	AllocateArray();

	int line_length = 12 + 2 * m_iRows;
	unsigned char *linebuf = new unsigned char[line_length];
	unsigned char swap[2];

	fseek(fp, 80 + 648 + 2700, 0);
	int i, j, offset;
	for (i = 0; i < m_iColumns; i++)
	{
		if (progress_callback != NULL)
			progress_callback(i * 100 / m_iColumns);

		/*  FIXME:  there be byte order issues here.  See below in this routine.  */
		fread(linebuf, line_length, 1, fp);
		if (*linebuf != 0xaa)
		{
			break;
		}
		offset = 8;
		for (j = 0; j < m_iRows; j++)
		{
			swap[1] = *(linebuf + offset++);
			swap[0] = *(linebuf + offset++);
			int z = *((unsigned short *)swap);
			SetValue(i, j, z);
		}
	}
	delete linebuf;
	fclose(fp);
	return true;
}


/** Loads from a GTOPO30 file.
 * \par
 * GTOPO30 files are actually composed of at least 2 files, a header with a
 * .hdr extension and data with a .dem extension.  Pass the filename of
 * the .hdr file to this function, and it will automatically look for
 * a corresponding .dem file in the same location.
 * \par
 * Projection is always geographic and elevation is integer meters.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromGTOPO30(const char *szFileName,
									void progress_callback(int))
{
	if (progress_callback != NULL) progress_callback(1);

	// Open the header file
	ifstream hdrFile(szFileName);
	if (!hdrFile.is_open())
	  return false;

	// Parse the file
	char strName[30];
	char strValue[30];

	GTOPOHeader gh;

	// Read the byte order
	hdrFile >> strName >> gh.ByteOrder;

	// Read the layout
	hdrFile >> strName >> gh.Layout;
	// Read the number of rows
	hdrFile >> strName >> strValue;
	gh.NumRows = atol( strValue );

	// Read the number of columns
	hdrFile >> strName >> strValue;
	gh.NumCols = atol( strValue );

	// Read the number of bands
	hdrFile >> strName >> gh.Bands;

	// Read the number of bits per pixel
	hdrFile >> strName >> gh.Bits;

	// Read the band row bytes
	hdrFile >> strName >> gh.BandRowBytes;

	// Read the total row bytes
	hdrFile >> strName >> gh.TotalRowBytes;

	// Read the band gap bytes
	hdrFile >> strName >> gh.BandGapBytes;

	// Read the no data value
	hdrFile >> strName >> strValue;
	gh.NoData = atoi( strValue );

	// Read the upper left x coordinate
	hdrFile >> strName >> strValue;
	gh.ULXMap = atof( strValue );

	// Read the upper left y coordinate
	hdrFile >> strName >> strValue;
	gh.ULYMap = atof( strValue );

	// Read the x pixel spacing
	hdrFile >> strName >> strValue;
	gh.XDim = atof( strValue );

	// Read the y pixel spacing
	hdrFile >> strName >> strValue;
	gh.YDim = atof( strValue );

	// Close the file
	hdrFile.close();

	// make the corresponding filename for the DEM
	char dem_fname[200];
	strcpy(dem_fname, szFileName);
	char *ext = strrchr(dem_fname, '.');
	if (!ext)
		return false;
	strcpy(ext, ".dem");
	FILE *fp = fopen(dem_fname, "rb");
	if (!fp)
		return false;

	if (progress_callback != NULL) progress_callback(5);

	// Projection is always geographic, integer
	m_proj.SetProjectionSimple(false, 0, WGS_84);
	m_bFloatMode = false;

	m_area.left = gh.ULXMap;
	m_area.top = gh.ULYMap;
	m_area.right = m_area.left + (gh.XDim * (gh.NumCols-1));
	m_area.bottom = m_area.top - (gh.YDim * (gh.NumRows-1));
	ComputeCornersFromExtents();

	// set up for an array of the indicated size
	m_iColumns = gh.NumCols;
	m_iRows = gh.NumRows;

	AllocateArray();

	// read the file
	int i, j;
	short z;
	char *cp = (char *)&z, temp;
	for (j = 0; j < m_iRows; j++)
	{
		if (progress_callback != NULL) progress_callback(10 + j * 90 / m_iRows);
		for (i = 0; i < m_iColumns; i++)
		{
			/*  FIXME:  there be byte order issues here.  See below in this routine.  */
			fread(&z, sizeof(short), 1, fp);
			// must swap byte order
			temp = cp[0];
			cp[0] = cp[1];
			cp[1] = temp;
			SetValue(i, m_iRows-1-j, (z == gh.NoData) ? 0 : z);
		}
	}
	return true;
}


/** Loads from a Surfer binary grid file (GRD)
 * \par
 * Projection is always geographic and elevation is floating-point.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromGRD(const char *szFileName,
								void progress_callback(int))
{
	if (progress_callback != NULL) progress_callback(1);

	// Open the header file
	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
	  return false;

	// Parse the file
	char szHeader[5];
	fread(szHeader, 4, 1, fp);
	if (strncmp(szHeader, "DSBB", 4))
	{
		// not the right kind of file
		fclose(fp);
		return false;
	}

	// read GRD header data
	short nx, ny;
	double xlo, xhi, ylo, yhi, zlo, zhi;
	/*  FIXME:  there be byte order issues here.  See below in this routine.  */
	fread(&nx, 2, 1, fp);
	fread(&ny, 2, 1, fp);
	fread(&xlo, 8, 1, fp);
	fread(&xhi, 8, 1, fp);
	fread(&ylo, 8, 1, fp);
	fread(&yhi, 8, 1, fp);
	fread(&zlo, 8, 1, fp);
	fread(&zhi, 8, 1, fp);

	// Set the projection (actually we don't know it)
	m_proj.SetProjectionSimple(true, 1, WGS_84);

	// set the corresponding vtElevationGrid info
	m_bFloatMode = true;
	m_area.left = xlo;
	m_area.top = yhi;
	m_area.right = xhi;
	m_area.bottom = ylo;
	ComputeCornersFromExtents();

	m_iColumns = nx;
	m_iRows = ny;

	AllocateArray();

	int x, y;
	float z;
	for (y = 0; y < ny; y++)
	{
		if (progress_callback != NULL) progress_callback(y * 100 / ny);
		for (x = 0; x < nx; x++)
		{
			fread(&z, 4, 1, fp);
			SetFValue(x, y, z);
		}
	}
	fclose(fp);
	return true;
}


/** Loads from a PGM (Portable Gray Map) file.
 * Both PGM Binary and ASCII varieties are supported.
 * \par
 * PGM does not contain any information about geographic location, so
 * the following assumptions are made: UTM coordinates, 1-meter spacing,
 * origin at (0,0).
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromPGM(const char *szFileName, void progress_callback(int))
{
	int xsize,ysize;
	char sbuf[512],			/* buffer for file input */
	dummy[2];				/* used for \n and \0   */
	char *junk;				/* unconverted part of a number */
	double a, maxval;
	FILE *fpin;				/* input file pointer */
	bool bBinary;			/* PGM binary format flag */
	unsigned char oneb;		/* one byte from file */

	/* open input file */
	if ((fpin = fopen(szFileName, "rb")) == NULL)
	{
		perror("Could not open input file ");
		return false;
	}

	if (fread(sbuf, sizeof(char), 2, fpin) != 2)
	{
		perror("Could not read file");
		return false;
	}

	bBinary = false;	/* assume PGM ascii */
	if (strncmp(sbuf,"P5",2) == 0)
		bBinary = 1;	/* PGM binary format */

	if (strncmp(sbuf,"GI",2) == 0) {
		fclose(fpin);
		return false;
	}
	else if ((strncmp(sbuf,"P5",2) != 0) && (strncmp(sbuf,"P2",2) != 0))
	{
		/* not PGM Ascii */
		fclose(fpin);
		return false;
	}

	/* read PGM ASCII or binary file */
	while ((fscanf(fpin, "%s", sbuf) != EOF) && sbuf[0] == '#')
	{
		// comment
		fscanf(fpin,"%[^\n]", sbuf);  /* read comment beyond '#' */
		fscanf(fpin,"%[\n]", dummy);  /* read newline */
	}

	xsize = atoi(sbuf);				/* store xsize of array */
	fscanf(fpin,"%s",sbuf);			/* read ysize of array */
	ysize = atoi(sbuf);
	fscanf(fpin,"%s\n",sbuf);		/* read maxval of array */
	maxval = strtod(sbuf, &junk);	/* store maxval. could throw away. */

	// Set the projection (actually we don't know it)
	m_proj.SetProjectionSimple(true, 1, WGS_84);

	// set the corresponding vtElevationGrid info
	m_bFloatMode = true;
	m_area.left = 0;
	m_area.top = ysize;
	m_area.right = xsize;
	m_area.bottom = 0;
	ComputeCornersFromExtents();

	m_iColumns = xsize;
	m_iRows = ysize;

	AllocateArray();

	if (bBinary)
	{
		/* read PGM binary */
		for (int j = 0; j < ysize; j++)
		{
			if (progress_callback != NULL) progress_callback(j * 100 / ysize);
			for (int i = 0; i < xsize; i++)
			{
				fread(&oneb, sizeof(unsigned char), 1, fpin);
				SetFValue(i, ysize-1-j, oneb);
			}
		}
	}
	else
	{
		/* read PGM ASCII */
		for (int j = 0; j < ysize; j++)
		{
			if (progress_callback != NULL) progress_callback(j * 100 / ysize);
			for (int i = 0; i < xsize; i++)
			{
				fscanf(fpin, "%s", sbuf);
				a = strtod( sbuf, &junk);
				SetFValue(i, ysize-1-j, (float)a);
			}
		}
	}
	return true;
}


/** Loads just the header information from a BT (Binary Terrain) file.
 * \par
 * This can be useful if you want to check the information such as the
 * size of the data, without reading the entire file.
 * \returns \c true if the header was successfully parsed.
 */
bool vtElevationGrid::LoadBTHeader(const char *szFileName)
{
	short svalue, proj_type, zone, datum, external;
	char buf[11];
	float ftmp;

	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
		return false;		// Cannot Open File

	fread(buf, 10, 1, fp);
	buf[10] = '\0';

	if (strncmp(buf, "binterr", 7))
	{
		fclose(fp);
		return false;		// Not a current BT file
	}

	float version;
	sscanf(buf+7, "%f", &version);

	/*  NOTE:  BT format is little-endian  */
	FRead(&m_iColumns, DT_INT, 1, fp, BO_LITTLE_ENDIAN );
	FRead(&m_iRows,    DT_INT, 1, fp, BO_LITTLE_ENDIAN );

	// Default to internal projection
	external = 0;

	if (version == 1.0f)
	{
		FRead(&m_iDataSize, DT_INT, 1, fp, BO_LITTLE_ENDIAN );

		// UTM flag
		FRead(&svalue, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );
		proj_type = (svalue == 1);

		// UTM zone
		FRead(&zone, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );

		// 1.0 didn't support Datum, assume WGS84
		datum = WGS_84;

		// coordinate extents left-right
		FRead(&ftmp, DT_FLOAT, 1, fp, BO_LITTLE_ENDIAN );
		m_area.left = ftmp;
		FRead(&ftmp, DT_FLOAT, 1, fp, BO_LITTLE_ENDIAN );
		m_area.right = ftmp;

		// coordinate extents bottom-top
		FRead(&ftmp, DT_FLOAT, 1, fp, BO_LITTLE_ENDIAN );
		m_area.bottom = ftmp;
		FRead(&ftmp, DT_FLOAT, 1, fp, BO_LITTLE_ENDIAN );
		m_area.top = ftmp;

		// is the data floating point or integers?
		FRead(&m_bFloatMode, DT_INT, 1, fp, BO_LITTLE_ENDIAN );
		if (m_bFloatMode != true)
			m_bFloatMode = false;
	}
	else if (version == 1.1f || version == 1.2f)
	{
		FRead(&svalue, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );
		m_iDataSize = svalue;

		// Is floating point data?
		FRead(&svalue, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );
		m_bFloatMode = (svalue == 1);

		// Projection (0 = geo, 1 = utm, 2 = use external .prj file)
		FRead(&proj_type, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );

		// UTM zone (ignore unless projection == 1)
		FRead(&zone, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );

		// Datum (ignore unless projection == 0 or 1)
		FRead(&datum, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );

		// coordinate extents
		FRead(&m_area.left, DT_DOUBLE, 1, fp, BO_LITTLE_ENDIAN );
		FRead(&m_area.right, DT_DOUBLE, 1, fp, BO_LITTLE_ENDIAN );
		FRead(&m_area.bottom, DT_DOUBLE, 1, fp, BO_LITTLE_ENDIAN );
		FRead(&m_area.top, DT_DOUBLE, 1, fp, BO_LITTLE_ENDIAN );

		// External projection flag
		FRead(&external, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );
	}
	else
	{
		fclose(fp);
		return false;		// unknown version
	}

	// Set up projection
	if (external == 1)
	{
		// Read external projection (.prj) file
		char prj_name[256];
		strcpy(prj_name, szFileName);
		strcpy(prj_name + strlen(prj_name) - 3, ".prj");
		FILE *fp2 = fopen(prj_name, "rb");
		if (!fp2)
			return false;
		char wkt_buf[2000], *wkt = wkt_buf;
		fgets(wkt, 2000, fp2);
		OGRErr err = m_proj.importFromWkt((char **) &wkt);
		if (err != OGRERR_NONE)
			return false;
		fclose(fp2);
	}
	else
	{
		// Internal specification: proj_type 0 = Geo, 1 = UTM
		m_proj.SetProjectionSimple(proj_type == 1, zone, (DATUM) datum);
	}

	ComputeCornersFromExtents();

	fclose(fp);
	return true;
}


/** Loads from a BT (Binary Terrain) file.
 * \par
 * Both the current version (1.1) and older BT versions are supported.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromBT(const char *szFileName, void progress_callback(int))
{
	int i;

	// First load the header
	if (!LoadBTHeader(szFileName))
		return false;

	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
		return false;		// Cannot Open File

	// elevation data always starts at offset 256
	fseek(fp, 256, SEEK_SET);

	AllocateArray();

#if 0
	// slow way
	int value;
	float fvalue;
	for (i = 0; i < m_iColumns; i++)
	{
		if (progress_callback != NULL) progress_callback(i * 100 / m_iColumns);
		for (j = 0; j < m_iRows; j++)
		{
			if (m_bFloatMode) {
				FRead(&fvalue, DT_FLOAT, 1, fp, BO_LITTLE_ENDIAN );
				SetFValue(i, j, fvalue);
			} else {
				if (m_iDataSize == 4)
				{
					FRead(&value, DT_INT, 1, fp, BO_LITTLE_ENDIAN );
					SetValue(i, j, value);
				}
				else if (m_iDataSize == 2)
				{
					FRead(&svalue, DT_SHORT, 1, fp, BO_LITTLE_ENDIAN );
					SetValue(i, j, svalue);
				}
			}
		}
	}
#else
	// fast way
	if (m_bFloatMode)
	{
		for (i = 0; i < m_iColumns; i++)
		{
			if (progress_callback != NULL) progress_callback(i * 100 / m_iColumns);
			FRead(m_pFData + (i*m_iRows), DT_FLOAT, m_iRows, fp, BO_LITTLE_ENDIAN );
		}
	}
	else
	{
		for (i = 0; i < m_iColumns; i++)
		{
			if (progress_callback != NULL) progress_callback(i * 100 / m_iColumns);
			FRead(m_pData + (i*m_iRows), DT_SHORT, m_iRows, fp, BO_LITTLE_ENDIAN );
		}
	}
#endif

	// be sure to close the file!
	fclose(fp);

	ComputeHeightExtents();
	return true;
}


/** Writes the grid to a TerraGen Terrain file.
 * \param fp A pointer to a file open for writing.
 */
bool vtElevationGrid::SaveToTerragen(const char *szFileName)
{
	FILE *fp = fopen(szFileName, "wb");
	if (!fp)
		return false;

	short w = m_iColumns;
	short h = m_iRows;
	short dummy = 0;

	// write identifying header
	fwrite("TERRAGEN", 8, 1, fp);
	fwrite("TERRAIN ", 8, 1, fp);

	// write dimension
	fwrite("SIZE", 4, 1, fp);
	fwrite(&w, 2, 1, fp);
	fwrite(&dummy, 2, 1, fp);

	fwrite("XPTS", 4, 1, fp);
	fwrite(&w, 2, 1, fp);
	fwrite(&dummy, 2, 1, fp);

	fwrite("YPTS", 4, 1, fp);
	fwrite(&h, 2, 1, fp);
	fwrite(&dummy, 2, 1, fp);

	// write "scale"
	// this is actually the grid spacing (x, y) and elevation precision (z)
	fwrite("SCAL", 4, 1, fp);
	FPoint2 spacing = GetSpacing();
	float zscale = 1.0f;
	fwrite(&spacing.x, 4, 1, fp);
	fwrite(&spacing.y, 4, 1, fp);
	fwrite(&zscale, 4, 1, fp);

	// don't write "CRAD" and "CRVM"
	// they are optional and would not have meaningful values

	// write altitude (elevation data)
	fwrite("ALTW", 4, 1, fp);
	short HeightScale = 16384;		// divided by 65526, means units of 1/4 meter
	short BaseHeight = 0;
	fwrite(&HeightScale, 2, 1, fp);
	fwrite(&BaseHeight, 2, 1, fp);

	int i, j;
	float fvalue;
	short svalue;
	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			fvalue = GetFValue(i, j);
			svalue = (short) (fvalue * 4.0f);
			fwrite(&svalue, 2, 1, fp);
		}
	}
	fclose(fp);
	return true;
}


/** Writes the grid to a BT (Binary Terrain) file.
 * The current BT format version (1.2) is written.
 * \param szFileName The file name to write to.
 */
bool vtElevationGrid::SaveToBT(const char *szFileName, void progress_callback(int))
{
	FILE *fp = fopen(szFileName, "wb");
	if (!fp)
		return false;

	int w = m_iColumns;
	int h = m_iRows;
	short zone = m_proj.GetUTMZone();
	short datum = (short) m_proj.GetDatum();
	short isfloat = (short) IsFloatMode();
	short external = 1;		// always true: we always write an external .prj file

	short projection;
	if (m_proj.IsGeographic())
		projection = 0;
	else
		projection = 1;

	// Latest header, version 1.2
	short datasize = m_bFloatMode ? 4 : 2;

	fwrite("binterr1.2", 10, 1, fp);
	fwrite(&w, 4, 1, fp);
	fwrite(&h, 4, 1, fp);
	fwrite(&datasize, 2, 1, fp);
	fwrite(&isfloat, 2, 1, fp);
	fwrite(&projection, 2, 1, fp);	// Projection (0, 1, 2)
	fwrite(&zone, 2, 1, fp);		// UTM zone
	fwrite(&datum, 2, 1, fp);		// Datum

	// coordinate extents
	fwrite(&m_area.left, 8, 1, fp);
	fwrite(&m_area.right, 8, 1, fp);
	fwrite(&m_area.bottom, 8, 1, fp);
	fwrite(&m_area.top, 8, 1, fp);

	fwrite(&external, 2, 1, fp);	// External projection specification

	// now write the data: always starts at offset 256
	fseek(fp, 256, SEEK_SET);

#if 0
	// slow way, one heixel at a time
	for (int i = 0; i < w; i++)
	{
		if (progress_callback != NULL) progress_callback(i * 100 / w);
		for (j = 0; j < h; j++)
		{
			if (m_bFloatMode) {
				fvalue = GetFValue(i, j);
				fwrite(&fvalue, datasize, 1, fp);
			} else {
				svalue = GetValue(i, j);
				fwrite(&svalue, datasize, 1, fp);
			}
		}
	}
#else
	// fast way, with the assumption that the data is stored column-first in memory
	if (m_bFloatMode)
	{
		for (int i = 0; i < w; i++)
		{
			if (progress_callback != NULL) progress_callback(i * 100 / w);
			fwrite(m_pFData + (i * m_iRows), 4, m_iRows, fp);
		}
	}
	else
	{
		for (int i = 0; i < w; i++)
		{
			if (progress_callback != NULL) progress_callback(i * 100 / w);
			fwrite(m_pData + (i * m_iRows), 2, m_iRows, fp);
		}
	}
#endif
	fclose(fp);

	if (external)
	{
		// Write external projection file (.prj)
		char prj_name[256];
		strcpy(prj_name, szFileName);
		strcpy(prj_name + strlen(prj_name) - 3, ".prj");
		FILE *fp2 = fopen(prj_name, "wb");
		if (!fp2)
			return false;
		char *wkt;
		OGRErr err = m_proj.exportToWkt(&wkt);
		if (err != OGRERR_NONE)
			return false;
		fprintf(fp2, "%s\n", wkt);
		fclose(fp2);
		OGRFree(wkt);
	}

	return true;
}


/** Loads an elevation grid using the GDAL library.  The GDAL library
 * supports a very large number of formats, including SDTS-DEM.  See
 * http://www.remotesensing.org/gdal/formats_list.html for the complete list
 * of GDAL-supported formats.
 *
 * \param szFileName The file name to read from.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadWithGDAL(const char *szFileName,
								   void progress_callback(int))
{
	GDALDataset  *poDataset;

	GDALAllRegister();

	poDataset = (GDALDataset *) GDALOpen( szFileName, GA_ReadOnly );
	if( poDataset == NULL )
	{
		// failed.
		return false;
	}
	m_iColumns = poDataset->GetRasterXSize();
	m_iRows = poDataset->GetRasterYSize();

	// Get the projection information
	const char *str1 = poDataset->GetProjectionRef();
	char *str2 = (char *) str1;
	OGRErr err = m_proj.importFromWkt(&str2);
	if (err == OGRERR_CORRUPT_DATA)
	{
		// just assume that it's geographic
		m_proj.SetWellKnownGeogCS( "WGS84" );
	}

	// Get spacing and extents
	double		adfGeoTransform[6];
	if( poDataset->GetGeoTransform( adfGeoTransform ) != CE_None )
		return false;

	// Upper left corner is adfGeoTransform[0], adfGeoTransform[3]
	m_area.left = adfGeoTransform[0];
	m_area.top = adfGeoTransform[3];
	m_area.right = m_area.left + (adfGeoTransform[1] * m_iColumns);
	m_area.bottom = m_area.top + (adfGeoTransform[5] * m_iRows);
	ComputeCornersFromExtents();

	// Raster count should be 1 for elevation datasets
	int rc = poDataset->GetRasterCount();

	GDALRasterBand *poBand = poDataset->GetRasterBand( 1 );

	// Assume block size will be one raster line
//	int			 nBlockXSize, nBlockYSize;
//	poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

	// Check data type - it's either integer or float
	GDALDataType rtype = poBand->GetRasterDataType();
	if (rtype == GDT_Int16 || rtype == GDT_Byte)
		m_bFloatMode = false;
	else
		m_bFloatMode = true;

	// Check vertical units - if they're feet, scale to meters
	float fScale = 1.0f;
	const char *runits = poBand->GetUnitType();
	if (runits && !strcmp(runits, "ft"))
	{
		fScale = 0.3048f;	// feet to meter conversion
		m_bFloatMode = true;
	}

	AllocateArray();

	short *pasScanline;
	short elev;
	int   nXSize = poBand->GetXSize();

	pasScanline = (short *) CPLMalloc(sizeof(short)*nXSize);
	int i, j;
	for (j = 0; j < m_iRows; j++)
	{
		poBand->RasterIO( GF_Read, 0, j, nXSize, 1,
						  pasScanline, nXSize, 1, GDT_Int16,
						  0, 0 );
		for (i = 0; i < nXSize; i++)
		{
			elev = pasScanline[i];
			if (elev == -9999 || elev == -32766)
				SetValue(i, m_iRows-1-j, INVALID_ELEVATION);
			else
				SetFValue(i, m_iRows-1-j, elev * fScale);
		}
		if (progress_callback != NULL) // 100 * cell number we on div total number cells
			progress_callback(100*j/m_iRows);
	}

	// Clean up
	CPLFree(pasScanline);
	delete poDataset;

	// Return success
	return true;
}

/** Loads from a RAW file (a naked array of elevation values).
 * The file will not contain any information about at all about data size,
 * data type, or projection, so this information must be passed in as
 * arguments.
 *
 * \param szFileName The file name to read from.
 * \param width The width of the expected array.
 * \param height The height of the expected array.
 * \param bytes_per_element The number of bytes for each elevation value.  If
 * this is 1 or 2, the data is assumed to be integer.  If 4, then the data is
 * assumed to be floating-point values.
 * \param vertical_units Indicates what scale factor to apply to the elevation
 * values to convert them to meters.  E.g. if the file is in meters, pass 1.0,
 * if in feet, pass 0.3048.
 * \returns \c true if the file was successfully opened and read.
 */
bool vtElevationGrid::LoadFromRAW(const char *szFileName, int width, int height,
								int bytes_per_element, float vertical_units)
{
	FILE *fp = fopen(szFileName, "rb");
	if (!fp)
		return false;

	m_iColumns = width;
	m_iRows = height;

	// set extents arbitrarily for now; if the user knows them, they can set
	// them after loading
	m_area.left = 0;
	m_area.top = height;
	m_area.right = width;
	m_area.bottom = 0;
	ComputeCornersFromExtents();

	m_proj.SetProjectionSimple(true, 1, WGS_84);
	m_bFloatMode = true;

	AllocateArray();

	int i, j, z;
	void *data = &z;
	for (j = 0; j < m_iRows; j++)
	{
		for (i = 0; i < m_iColumns; i++)
		{
			if (bytes_per_element == 1)
			{
				fread(data, bytes_per_element, 1, fp);
				SetValue(i, m_iRows-1-j, *((unsigned char *)data));
			}
			if (bytes_per_element == 2)
			{
				FRead(data, DT_SHORT, 1, fp, BO_BIG_ENDIAN );
				SetValue(i, m_iRows-1-j, *((short *)data) * vertical_units);
			}
			if (bytes_per_element == 4)
			{
				FRead(data, DT_INT, 1, fp, BO_BIG_ENDIAN );
				SetFValue(i, m_iRows-1-j, *((int *)data) * vertical_units);
			}
		}
//		if (progress_callback != NULL)
//			progress_callback(100*j/m_iRows);
	}

	// Clean up
	fclose(fp);

	// Return success
	return true;
}

