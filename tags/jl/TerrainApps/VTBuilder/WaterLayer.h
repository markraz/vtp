//
// WaterLayer.h
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef WATERLAYER_H
#define WATERLAYER_H

#include "vtdata/shapelib/shapefil.h"
#include "vtdata/RoadMap.h"
#include "Layer.h"

//////////////////////////////////////////////////////////

class vtWaterLayer : public vtLayer
{
public:
	vtWaterLayer();
	~vtWaterLayer();

	bool GetExtent(DRECT &rect);
	void DrawLayer(wxDC* pDC, vtScaledView *pView);
	bool ConvertProjection(vtProjection &proj);
	bool OnSave();
	bool OnLoad();
	void AppendDataFrom(vtLayer *pL);
	void GetProjection(vtProjection &proj);
	void Offset(DPoint2 p);

	void AddElementsFromDLG(vtDLGFile *pDlg);
	void AddElementsFromSHP(const char *filename, vtProjection &proj);

protected:
	// data for rivers and water bodies
	// eventually, should have vector+width data for rivers, area data for bodies
	// for now, just use plain vectors for everything
	DPolyArray2		m_Lines;
	vtProjection	m_proj;
};

#endif