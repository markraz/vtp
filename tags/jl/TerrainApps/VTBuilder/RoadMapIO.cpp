//
// RoadMapEdit.cpp
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#include "wx/wxprec.h"
#include "vtdata/shapelib/shapefil.h"

#include "RoadMapEdit.h"
#include "assert.h"

#include "ogrsf_frmts.h"

#define BUFFER_SIZE		8000
#define MAX_SEGMENT_LENGTH	80.0						// in meters

bool RoadMapEdit::attribute_filter_roads(DLGLine *pLine, int &lanes, SurfaceType &stype, int &priority)
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
	stype = ST_NONE;
	priority = 0;

	// consider only specific roads (highway)
	switch (road_type)
	{
	case -201:
	case -202:
		stype = ST_RAILROAD;
		lanes = 1;
		priority=1;
		break;
	case 201:	//	Primary route, class 1, symbol undivided
	case 202:	//	Primary route, class 1, symbol divided by centerline
	case 203:	//	Primary route, class 1, divided, lanes separated
	case 204:	//	Primary route, class 1, one way, other than divided highway
		stype = ST_PAVED;
		lanes = 4;
		priority = 1;
		break;
	case 205:	//	Secondary route, class 2, symbol undivided
	case 206:	//	Secondary route, class 2, symbol divided by centerline
	case 207:	//	Secondary route, class 2, symbol divided, lanes separated
	case 208:	//	Secondary route, class 2, one way, other then divided highway
		stype = ST_PAVED;
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
		stype = ST_PAVED;
		lanes = 2;
		priority = 3;
		break;
	case 210:	//	Road or street, class 4
	case 219:	//  Road or street, class 4, 1-way
		stype = ST_DIRT;
		lanes = 2;
		priority = 5;
		break;
	case 402:	//	Ramp in interchange
		stype = ST_PAVED;
		lanes = 1;
		priority = 4;
		break;
	case 211:	//	Trail, class 5, other than four-wheel drive vehicle
		stype = ST_TRAIL;
		lanes = 1;
		priority = 10;
		break;
	case 212:	//	Trail, class 5, four-wheel-drive vehicle
		stype = ST_2TRACK;
		lanes = 1;
		priority = 6;
		break;
	case 213:	//	Footbridge
		break;

	}
	return (stype != ST_NONE);
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
	RoadEdit *pR;
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
		pR = new RoadEdit();
		pR->m_fWidth = 1.0f;
		pR->m_Surface = stype;
		pR->m_iLanes = lanes;
		pR->SetHeightAt(0,0);
		pR->SetHeightAt(1,0);
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
		AddRoad(pR);

		// inform the Nodes to which it belongs
		pR->GetNode(0)->AddRoad(pR);
		pR->GetNode(1)->AddRoad(pR);
		pR->m_fLength = pR->Length();	
	}
	
	//delete the lookup array.
	delete pNodeLookup;

	//guess and add some intersection behaviors
	pN = GetFirstNode();
	RoadEdit* curRoad;
	while (pN)
	{
		if (pN->m_iRoads <= 2)
		{
			pN->SetVisual(VIT_NONE);
			if (pN->m_iRoads > 0)
				assert(pN->SetIntersectType(0,IT_NONE));
			if (pN->m_iRoads == 2)
				assert(pN->SetIntersectType(1,IT_NONE));
		}
		else
		{
			int topPriority = ((RoadEdit*)(pN->GetRoad(0)))->m_iPriority;
			int topCount = 0;
			int lowPriority = topPriority;

			//analyze the roads intersecting at the node
			for (i = 0; i < pN->m_iRoads; i++)
			{
				curRoad = (RoadEdit*)(pN->GetRoad(i));
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
			if (topCount == pN->m_iRoads)
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
				for (i = 0; i < pN->m_iRoads; i++)
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
					for (i = 0; i < pN->m_iRoads; i++)
					{
						pN->SetIntersectType(i, IT_LIGHT);
					}
				}
				else
				{
					//top priority roads have right of way
					pN->SetVisual(VIT_STOPSIGN);
					for (i = 0; i < pN->m_iRoads; i++)
					{
						curRoad = (RoadEdit*)(pN->GetRoad(i));
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

		for (i = 0; i< pN->m_iRoads; i++) {
			pN->SetLightStatus(i, LT_INVALID);
		}
		pN->AdjustForLights();

		pN = pN->GetNext();
	}
}

void RoadMapEdit::AddElementsFromSHP(const char *filename, vtProjection &proj)
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

	// set projection
	m_proj = proj;

	NodeEdit *pN1, *pN2;
	RoadEdit *pR;
	int i, j, npoints;

	for (i = 0; i < nEntities; i++)
	{
		SHPObject *psShape = SHPReadObject(hSHP, i);
		npoints = psShape->nVertices;

		// create 2 new nodes (begin/end) and a new line
		pN1 = new NodeEdit();
		pN1->m_id = -1;
		pN1->m_p.x = psShape->padfX[0];
		pN1->m_p.y = psShape->padfY[0];
		pN1->SetVisual(VIT_NONE);

		// add to list
		AddNode(pN1);

		pN2 = new NodeEdit();
		pN2->m_id = -1;
		pN2->m_p.x = psShape->padfX[npoints-1];
		pN2->m_p.y = psShape->padfY[npoints-1];
		pN2->SetVisual(VIT_NONE);

		// add to list
		AddNode(pN2);

		// create new road
		pR = new RoadEdit();
		pR->m_fWidth = 1.0f;
		pR->m_Surface = ST_PAVED;
		pR->m_iLanes = 2;
		pR->SetHeightAt(0, 0);
		pR->SetHeightAt(1, 0);
		pR->m_iPriority = 1;

		// copy data from DLG line
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
		pR->m_iHwy = -1;

		// add to list
		AddRoad(pR);

		// inform the Nodes to which it belongs
		pR->GetNode(0)->AddRoad(pR);
		pR->GetNode(1)->AddRoad(pR);
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

	stype = ST_NONE;
	priority = 0;

	// consider only specific roads (highway)
	switch (road_type)
	{
	case -201:
	case -202:
		stype = ST_RAILROAD;
		lanes = 1;
		priority=1;
		break;
	case 201:	//	Primary route, class 1, symbol undivided
	case 202:	//	Primary route, class 1, symbol divided by centerline
	case 203:	//	Primary route, class 1, divided, lanes separated
	case 204:	//	Primary route, class 1, one way, other than divided highway
		stype = ST_PAVED;
		lanes = 4;
		priority = 1;
		break;
	case 205:	//	Secondary route, class 2, symbol undivided
	case 206:	//	Secondary route, class 2, symbol divided by centerline
	case 207:	//	Secondary route, class 2, symbol divided, lanes separated
	case 208:	//	Secondary route, class 2, one way, other then divided highway
		stype = ST_PAVED;
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
		stype = ST_PAVED;
		lanes = 2;
		priority = 3;
		break;
	case 210:	//	Road or street, class 4
	case 219:	//  Road or street, class 4, 1-way
		stype = ST_DIRT;
		lanes = 2;
		priority = 5;
		break;
	case 402:	//	Ramp in interchange
		stype = ST_PAVED;
		lanes = 1;
		priority = 4;
		break;
	case 211:	//	Trail, class 5, other than four-wheel drive vehicle
		stype = ST_TRAIL;
		lanes = 1;
		priority = 10;
		break;
	case 212:	//	Trail, class 5, four-wheel-drive vehicle
		stype = ST_2TRACK;
		lanes = 1;
		priority = 6;
		break;
	case 213:	//	Footbridge
		break;

	}
	return (stype != ST_NONE);
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

	NodeEdit *pN;
	RoadEdit *pR;
	NodeEditPtr *pNodeLookup;

	// Assume that this data source is a USGS SDTS DLG
	//
	// Iterate through the layers looking for the ones we care about
	//
	int num_layers = pDatasource->GetLayerCount();
	for (i = 0; i < num_layers; i++)
	{
		pLayer = pDatasource->GetLayer(i);
		if (!pLayer)
			continue;

		feature_count = pLayer->GetFeatureCount();
  		pLayer->ResetReading();
		OGRFeatureDefn *defn = pLayer->GetLayerDefn();
		if (!defn)
			continue;

		const char *layer_name = defn->GetName();

		// Nodes
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
		// Lines (Arcs, Roads)
		if (!strcmp(layer_name, "LE01"))
		{
			// field indices
			int index_snid;
			int index_enid;
			int index_entity;
			int index_lanes;
			int index_class;
			int index_route;
			int index_rtype;

			count = 0;
			while( (pFeature = pLayer->GetNextFeature()) != NULL )
			{
				if (count == 0)
				{
					// get field indices from the first feature
					index_snid = pFeature->GetFieldIndex("SNID");
					index_enid = pFeature->GetFieldIndex("ENID");
					index_entity = pFeature->GetFieldIndex("ENTITY_LABEL");
					index_lanes = pFeature->GetFieldIndex("LANES");
					index_class = pFeature->GetFieldIndex("FUNCTIONAL_CLASS");
					index_route = pFeature->GetFieldIndex("ROUTE_NUMBER");
					index_rtype = pFeature->GetFieldIndex("ROUTE_TYPE");
				}
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

				pR = new RoadEdit();
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

				AddRoad(pR);

				// inform the Nodes to which it belongs
				pR->GetNode(0)->AddRoad(pR);
				pR->GetNode(1)->AddRoad(pR);
			}
		}
	}
}

#if 0
	// unused code; here in care is becomes useful in the future
{
	{
		int field_count1 = defn->GetFieldCount();
		for (j = 0; j < field_count1; j++)
		{
			OGRFieldDefn *field_def1 = defn->GetFieldDefn(j);
			if (field_def1)
			{
				const char *fnameref = field_def1->GetNameRef();
				OGRFieldType ftype = field_def1->GetType();
			}
		}

		OGRSpatialReference *spatialref2 = layer->GetSpatialRef();
		// const char*  layer->GetInfo ( const char * ) 

		int feature_count = layer->GetFeatureCount();
		OGRFeature  *pFeature;

		int count = 0;
  		layer->ResetReading();
		while( (pFeature = layer->GetNextFeature()) != NULL )
		{
			count++;
			OGRGeometry *geom = pFeature->GetGeometryRef();
			if (geom)
			{
				int dim = geom->getDimension();
				int dim2 = geom->getCoordinateDimension();
				const char *gname2 = geom->getGeometryName();
				OGRwkbGeometryType gtype2 = geom->getGeometryType();
				switch (gtype2)
				{
				case wkbPoint:
					{
						OGRPoint    *pPoint = (OGRPoint *) geom;
						double x = pPoint->getX();
					}
					break;
				case wkbLineString:
					{
						OGRLineString    *pLine = (OGRLineString *) geom;
						int line_poins = pLine->getNumPoints();
						double x = pLine->getX(0);
					}
					break;
				}
			}
			OGRFeatureDefn *feat_defn = pFeature->GetDefnRef();
			if (feat_defn)
			{
				const char *name2 = feat_defn->GetName();
				int field_count2 = feat_defn->GetFieldCount();
				for (j = 0; j < field_count2; j++)
				{
					OGRFieldDefn *field_def2 = feat_defn->GetFieldDefn(j);
					if (field_def2)
					{
						const char *fnameref = field_def2->GetNameRef();
						OGRFieldType ftype = field_def2->GetType();
					}
				}
			}
			int field_count2 = pFeature->GetFieldCount();
			for (j = 0; j < field_count2; j++)
			{
				OGRFieldDefn *field_def2 = pFeature->GetFieldDefnRef(j);
				if (field_def2)
				{
					const char *fnameref2 = field_def2->GetNameRef();
					OGRFieldType ftype2 = field_def2->GetType();
				}
				if (!pFeature->IsFieldSet(j))
					continue;

				OGRFieldType ftype2 = pFeature->GetFieldDefnRef(j)->GetType();
				switch (ftype2)
				{
				case OFTInteger:
					{
						int ivalue = pFeature->GetFieldAsInteger(j);
					}
					break;
				case OFTString:
					{
						const char * strvalue = pFeature->GetFieldAsString(j);
					}
					break;
				}
			}
		}
	}
}
#endif

