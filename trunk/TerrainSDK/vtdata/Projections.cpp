//
// Projections.cpp
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//
// Parts of the code are derived from public-domain USGS software.
//

#include "Projections.h"
#include "StatePlane.h"
#include "MathTypes.h"
#include "vtString.h"	// for stricmp
#include "vtLog.h"

/**
 * Enumeration of the Datum types
 *
 * This list of Datums was originally from the USGS "ProjectionLib" library
 * and provides a more concise way to represent a Datum than the string
 * values used by OGRSpatialReference.
 */
enum DATUM { ADINDAN = 0, ARC1950, ARC1960, AUSTRALIAN_GEODETIC_1966,
			 AUSTRALIAN_GEODETIC_1984, CAMP_AREA_ASTRO, CAPE,
			 EUROPEAN_DATUM_1950, EUROPEAN_DATUM_1979, GEODETIC_DATUM_1949,
			 HONG_KONG_1963, HU_TZU_SHAN, INDIAN, NAD27, NAD83,
			 OLD_HAWAIIAN_MEAN, OMAN, ORDNANCE_SURVEY_1936, PUERTO_RICO,
			 PULKOVO_1942, PROVISIONAL_S_AMERICAN_1956, TOKYO, WGS_72, WGS_84,
			 UNKNOWN_DATUM = -1, NO_DATUM = -2, DEFAULT_DATUM = -3 };

const char *datumToString(DATUM d);	// old function
const char *datumToStringShort(DATUM d); // old function
static void WKTMassageDatum(vtString &strDatum );
static void MassageDatumFromWKT(vtString &strDatum );

/////////////////////////////////////////////////////////////////////////////
// Implementation of class vtProjection
//

vtProjection::vtProjection() : OGRSpatialReference()
{
}

/**
 * Assignment operator.
 */
vtProjection &vtProjection::operator=(const vtProjection &ref)
{
	// copy new projection
	if (ref.GetRoot() != NULL)
	{
		OGRSpatialReference *ref_copy = ref.Clone();
		(*(OGRSpatialReference *)this) = *ref_copy;
	}
	return *this;
}

/**
 * Equality operator.
 */
bool vtProjection::operator==(const vtProjection &ref)
{
	// Work around problem in IsSame, by detecting this type of difference
	if( IsProjected() != ref.IsProjected() )
		return false;
	return (IsSame( (OGRSpatialReference *) &ref ) != 0);
}

/**
 * Get the UTM zone of the projection.
 *
 * \param iZone
 * Should be one of the following values:
	- 1 through 60 for the northern hemisphere
	- -1 through -60 for the southern hemisphere
 */
void vtProjection::SetUTMZone(int iZone)
{
	// reset the name of the projection so that SetUTM() will set it
	SetProjCS("unnamed");

	// Northern Hemisphere for positive zone numbers
	if (iZone > 0)
		SetUTM( iZone, TRUE);
	else
		SetUTM( -iZone, FALSE);
}

/**
 * Get the UTM zone of the projection.
 *
 * \return
 *	- 1 through 60 in the northern hemisphere
 *  - -1 through -60 for the southern hemisphere
 *  - 0 if the projection is not UTM
 */
int	vtProjection::GetUTMZone() const
{
	int north;
	int zone = OGRSpatialReference::GetUTMZone(&north);
	if (north)
		return zone;
	else
		return -zone;
}


/**
 * Set the datum to an EPSG Datum code, a value in the range (6120 - 6904).
 */
void vtProjection::SetDatum(int iDatum)
{
	// OGR does not have functionality to change the Datum of an
	// existing coordinate system.
	//
	// Try to fake it by just changing the DATUM node.  This is not
	// good enough for all purposes, since it doesn't change the
	// underlying properties such as spheroid, but it appears to suffice
	// to make coordinate transformations work (which use PROJ.4)

	if (IsGeographic())
	{
		// re-create the object with the new datum
		SetGeogCSFromDatum(iDatum);
	}
	else if (iDatum < 24)
	{
		DATUM datum = (DATUM) iDatum;
		// Convert the DATUM enumeration to a Datum string
		OGR_SRSNode *dnode = GetAttrNode("DATUM");
		if (!dnode)
			return;
		switch (datum)
		{
			case AUSTRALIAN_GEODETIC_1966:
				dnode->GetChild(0)->SetValue("Australian_Geodetic_Datum_1966"); break;
			case AUSTRALIAN_GEODETIC_1984:
				dnode->GetChild(0)->SetValue("Australian_Geodetic_Datum_1984"); break;
			case EUROPEAN_DATUM_1950:
				dnode->GetChild(0)->SetValue("European_Datum_1950"); break;
			case NAD27:
				dnode->GetChild(0)->SetValue("North_American_Datum_1927"); break;
			case NAD83:
				dnode->GetChild(0)->SetValue("North_American_Datum_1983"); break;
			case OLD_HAWAIIAN_MEAN:
				dnode->GetChild(0)->SetValue("Old_Hawaiian"); break;
			case ORDNANCE_SURVEY_1936:
				dnode->GetChild(0)->SetValue("OSGB_1936"); break;
			case PUERTO_RICO:
				dnode->GetChild(0)->SetValue("Puerto_Rico"); break;
			case WGS_72:
				dnode->GetChild(0)->SetValue("WGS_1972"); break;
			default:
			case WGS_84:
				dnode->GetChild(0)->SetValue("WGS_1984"); break;
		}
	}
	else
	{
		OGR_SRSNode *dnode = GetAttrNode("DATUM");
		if (!dnode)
			return;
		vtString str = DatumToString(iDatum);
		WKTMassageDatum(str);
		dnode->GetChild(0)->SetValue((const char *)str);
	}
	// TODO: also change SPHEROID node to match desired DATUM?
//	OGR_SRSNode *enode1 = pSource->GetAttrNode("SPHEROID");
}

/**
 * Return the datum as an EPSG code (an integer in the range of 6120 - 6904),
 * or -1 if the datum could not be determined.
 */
int vtProjection::GetDatum()
{
	// Convert new DATUM string to old Datum enum
	const char *datum_string = GetAttrValue("DATUM");
	if (!datum_string)
		return -1;

	vtString strDatum = datum_string;
	MassageDatumFromWKT(strDatum);	// Convert WKT name to EPSG name

	for (int i = 0; i < g_EPSGDatums.GetSize(); i++)
	{
		if (!strcmp((const char *)strDatum, g_EPSGDatums[i].szName))
			return g_EPSGDatums[i].iCode;
	}
	return -1;
}

/**
 * Return the kind of horizontal units used by the projection.  This is
 * also called "linear units."
 *
 * \return
	- LU_DEGREES - Arc Degrees
	- LU_METERS - Meters
	- LU_FEET_INT - Feet (International Foot)
	- LU_FEET_US - Feet (U.S. Survey Foot)
 */
LinearUnits vtProjection::GetUnits()
{
	if( IsGeographic() )
		return LU_DEGREES;  // degrees

	// Get horizontal units ("linear units")
	char *pszLinearUnits;
	double dfLinearConv = GetLinearUnits(&pszLinearUnits);
	double diff;

	diff = dfLinearConv - 0.3048;
	if( EQUAL(pszLinearUnits,SRS_UL_FOOT) || fabs(diff) < 0.000000001)
		return LU_FEET_INT;  // international feet

	diff = dfLinearConv - (1200.0/3937.0);
	if( EQUAL(pszLinearUnits,SRS_UL_US_FOOT) || fabs(diff) < 0.000000001)
		return LU_FEET_US;  // u.s. survey feet

	if( dfLinearConv == 1.0 )
		return LU_METERS;  // meters

	return LU_METERS;	// can't guess; assume meters
}


/**
 * Set the projection by copying from a OGRSpatialReference.
 */
void vtProjection::SetSpatialReference(OGRSpatialReference *pRef)
{
	*((OGRSpatialReference *)this) = *(pRef->Clone());
}

/**
 * Return a string describing the type of projection.
 *
 * \par Example:
 *	"Geographic", "Transverse_Mercator", "Albers_Conic_Equal_Area"
 */
const char *vtProjection::GetProjectionName() const
{
	const char *proj_string = GetAttrValue("PROJECTION");
	if (!proj_string)
		return "Geographic";
	else
		return proj_string;
}

/**
 * Return a very short string describing the type of projection.
 * \par
 * Possible values are "Geo", "UTM", "TM", "Albers", "LCC", "Other", or "Unknown"
 */
const char *vtProjection::GetProjectionNameShort() const
{
	if (IsGeographic())
		return "Geo";
	const char *proj_string = GetAttrValue("PROJECTION");
	if (!proj_string)
		return "Unknown";
	if (!strcmp(proj_string, SRS_PT_TRANSVERSE_MERCATOR))
	{
		if (GetUTMZone() != 0)
			return "UTM";
		else
			return "TM";
	}
	if (!strcmp(proj_string, SRS_PT_ALBERS_CONIC_EQUAL_AREA))
		return "Albers";
	if (!strncmp(proj_string, "Lambert_Conformal_Conic", 23))
		return "LCC";
	return "Other";
}


/**
 * Set the projection to a fresh, new geographical coordinate system
 * based on the indicated Datum.
 */
void vtProjection::SetGeogCSFromDatum(int iDatum)
{
	Clear();

	// support old USGS datums for backward compatibility
	if (iDatum < 24)
	{
		DATUM eDatum = (DATUM) iDatum;
		switch (eDatum)
		{
			case ADINDAN:			SetWellKnownGeogCS( "EPSG:4201" ); break;
			case ARC1950:			SetWellKnownGeogCS( "EPSG:4209" ); break;
			case ARC1960:			SetWellKnownGeogCS( "EPSG:4210" ); break;
			case AUSTRALIAN_GEODETIC_1966: SetWellKnownGeogCS( "EPSG:4202" ); break;
			case AUSTRALIAN_GEODETIC_1984: SetWellKnownGeogCS( "EPSG:4203" ); break;
			case CAPE:				SetWellKnownGeogCS( "EPSG:4222" ); break;
			case EUROPEAN_DATUM_1950: SetWellKnownGeogCS( "EPSG:4230" ); break;
			case GEODETIC_DATUM_1949: SetWellKnownGeogCS( "EPSG:4272" ); break;
			case HU_TZU_SHAN:		SetWellKnownGeogCS( "EPSG:4236" ); break;
		//	case INDIAN:			SetWellKnownGeogCS( "EPSG:" ); break;	// there are 3 Indian Datum
			case NAD27:				SetWellKnownGeogCS( "NAD27" ); break;
			case NAD83:				SetWellKnownGeogCS( "NAD83" ); break;

			case OLD_HAWAIIAN_MEAN: SetWellKnownGeogCS( "EPSG:4135" ); break;
			case OMAN:				SetWellKnownGeogCS( "EPSG:4232" ); break;	// Fahud
			case ORDNANCE_SURVEY_1936: SetWellKnownGeogCS( "EPSG:4277" ); break;
			case PUERTO_RICO:		SetWellKnownGeogCS( "EPSG:4139" ); break;
			case PULKOVO_1942:		SetWellKnownGeogCS( "EPSG:4284" ); break;
			case PROVISIONAL_S_AMERICAN_1956: SetWellKnownGeogCS( "EPSG:4248" ); break;
			case TOKYO:				SetWellKnownGeogCS( "EPSG:4301" ); break;

			case WGS_72:			SetWellKnownGeogCS( "WGS72" ); break;
			case WGS_84:			SetWellKnownGeogCS( "WGS84" ); break;
		}
		SetWellKnownGeogCS( "WGS84" );
		return;
	}

	// Turn the datum into a coord system
	int iCoordSystem = iDatum - 2000;
	char name[12];
	sprintf(name, "EPSG:%d", iCoordSystem);
	SetWellKnownGeogCS(name);
}


/**
 * Convenient way to set a simple projection.
 *
 * \param bUTM true for UTM, false for Geographic.
 * \param iUTMZone If UTM, this is the zone: 1 through 60 in the northern
 *		hemisphere, -1 through -60 for the southern hemisphere.
 * \param iDatum The Datum as either an old USGS code or an EPSG code
 */
void vtProjection::SetProjectionSimple(bool bUTM, int iUTMZone, int iDatum)
{
	SetGeogCSFromDatum(iDatum);
	if (bUTM)
		SetUTMZone(iUTMZone);
}

/**
 * Get the projection as a text description.  If the projection is Geographic
 * or UTM, then a "simple" type string will be returned.  For all other
 * projection types, a WKT string is returned.
 *
 * \param type A string buffer to contain the type of description.
 * This buffer should be at least 7 characters long to contain either the
 * word "simple" or "wkt".
 *
 * \param value A string buffer to contain the full description.
 * This buffer should be at least 2048 characters long to contain either
 * a simple or WKT description.
 */
bool vtProjection::GetTextDescription(char *type, char *value)
{
	int datum = GetDatum();
	const char *datum_string = DatumToStringShort(datum);
	int zone = GetUTMZone();

	if (IsGeographic())
	{
		strcpy(type, "simple");
		sprintf(value, "geo, datum %s", datum_string);
	}
	else if (zone != 0)
	{
		strcpy(type, "simple");
		sprintf(value, "utm, datum %s, zone %d", datum_string, zone);
	}
	else
	{
		// Something less common, must use full WTK representation
		strcpy(type, "wkt");

		char *wkt;
		OGRErr err = exportToWkt(&wkt);
		if (err != OGRERR_NONE)
			return false;
		strcpy(value, wkt);
		OGRFree(wkt);
	}
	return true;
}

/**
 * Set the projection using a text description.
 *
 * \param type The type of description, either "simple" for short simple
 * string, or "wkt" for a full-length WKT (Well-Known Text) description.
 *
 * \param value The description itself.  A WKT description should be a
 * single string, with no extra whitespace.  A simple string can have the
 * following forms:
 *		- geo, datum D
 *		- utm, datum D, zone Z
 *
 * \par Example:
	\code
	proj.SetTextDescription("simple", "utm, datum WGS_84, zone 11");
	\endcode
 */
bool vtProjection::SetTextDescription(const char *type, const char *value)
{
	if (!strcmp(type, "simple"))
	{
		char datum[60];
		int iUTMZone;

		if (!strncmp(value, "geo", 3))
		{
			sscanf(value, "geo, datum %s", datum);
			SetWellKnownGeogCS(datum);
			return true;
		}
		else if (!strncmp(value, "utm", 3))
		{
			sscanf(value, "utm, datum %s zone %d", datum, &iUTMZone);
			if (datum[strlen(datum)-1] == ',')
				datum[strlen(datum)-1] = 0;
			SetWellKnownGeogCS(datum);
			SetUTMZone(iUTMZone);
			return true;
		}
	}
	else if (!strcmp(type, "wkt"))
	{
		char wkt_buf[2000], *wkt = wkt_buf;
		strcpy(wkt, value);
		OGRErr err = importFromWkt((char **) &wkt);
		return (err == OGRERR_NONE);
	}
	return false;
}

/**
 * Read the projection from a .prj file.
 *
 * If the filename does not have the file extension ".prj", this
 * method will look for a file which has the same name with a
 * ".prj" extension.
 *
 * \return true if successful.
 */
bool vtProjection::ReadProjFile(const char *filename)
{
	char prj_name[256];
	int len = strlen(filename);

	// check file extension
	if (len >= 4 && !stricmp(filename + len - 4, ".prj"))
	{
		strcpy(prj_name, filename);
	}
	else
	{
		strcpy(prj_name, filename);
		char *dot = strrchr(prj_name, '.');
		if (dot)
			strcpy(dot, ".prj");
		else
			strcat(prj_name, ".prj");
	}

	FILE *fp2 = fopen(prj_name, "rb");
	if (!fp2)
		return false;
	char wkt_buf[2000], *wkt = wkt_buf;
	fgets(wkt, 2000, fp2);
	fclose(fp2);
	OGRErr err = importFromWkt((char **) &wkt);
	if (err != OGRERR_NONE)
		return false;
	return true;
}


/**
 * Write the projection to a .prj file.
 *
 * \return true if successful.
 */
bool vtProjection::WriteProjFile(const char *filename)
{
	FILE *fp2 = fopen(filename, "wb");
	if (!fp2)
		return false;
	char *wkt;
	OGRErr err = exportToWkt(&wkt);
	if (err != OGRERR_NONE)
		return false;
	fprintf(fp2, "%s\n", wkt);
	fclose(fp2);
	OGRFree(wkt);
	return true;
}

/**
 * Given two geographic coordinates (longitude/latitude in degrees),
 * return the geodesic arc distance in meters.  The WGS84 spheroid
 * is used.
 */
double vtProjection::GeodesicDistance(const DPoint2 &geo1, DPoint2 &geo2,
	bool bQuick)
{
	if (bQuick)
	{
		// when the user cares more about speed than accuracy, just do
		// the quick calculation assuming the earth is a sphere
		// (TODO if performance of full calculation ever proves a bottleneck)
	}

	// We don't have direct access to the PROJ.4 library from this module,
	// so we can't set the exact coordinate system (in particular, the
	// spheroid) using exportToProj4() and pj_init().
	//
	// Instead, fill in the values directly for WGS84, which in practice
	// shouldn't give distance values significantly different from other
	// spheroids.

	Geodesic gd;
	gd.a = 6378137.0000000;
	gd.onef = 0.99664718933525;
	gd.f = 1.0 - gd.onef;
	gd.f2 = gd.f / 2.0;
	gd.f4 = gd.f / 4.0;
	gd.f64 = gd.f * gd.f / 64.0;

	// Now fill in the start and end points, convert to lon/lat in radians
	gd.lam1 = geo1.x / 180.0 * PId;
	gd.phi1 = geo1.y / 180.0 * PId;
	gd.lam2 = geo2.x / 180.0 * PId;
	gd.phi2 = geo2.y / 180.0 * PId;

	gd.CalculateInverse();
	return gd.S;
}

/////////////////////////////////////////////////////////////////////////////
// Helper functions


/**
 * Given a non-geographic projection, produce a geographic projection which
 * has the same datum/ellipsoid values.
 */
void CreateSimilarGeographicProjection(const vtProjection &source,
	vtProjection &geo)
{
	geo.SetWellKnownGeogCS("WGS84");

	// We can't convert datum yet.  Force assumption that source datum
	// is the same as the destination.
	const char *datum_string = source.GetAttrValue("DATUM");
	const char *ellipsoid_string = source.GetAttrValue("SPHEROID");
	geo.SetGeogCS("WGS84", datum_string, ellipsoid_string,
		source.GetSemiMajor(), source.GetInvFlattening());
}


/**
 * Provides access to the State Plane Table
 */
StatePlaneInfo *GetStatePlaneTable()
{
	return g_StatePlaneInfo;
}

int GetNumStatePlanes()
{
	return sizeof(g_StatePlaneInfo) /  sizeof(StatePlaneInfo);
}


const char *datumToString(DATUM d)
{
	switch ( d )
	{
		case ADINDAN:
			return "ADINDAN";
		case ARC1950:
			return "ARC1950";
		case ARC1960:
			return "ARC1960";
		case AUSTRALIAN_GEODETIC_1966:
			return "AUSTRALIAN GEODETIC 1966";
		case AUSTRALIAN_GEODETIC_1984:
			return "AUSTRALIAN GEODETIC 1984";
		case CAMP_AREA_ASTRO:
			return "CAMP AREA ASTRO";
		case CAPE:
			return "CAPE";
		case EUROPEAN_DATUM_1950:
			return "EUROPEAN DATUM 1950";
		case EUROPEAN_DATUM_1979:
			return "EUROPEAN DATUM 1979";
		case GEODETIC_DATUM_1949:
			return "GEODETIC DATUM 1949";
		case HONG_KONG_1963:
			return "HONG KONG 1963";
		case HU_TZU_SHAN:
			return "HU TZU SHAN";
		case INDIAN:
			return "INDIAN";
		case NAD27:
			return "NAD27";
		case NAD83:
			return "NAD83";
		case OLD_HAWAIIAN_MEAN:
			return "OLD HAWAIIAN MEAN";
		case OMAN:
			return "OMAN";
		case ORDNANCE_SURVEY_1936:
			return "ORDNANCE SURVEY 1936";
		case PUERTO_RICO:
			return "PUERTO RICO";
		case PULKOVO_1942:
			return "PULKOVO 1942";
		case PROVISIONAL_S_AMERICAN_1956:
			return "PROVISIONAL SOUTH AMERICAN 1956";
		case TOKYO:
			return "TOKYO";
		case WGS_72:
			return "WGS72";
		case WGS_84:
			return "WGS84";
		case UNKNOWN_DATUM:
			return "Unknown";
		case NO_DATUM:
			return "None";
		case DEFAULT_DATUM:
			return "Default";
		default:
			return "Bad";
	}
}
/**
 * Convert an a Datum Code to a string of the Datum Name.
 */
const char *DatumToString(int d)
{
	if (d < 24)
		return datumToString((DATUM)d);	// allow backward compatibility

	for (int i = 0; i < g_EPSGDatums.GetSize(); i++)
	{
		if (g_EPSGDatums[i].iCode == d)
			return g_EPSGDatums[i].szName;
	}
	return "Bad";
}


const char *datumToStringShort(DATUM d)
{
	switch ( d )
	{
		case ADINDAN:
			return "ADINDAN";
		case ARC1950:
			return "ARC1950";
		case ARC1960:
			return "ARC1960";
		case AUSTRALIAN_GEODETIC_1966:
			return "AGD66";
		case AUSTRALIAN_GEODETIC_1984:
			return "AGD84";
		case CAMP_AREA_ASTRO:
			return "CAMP AREA ASTRO";
		case CAPE:
			return "CAPE";
		case EUROPEAN_DATUM_1950:
			return "ED50";
		case EUROPEAN_DATUM_1979:
			return "ED79";
		case GEODETIC_DATUM_1949:
			return "GD49";
		case HONG_KONG_1963:
			return "HONG KONG 1963";
		case HU_TZU_SHAN:
			return "HU TZU SHAN";
		case INDIAN:
			return "INDIAN";
		case NAD27:
			return "NAD27";
		case NAD83:
			return "NAD83";
		case OLD_HAWAIIAN_MEAN:
			return "OLD HAWAII";
		case OMAN:
			return "OMAN";
		case ORDNANCE_SURVEY_1936:
			return "OSGB 1936";
		case PUERTO_RICO:
			return "PUERTO RICO";
		case PULKOVO_1942:
			return "PULKOVO 1942";
		case PROVISIONAL_S_AMERICAN_1956:
			return "PSAD 1956";
		case TOKYO:
			return "TOKYO";
		case WGS_72:
			return "WGS72";
		case WGS_84:
			return "WGS84";
		case UNKNOWN_DATUM:
			return "Unknown";
		case NO_DATUM:
			return "None";
		case DEFAULT_DATUM:
			return "Default";
		default:
			return "Bad";
	}
}
/**
 * Convert a Datum Code to a (short) string of the Datum Name.
 */
const char *DatumToStringShort(int d)
{
	if (d < 24)
		return datumToStringShort((DATUM)d); // allow backward compatibility

	for (int i = 0; i < g_EPSGDatums.GetSize(); i++)
	{
		if (g_EPSGDatums[i].iCode == d)
		{
			const char *name = g_EPSGDatums[i].szShortName;
			if (*name == '\0')
				name = g_EPSGDatums[i].szName;
			return name;
		}
	}
	return "Bad";
}

#include "EPSG_Datums.h"

Array<EPSGDatum> g_EPSGDatums;

/**
 * Setup the global array of EPSG Datums, accessible as g_EPSGDatums.
 */
void SetupEPSGDatums()
{
	// Copy from the statically initialized C array into our more
	//  robust C++ array.
	int count = sizeof(epsg_datums) / sizeof(epsg_datum);
	EPSGDatum dat;

	for (int i = 0; i < count; i++)
	{
		dat.bCommon = epsg_datums[i].common != 0;
		dat.iCode = epsg_datums[i].code;
		dat.szName = epsg_datums[i].name;
		dat.szShortName = epsg_datums[i].shortname;
		g_EPSGDatums.Append(dat);
	}
}

/**
 * Determine an approximate conversion from degrees of longitude to meters,
 * given a latitude in degrees.
 */
double EstimateDegreesToMeters(double latitude)
{
	// estimate meters per degree of longitude, using the terrain origin
	double r0 = EARTH_RADIUS * cos(latitude / 180.0 * PId);
	double circ = 2.0 * r0 * PId;
	return circ / 360.0f;
}

/**
 * Create a conversion between projections, making the assumption that
 * the Datum of the target is the same as the Datum of the source.
 */
OCT *CreateConversionIgnoringDatum(const vtProjection *pSource, vtProjection *pTarget)
{
	vtProjection DummyTarget = *pTarget;

	const char *datum_string = pSource->GetAttrValue("DATUM");

	OGR_SRSNode *dnode = DummyTarget.GetAttrNode("DATUM");
	if (dnode)
		dnode->GetChild(0)->SetValue(datum_string);

	const OGR_SRSNode *enode1 = pSource->GetAttrNode("SPHEROID");
	OGR_SRSNode *enode2 = DummyTarget.GetAttrNode("SPHEROID");
	if (enode1 && enode2)
	{
		enode2->GetChild(0)->SetValue(enode1->GetChild(0)->GetValue());
		enode2->GetChild(1)->SetValue(enode1->GetChild(1)->GetValue());
		enode2->GetChild(2)->SetValue(enode1->GetChild(2)->GetValue());
	}

#if DEBUG && 0
	// Debug: Check texts in PROJ4
	char *str3, *str4;
	pSource->exportToProj4(&str3);
	DummyTarget.exportToProj4(&str4);

	char *wkt1, *wkt2;
	OGRErr err = ((vtProjection *)pSource)->exportToWkt(&wkt1);
	err = DummyTarget.exportToWkt(&wkt2);

	OGRFree(wkt1);
	OGRFree(wkt2);
#endif

	return OGRCreateCoordinateTransformation((OGRSpatialReference *)pSource,
		(OGRSpatialReference *)&DummyTarget);
}

OCT *CreateCoordTransform(const vtProjection *pSource,
						  const vtProjection *pTarget, bool bLog)
{
	if (bLog)
	{
		// display debugging information to the log
		char *wkt1, *wkt2;
		OGRErr err = ((vtProjection *)pSource)->exportToWkt(&wkt1);
		err = ((vtProjection *)pTarget)->exportToWkt(&wkt2);
		VTLOG(" Converting from: %s\n", wkt1);
		VTLOG("   Converting to: %s\n", wkt2);
		OGRFree(wkt1);
		OGRFree(wkt2);
	}
	OCT *result = OGRCreateCoordinateTransformation((OGRSpatialReference *)pSource,
		(OGRSpatialReference *)pTarget);
	if (bLog)
	{
		VTLOG(" Conversion: %s\n", result ? "succeeded" : "failed");
	}
	return result;
}


double GetMetersPerUnit(LinearUnits lu)
{
	switch (lu)
	{
		case LU_DEGREES:
			return 1.0;		// actually no definition for degrees -> meters
		case LU_METERS:
			return 1.0;		// meters per meter
		case LU_FEET_INT:
			return 0.3048;		// international foot
		case LU_FEET_US:
			return (1200.0/3937.0);	// U.S. survey foot
	}
	return 1.0;
};


static const char *papszDatumEquiv[] =
{
	"Militar_Geographische_Institut",
	"Militar_Geographische_Institute",
	"World_Geodetic_System_1984",
	"WGS_1984",
	"WGS_72_Transit_Broadcast_Ephemeris",
	"WGS_1972_Transit_Broadcast_Ephemeris",
	"World_Geodetic_System_1972",
	"WGS_1972",
	"European_Terrestrial_Reference_System_89",
	"European_Reference_System_1989",
	NULL
};

/**
 * WKTMassageDatum()
 *
 * Massage an EPSG datum name into WKT format.  Also transform specific
 * exception cases into WKT versions.
 *
 * Origin of this function: GDAL/OGR, ogr_fromepsg.cpp, Revision 1.23
 */
static void WKTMassageDatum(vtString &strDatum )
{
	char		szDatum[80];
	strcpy(szDatum, (const char *)strDatum);
	char		*pszDatum = szDatum;
	int		 i, j;

/* -------------------------------------------------------------------- */
/*	  Translate non-alphanumeric values to underscores.			   */
/* -------------------------------------------------------------------- */
	for( i = 0; pszDatum[i] != '\0'; i++ )
	{
		if( !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
			&& !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
			&& !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
		{
			pszDatum[i] = '_';
		}
	}

/* -------------------------------------------------------------------- */
/*	  Remove repeated and trailing underscores.					   */
/* -------------------------------------------------------------------- */
	for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
	{
		if( pszDatum[j] == '_' && pszDatum[i] == '_' )
			continue;

		pszDatum[++j] = pszDatum[i];
	}
	if( pszDatum[j] == '_' )
		pszDatum[j] = '\0';
	else
		pszDatum[j+1] = '\0';
	strDatum = pszDatum;

/* -------------------------------------------------------------------- */
/*	  Search for datum equivelences.  Specific massaged names get	 */
/*	  mapped to OpenGIS specified names.							  */
/* -------------------------------------------------------------------- */
	for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
	{
		if( !strcmp(pszDatum,papszDatumEquiv[i]) )
		{
			strDatum = papszDatumEquiv[i+1];
			break;
		}
	}
}

//
// A limited implementation of reversing the effect of WKTMassageDatum
//
static void MassageDatumFromWKT(vtString &strDatum )
{
	int i;
	for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
	{
		if( !strcmp((const char *)strDatum,papszDatumEquiv[i+1]) )
		{
			strDatum = papszDatumEquiv[i];
			break;
		}
	}
	int len = strDatum.GetLength();
	for (i = 0; i < len; i++)
	{
		if (strDatum.GetAt(i) == '_')
			strDatum.SetAt(i, ' ');
	}
}


