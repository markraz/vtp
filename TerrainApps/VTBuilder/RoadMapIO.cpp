//
// RoadMapEdit.cpp
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "vtdata/shapelib/shapefil.h"

#include "RoadMapEdit.h"
#include "assert.h"

#include "ogrsf_frmts.h"

#define BUFFER_SIZE		8000
#define MAX_SEGMENT_LENGTH	80.0						// in meters

void RoadMapEdit::ApplyDLGAttributes(int road_type, int &lanes,
									 SurfaceType &stype, int &priority)
{
	switch (road_type)
	{
	case -201:
	case -202:
		stype = SURFT_RAILROAD;
		lanes = 1;
		priority = 1;
		break;
	case 201:	//	Primary route, class 1, symbol undivided
	case 202:	//	Primary route, class 1, symbol divided by centerline
	case 203:	//	Primary route, class 1, divided, lanes separated
	case 204:	//	Primary route, class 1, one way, other than divided highway
		stype = SURFT_PAVED;
		lanes = 4;
		priority = 1;
		break;
	case 205:	//	Secondary route, class 2, symbol undivided
	case 206:	//	Secondary route, class 2, symbol divided by centerline
	case 207:	//	Secondary route, class 2, symbol divided, lanes separated
	case 208:	//	Secondary route, class 2, one way, other then divided highway
		stype = SURFT_PAVED;
		lanes = 2;
		priority = 2;
		break;
	case 209:	//	Road or street, class 3
	case 217:	//	Road or street, class 3, symbol divided by centerline
	case 218:	//	Road or street, class 3, divided lanes separated
	case 221:	//	Road in street, class 3, one way
	case 222:	//  Road in transition
	case 223:	//  Road in service facility, rest area or viewpoint.
	case 5:		//  cul-de-sac
	case 405:	//  non-standard section of road...???
		stype = SURFT_PAVED;
		lanes = 2;
		priority = 3;
		break;
	case 210:	//	Road or street, class 4
	case 219:	//  Road or street, class 4, 1-way
		stype = SURFT_DIRT;
		lanes = 2;
		priority = 5;
		break;
	case 402:	//	Ramp in interchange
		stype = SURFT_PAVED;
		lanes = 1;
		priority = 4;
		break;
	case 211:	//	Trail, class 5, other than four-wheel drive vehicle
		stype = SURFT_TRAIL;
		lanes = 1;
		priority = 10;
		break;
	case 212:	//	Trail, class 5, four-wheel-drive vehicle
		stype = SURFT_2TRACK;
		lanes = 1;
		priority = 6;
		break;
	case 213:	//	Footbridge
		break;
	}
}

bool RoadMapEdit::attribute_filter_roads(DLGLine *pLine, int &lanes,
										 SurfaceType &stype, int &priority)
{
	// check to see if there is an attribute for road type
	int road_type = 0;
	for (int j = 0; j < pLine->m_iAttribs; j++)
	{
		if (pLine->m_attr[j].m_iMajorAttr == 170 &&
			((pLine->m_attr[j].m_iMinorAttr >= 201 && pLine->m_attr[j].m_iMinorAttr <= 213) ||
			(pLine->m_attr[j].m_iMinorAttr >= 217 && pLine->m_attr[j].m_iMinorAttr <= 222) ||
			(pLine->m_attr[j].m_iMinorAttr >= 401 && pLine->m_attr[j].m_iMinorAttr <= 405))
			)
		{
			road_type = pLine->m_attr[j].m_iMinorAttr;
			break;
		}
		if (pLine->m_attr[j].m_iMajorAttr == 180 && 
			(pLine->m_attr[j].m_iMinorAttr == 201 ||
			 pLine->m_attr[j].m_iMinorAttr == 202)
			) {
			road_type = -pLine->m_attr[j].m_iMinorAttr;
		}
	}
	stype = SURFT_NONE;
	priority = 0;

	ApplyDLGAttributes(road_type, lanes, stype, priority);
	return (stype != SURFT_NONE);
}


void RoadMapEdit::AddElementsFromDLG(vtDLGFile *pDlg)
{
	int i, j, lanes;
	SurfaceType stype;
	DPoint2 buffer[BUFFER_SIZE];

	// set projection
	m_proj = pDlg->GetProjection();

	// expand extents to include the new DLG
	if (!m_bValidExtents)
	{
		// unitialized
		m_extents.right = pDlg->m_NE_utm.x;
		m_extents.top = pDlg->m_NE_utm.y;
		m_extents.left = pDlg->m_SW_utm.x;
		m_extents.bottom = pDlg->m_SW_utm.y;
		m_bValidExtents = true;
	}
	else
	{
		// expand extents
		m_extents.GrowToContainPoint(pDlg->m_SW_utm);
		m_extents.GrowToContainPoint(pDlg->m_NE_utm);
	}

	//an array to allow for fast lookup
	NodeEditPtr *pNodeLookup = new NodeEditPtr[pDlg->m_iNodes+1];
	int id = 1;

	NodeEdit *pN;
	for (i = 0; i < pDlg->m_iNodes; i++)
	{
		DLGNode *pDNode = pDlg->m_nodes + i;

		// create new node
		pN = new NodeEdit();
		pN->m_id = id++;
		pN->m_p = pDNode->m_p;

		AddNode(pN);

		//add to array
		pNodeLookup[pN->m_id] = pN;
	}
	LinkEdit *pR;
	int count = 0;
	for (i = 0; i < pDlg->m_iLines; i++)
	{
		DLGLine *pDLine = pDlg->m_lines + i;
		
		int priority;
		bool result = false;
		result = attribute_filter_roads(pDLine, lanes, stype, priority);
		if (!result)
			continue;

		// create new road
		pR = new LinkEdit();
		pR->m_Surface = stype;
		pR->m_iLanes = lanes;
		pR->m_iPriority = priority;

		// copy data from DLG line
		pR->SetNode(0, pNodeLookup[pDLine->m_iNode1]);
		pR->SetNode(1, pNodeLookup[pDLine->m_iNode2]);
		
		int actual_coords = 0;
		for (j = 0; j < pDLine->m_iCoords; j++)
		{
			// safety check
			assert(actual_coords < BUFFER_SIZE);
			if (j > 0)
			{
				//
				// check how long this segment is
				//
				DPoint2 delta = pDLine->m_p[j] - pDLine->m_p[j-1];
				double length = delta.Length();

				//
				// if it's too long, chop it up into a series of smaller segments,
				// by adding more points
				//
				if (length > MAX_SEGMENT_LENGTH)
				{
					int splits = (int) (length / MAX_SEGMENT_LENGTH);
					double step = 1.0 / (splits+1);
					for (double amount = step; amount <= 0.999; amount += step)
					{
						buffer[actual_coords] = (pDLine->m_p[j-1] + (delta * amount));
						actual_coords++;
					}
				}
			}
			buffer[actual_coords] = pDLine->m_p[j];
			actual_coords++;
		}
		pR->SetSize(actual_coords);
		for (j = 0; j < actual_coords; j++)
			pR->SetAt(j, buffer[j]);

		//set bounding box for the road
		pR->ComputeExtent();

		pR->m_iHwy = pDLine->HighwayNumber();

		// add to list
		AddLink(pR);

		// inform the Nodes to which it belongs
		pR->GetNode(0)->AddLink(pR);
		pR->GetNode(1)->AddLink(pR);
		pR->m_fLength = pR->Length();	
	}
	
	//delete the lookup array.
	delete pNodeLookup;

	GuessIntersectionTypes();
}

//
// Guess and add some intersection behaviors
//
void RoadMapEdit::GuessIntersectionTypes()
{
	int i;
	NodeEdit *pN;

	pN = GetFirstNode();
	LinkEdit* curRoad;
	while (pN)
	{
		if (pN->m_iLinks <= 2)
		{
			pN->SetVisual(VIT_NONE);
			if (pN->m_iLinks > 0)
				assert(pN->SetIntersectType(0,IT_NONE));
			if (pN->m_iLinks == 2)
				assert(pN->SetIntersectType(1,IT_NONE));
		}
		else
		{
			int topPriority = ((LinkEdit*)(pN->GetLink(0)))->m_iPriority;
			int topCount = 0;
			int lowPriority = topPriority;

			//analyze the roads intersecting at the node
			for (i = 0; i < pN->m_iLinks; i++)
			{
				curRoad = (LinkEdit*)(pN->GetLink(i));
				if (curRoad->m_iPriority == topPriority) {
					topCount++;
				} else if (curRoad->m_iPriority < topPriority) {
					topCount = 1;
					topPriority = curRoad->m_iPriority;
				} else if (curRoad->m_iPriority > lowPriority) {
					lowPriority = curRoad->m_iPriority;
				}
			}

			IntersectionType bType;
			//all roads have same priority
			if (topCount == pN->m_iLinks)
			{
				if (topPriority <= 2) {
					//big roads.  use lights
					pN->SetVisual(VIT_ALLLIGHTS);
					bType = IT_LIGHT;
				} else if (topPriority >= 5) {
					//dirt roads.  uncontrolled
					pN->SetVisual(VIT_NONE);
					bType = IT_NONE;
				} else {
					//smaller roads, use stop signs
					bType = IT_STOPSIGN;
					pN->SetVisual(VIT_ALLSTOPS);
				}
				for (i = 0; i < pN->m_iLinks; i++)
				{
					pN->SetIntersectType(i, bType);
				}
			}
			else
			{
				//we have a mix of priorities
				if (lowPriority <=2)
				{
					//big roads, use lights
					pN->SetVisual(VIT_ALLLIGHTS);
					for (i = 0; i < pN->m_iLinks; i++)
					{
						pN->SetIntersectType(i, IT_LIGHT);
					}
				}
				else
				{
					//top priority roads have right of way
					pN->SetVisual(VIT_STOPSIGN);
					for (i = 0; i < pN->m_iLinks; i++)
					{
						curRoad = (LinkEdit*)(pN->GetLink(i));
						if (curRoad->m_iPriority == topPriority) {
							//higher priority road.
							pN->SetIntersectType(i, IT_NONE);
						} else {
							//low priority.
							pN->SetIntersectType(i, IT_STOPSIGN);
						}
					}
				}
			}
		}

		for (i = 0; i< pN->m_iLinks; i++) {
			pN->SetLightStatus(i, LT_INVALID);
		}
		pN->AdjustForLights();

		pN = pN->GetNext();
	}
}

bool RoadMapEdit::ApplyCFCC(LinkEdit *pR, const char *str)
{
	bool bReject = false;

	if (str[0] != 'A')
		return false;
	int code1 = str[1] - '0';
	int code2 = str[2] - '0';
	switch (code1)
	{
	case 1:
		// Primary Highway With Limited Access
		pR->m_iLanes = 4;
		pR->m_iHwy = 1;		// better to have actual highway number
		break;
	case 2:
		// Primary Road Without Limited Access
		pR->m_iLanes = 2;
		pR->m_iHwy = 1;		// better to have actual highway number
		break;
	case 3:
		// Secondary and Connecting Road
		pR->m_iLanes = 2;
		break;
	case 4:
		// Local, Neighborhood, and Rural Road 
		pR->m_iLanes = 2;
		break;
	case 5:
		// Vehicular Trail
		pR->m_iLanes = 1;
		pR->m_Surface = SURFT_2TRACK;
		break;
	// Road with Special Characteristics 
	case 6:
		if (code2 == 1)
		{
			// cul-de-sac
		}
		if (code2 == 2)
		{
			// traffic circle
		}
		if (code2 == 3)
		{
			// access ramp
			// 1 lane, 1 direction
			pR->m_iLanes = 1;
			pR->m_iFlags &= ~RF_REVERSE;
		}
		if (code2 == 4)
		{
			 // service drive
		}
		if (code2 == 5)
		{
			// ferry crossing
			bReject = true;
		}
		break;
	// 
	case 7:
		// Road as Other Thoroughfare 
		if (code2 == 1)
		{
			// Walkway or trail for pedestrians, usually unnamed
			pR->m_iLanes = 1;
			pR->m_Surface = SURFT_TRAIL;
		}
		if (code2 == 2)
		{
			// Stairway, stepped road for pedestrians, usually unnamed
			bReject = true;
		}
		if (code2 == 3)
		{
			// Alley, road for service vehicles, usually unnamed, located at
			// the rear of buildings and property
			pR->m_iLanes = 1;
		}
		if (code2 == 4)
		{
			// Driveway or service road, usually privately owned and unnamed,
			// used as access to residences, trailer parks, and apartment
			// complexes, or as access to logging areas, oil rigs, ranches,
			// farms, and park lands
			pR->m_iLanes = 1;
		}
		break;
	}
	return bReject;
}

void RoadMapEdit::AddElementsFromSHP(const char *filename, vtProjection &proj,
									 void progress_callback(int))
{
	SHPHandle hSHP = SHPOpen(filename, "rb");
	if (hSHP == NULL)
		return;

    int		nEntities, nShapeType;
    double 	adfMinBound[4], adfMaxBound[4];
	FPoint2 point;

	SHPGetInfo(hSHP, &nEntities, &nShapeType, adfMinBound, adfMaxBound);
	if (nShapeType != SHPT_ARC)
		return;

	// Open DBF File, if one exists
	int cfcc = -1;
	DBFHandle db = DBFOpen(filename, "rb");
	if (db != NULL)
	{
		int fields, i, *pnWidth = 0, *pnDecimals = 0;
		char pszFieldName[20];
		fields = DBFGetFieldCount(db);
		for (i = 0; i < fields; i++)
		{
			DBFFieldType fieldtype = DBFGetFieldInfo(db, i,
				pszFieldName, pnWidth, pnDecimals );
			if (!strcmp(pszFieldName, "CFCC"))
				cfcc = i;
		}
	}

	// set projection
	m_proj = proj;

	NodeEdit *pN1, *pN2;
	LinkEdit *pR;
	int i, j, npoints;

	for (i = 0; i < nEntities; i++)
	{
		if ((i % 32) == 0)
			progress_callback(i * 100 / nEntities);

		SHPObject *psShape = SHPReadObject(hSHP, i);
		npoints = psShape->nVertices;

		// create 2 new nodes (begin/end) and a new line
		pN1 = new NodeEdit();
		pN1->m_p.x = psShape->padfX[0];
		pN1->m_p.y = psShape->padfY[0];
		pN1->SetVisual(VIT_NONE);

		// add to list
		AddNode(pN1);

		pN2 = new NodeEdit();
		pN2->m_p.x = psShape->padfX[npoints-1];
		pN2->m_p.y = psShape->padfY[npoints-1];
		pN2->SetVisual(VIT_NONE);

		// add to list
		AddNode(pN2);

		// create new road
		pR = new LinkEdit();
		pR->m_iLanes = 2;
		pR->m_iPriority = 1;

		if (cfcc != -1)
		{
			const char *str = DBFReadStringAttribute(db, i, cfcc);
			ApplyCFCC(pR, str);
		}
		// copy point data
		pR->SetNode(0, pN1);
		pR->SetNode(1, pN2);

		pR->SetSize(npoints);
		for (j = 0; j < npoints; j++)
		{
			pR->GetAt(j).x = psShape->padfX[j];
			pR->GetAt(j).y = psShape->padfY[j];
		}

		//set bounding box for the road
		pR->ComputeExtent();

		// add to list
		AddLink(pR);

		// inform the Nodes to which it belongs
		pR->GetNode(0)->AddLink(pR);
		pR->GetNode(1)->AddLink(pR);
		pR->m_fLength = pR->Length();

		SHPDestroyObject(psShape);
	}
	m_bValidExtents = false;
	SHPClose(hSHP);

	//guess and add some intersection behaviors
	// (chopped)
}

bool RoadMapEdit::extract_road_attributes(const char *strEntity, int &lanes,
										  SurfaceType &stype, int &priority)
{
	int numEntity = atoi(strEntity);
	int iMajorAttr = numEntity / 10000;
	int iMinorAttr = numEntity % 10000;

	// check to see if there is an attribute for road type
	int road_type = 0;
	if (iMajorAttr == 170 &&
		((iMinorAttr >= 201 && iMinorAttr <= 213) ||
		(iMinorAttr >= 217 && iMinorAttr <= 222) ||
		(iMinorAttr >= 401 && iMinorAttr <= 405))
		)
	{
		road_type = iMinorAttr;
	}
	if (iMajorAttr == 180 && 
		(iMinorAttr == 201 || iMinorAttr == 202))
	{
		road_type = -iMinorAttr;
	}

	stype = SURFT_NONE;
	priority = 0;

	ApplyDLGAttributes(road_type, lanes, stype, priority);
	return (stype != SURFT_NONE);
}


void RoadMapEdit::AddElementsFromOGR(OGRDataSource *pDatasource,
									 void progress_callback(int))
{
	int i, j, feature_count, count;
	OGRLayer		*pLayer;
	OGRFeature		*pFeature;
	OGRGeometry		*pGeom;
	OGRPoint		*pPoint;
	OGRLineString   *pLineString;
	OGRFeatureDefn	*defn;
	const char		*layer_name;

	NodeEdit *pN;
	LinkEdit *pR;
	NodeEditPtr *pNodeLookup;

	// 
	// Check if this data source is a USGS SDTS DLG
	//
	// Iterate through the layers looking for the ones we care about
	//
	bool bIsSDTS = false;
	int num_layers = pDatasource->GetLayerCount();
	for (i = 0; i < num_layers; i++)
	{
		pLayer = pDatasource->GetLayer(i);
		defn = pLayer->GetLayerDefn();
		layer_name = defn->GetName();
		if (!strcmp(layer_name, "NO01"))
			bIsSDTS = true;
	}

	for (i = 0; i < num_layers; i++)
	{
		pLayer = pDatasource->GetLayer(i);
		feature_count = pLayer->GetFeatureCount();
  		pLayer->ResetReading();
		defn = pLayer->GetLayerDefn();
		layer_name = defn->GetName();

		// Nodes (from an SDTS DLG file)
		if (!strcmp(layer_name, "NO01"))
		{
			// Get the projection (SpatialReference) from this layer
			OGRSpatialReference *pSpatialRef = pLayer->GetSpatialRef();
			if (pSpatialRef)
				m_proj.SetSpatialReference(pSpatialRef);

			pNodeLookup = new NodeEditPtr[feature_count+1];

			int id = 1;
			while( (pFeature = pLayer->GetNextFeature()) != NULL )
			{
				pGeom = pFeature->GetGeometryRef();
				if (!pGeom) continue;
				pPoint = (OGRPoint *) pGeom;
				pN = new NodeEdit();
				pN->m_id = id++;

				pN->m_p.x = pPoint->getX();
				pN->m_p.y = pPoint->getY();

				AddNode(pN);

				//add to array
				pNodeLookup[pN->m_id] = pN;
			}
		}
		// Lines (Arcs, Roads) (from an SDTS DLG file)
		else if (!strcmp(layer_name, "LE01"))
		{
			// get field indices
			int index_snid = defn->GetFieldIndex("SNID");
			int index_enid = defn->GetFieldIndex("ENID");
			int index_entity = defn->GetFieldIndex("ENTITY_LABEL");
			int index_lanes = defn->GetFieldIndex("LANES");
			int index_class = defn->GetFieldIndex("FUNCTIONAL_CLASS");
			int index_route = defn->GetFieldIndex("ROUTE_NUMBER");
			int index_rtype = defn->GetFieldIndex("ROUTE_TYPE");

			count = 0;
			while( (pFeature = pLayer->GetNextFeature()) != NULL )
			{
				count++;
				progress_callback(count * 100 / feature_count);

				// Ignore non-entities
				if (!pFeature->IsFieldSet(index_entity))
					continue;

				// The "ENTITY_LABEL" contains the same information as the old
				// DLG classification.  First, try to use this field to guess
				// values such as number of lanes, etc.
				const char *str_entity = pFeature->GetFieldAsString(index_entity);
				int lanes;
				SurfaceType stype;
				int priority;
				bool result = extract_road_attributes(str_entity, lanes, stype, priority);
				if (!result)
					continue;

				pGeom = pFeature->GetGeometryRef();
				if (!pGeom) continue;
				pLineString = (OGRLineString *) pGeom;

				pR = new LinkEdit();
				pR->m_fWidth = 1.0f;	// defaults
				pR->m_Surface = stype;
				pR->m_iLanes = lanes;		// defaults
				pR->SetHeightAt(0,0);
				pR->SetHeightAt(1,0);
				pR->m_iPriority = priority;

				if (pFeature->IsFieldSet(index_lanes))
				{
					// If the 'lanes' field is set, it should override the
					// previous guess
					int value_lanes = pFeature->GetFieldAsInteger(index_lanes);
					if (value_lanes > 0)
						pR->m_iLanes = value_lanes;
				}
				if (pFeature->IsFieldSet(index_route))
				{
					const char *str_route = pFeature->GetFieldAsString(index_route);
					// should probably eventually store the route as a string.
					// currently, it is only supported as an integer
					if (!strncmp(str_route, "SR", 2))
					{
						int int_route = atoi(str_route+2);
						pR->m_iHwy = int_route;
					}
				}
				if (pFeature->IsFieldSet(index_rtype))
				{
					const char *str_rtype = pFeature->GetFieldAsString(index_rtype);
				}

				int num_points = pLineString->getNumPoints();
				pR->SetSize(num_points);
				for (j = 0; j < num_points; j++)
					pR->SetAt(j, DPoint2(pLineString->getX(j),
						pLineString->getY(j)));

#if 0
				// Slow way to guess at road-node connectivity: compare points
				for (pN = GetFirstNode(); pN; pN=pN->GetNext())
				{
					if (pR->GetAt(0) == pN->m_p)
						pR->SetNode(0, pN);
					if (pR->GetAt(num_points-1) == pN->m_p)
						pR->SetNode(1, pN);
				}
#else
				// Much faster: Get start/end information from SDTS via OGR
				int snid = pFeature->GetFieldAsInteger(index_snid);
				int enid = pFeature->GetFieldAsInteger(index_enid);
				pR->SetNode(0, pNodeLookup[snid]);
				pR->SetNode(1, pNodeLookup[enid]);
#endif
				pR->ComputeExtent();

				AddLink(pR);

				// inform the Nodes to which it belongs
				pR->GetNode(0)->AddLink(pR);
				pR->GetNode(1)->AddLink(pR);
			}
		}
		else if (!bIsSDTS)
		{
			// For OGR import from a file that isn't an SDTS-DLG, import what
			// we can from the first layer, then stop.
			AppendFromOGRLayer(pLayer);
			break;
		}
	}
	if (bIsSDTS)
		GuessIntersectionTypes();
}


bool RoadMapEdit::AppendFromOGRLayer(OGRLayer *pLayer)
{
	int i, num_fields, feature_count, count;
	OGRFeature		*pFeature;
	OGRGeometry		*pGeom;
	OGRFeatureDefn	*defn;
	const char		*layer_name;
	OGRwkbGeometryType geom_type;

	// Get basic information about the layer we're reading
	feature_count = pLayer->GetFeatureCount();
  	pLayer->ResetReading();
	defn = pLayer->GetLayerDefn();
	if (!defn)
		return false;

	layer_name = defn->GetName();
	num_fields = defn->GetFieldCount();
	geom_type = defn->GetGeomType();

	// Get the projection (SpatialReference) from this layer, if we can.
	// Sometimes (e.g. for GML) the layer doesn't have it; may have to
	// use the first Geometry instead.
	bool bGotCS = false;
	OGRSpatialReference *pSpatialRef = pLayer->GetSpatialRef();
	if (pSpatialRef)
	{
		m_proj.SetSpatialReference(pSpatialRef);
		bGotCS = true;
	}

	// Convert from OGR to our geometry type
	bool bGood = false;
	while (!bGood)
	{
		switch (geom_type)
		{
		case wkbLineString:
		case wkbMultiLineString:
			bGood = true;
			break;
		case wkbUnknown:
			// This usually indicates that the file contains a mix of different
			// geometry types.  Look at the first geometry.
			pFeature = pLayer->GetNextFeature();
			pGeom = pFeature->GetGeometryRef();
			geom_type = pGeom->getGeometryType();
			break;
		default:
			return false;	// don't know what to do with this geom type
		}
	}

	// Read Data from OGR into memory
	int num_geoms;
	OGRLineString   *pLineString;
	OGRMultiLineString   *pMulti;

	pLayer->ResetReading();
	count = 0;
	while( (pFeature = pLayer->GetNextFeature()) != NULL )
	{
		pGeom = pFeature->GetGeometryRef();
		if (!pGeom)
			continue;

		if (!bGotCS)
		{
			OGRSpatialReference *pSpatialRef = pGeom->getSpatialReference();
			if (pSpatialRef)
			{
				m_proj.SetSpatialReference(pSpatialRef);
				bGotCS = true;
			}
		}
		// Beware - some OGR-supported formats, such as MapInfo,
		//  will have more than one kind of geometry per layer.
		geom_type = pGeom->getGeometryType();
		num_geoms = 1;

		switch (geom_type)
		{
		case wkbLineString:
			pLineString = (OGRLineString *) pGeom;
			AddLinkFromLineString(pLineString);
			break;
		case wkbMultiLineString:
			pMulti = (OGRMultiLineString *) pGeom;
			num_geoms = pMulti->getNumGeometries();
			for (i = 0; i < num_geoms; i++)
			{
				pLineString = (OGRLineString *) pMulti->getGeometryRef(i);
				AddLinkFromLineString(pLineString);
			}
			break;
		default:
			continue;	// ignore all other geometry types
		}
		// track total features
		feature_count += (num_geoms-1);
	}
	return true;
}


void RoadMapEdit::AddLinkFromLineString(OGRLineString *pLineString)
{
	NodeEdit *pN1, *pN2;
	LinkEdit *pR;
	DPoint2 p2;

	int j, num_points = pLineString->getNumPoints();

	// create new road
	pR = new LinkEdit();
	pR->m_iLanes = 2;
	pR->m_iPriority = 1;

	pR->SetSize(num_points);
	for (j = 0; j < num_points; j++)
	{
		p2.Set(pLineString->getX(j), pLineString->getY(j));
		pR->SetAt(j, p2);
	}

	// create 2 new nodes (begin/end) and a new line
	pN1 = new NodeEdit();
	pN1->m_p.x = pLineString->getX(0);
	pN1->m_p.y = pLineString->getY(0);
	pN1->SetVisual(VIT_NONE);

	// add to list
	AddNode(pN1);

	pN2 = new NodeEdit();
	pN2->m_p.x = pLineString->getX(num_points-1);
	pN2->m_p.y = pLineString->getY(num_points-1);
	pN2->SetVisual(VIT_NONE);

	// add to list
	AddNode(pN2);

	// point link to nodes
	pR->SetNode(0, pN1);
	pR->SetNode(1, pN2);

	//set bounding box for the road
	pR->ComputeExtent();

	// add to list
	AddLink(pR);

	// point node to links
	pR->GetNode(0)->AddLink(pR);
	pR->GetNode(1)->AddLink(pR);
}

