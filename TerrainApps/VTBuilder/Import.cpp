//
// Import.cpp - MainFrame methods for importing data
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "vtdata/DLG.h"
#include "vtdata/LULC.h"
#include "vtdata/Unarchive.h"
#include "vtdata/boost/directory.h"
#include "vtdata/FilePath.h"
#include "vtdata/vtLog.h"

#include "Frame.h"
#include "Helper.h"
// Layers
#include "StructLayer.h"
#include "WaterLayer.h"
#include "ElevLayer.h"
#include "ImageLayer.h"
#include "RawLayer.h"
#include "RoadLayer.h"
#include "VegLayer.h"
#include "UtilityLayer.h"
// Dialogs
#include "ImportVegDlg.h"
#include "VegFieldsDlg.h"
#include "Projection2Dlg.h"
#include "ImportStructDlgOGR.h"

#include "ogrsf_frmts.h"

//
// remember a set of directories, one for each layer type
//
wxString ImportDirectory[LAYER_TYPES];


// Helper
wxString GetTempFolderName(const char *base)
{
	// first determine where to put our temporary directory
	wxString2 path;

	const char *temp = getenv("TEMP");
	if (temp)
		path = temp;
	else
#if WIN32
		path = _T("C:/TEMP");
#else
		path = _T("/tmp");
#endif
	path += _T("/");

	// the create a folder named after the file in the full path "base"
	wxString2 base2 = StartOfFilename(base);
	path += base2;

	// appended with the word _temp
	path += _T("_temp");

	return path;
}


//
// Ask the user for a filename, and import data from it.
//
void MainFrame::ImportData(LayerType ltype)
{
	// make a string which contains filters for the appropriate file types
	wxString filter = GetImportFilterString(ltype);

	// ask the user for a filename
	// default the same directory they used last time for a layer of this type
	wxFileDialog loadFile(NULL, _T("Import Data"), ImportDirectory[ltype], _T(""), filter, wxOPEN);
	bool bResult = (loadFile.ShowModal() == wxID_OK);
	if (!bResult)
		return;
	wxString2 strFileName = loadFile.GetPath();

	// remember the directory they used
	ImportDirectory[ltype] = loadFile.GetDirectory();

	ImportDataFromArchive(ltype, strFileName, true);
}

/**
 * Import data of a given type from a file, which can potentially be an
 * archive file.  If it's an archive, it will be unarchived to a temporary
 * folder, and the contents will be imported.
 */
void MainFrame::ImportDataFromArchive(LayerType ltype, const wxString2 &fname_in,
									  bool bRefresh)
{
	// check file extension
	wxString2 fname = fname_in;
	wxString2 ext = fname.AfterLast('.');

	// check if it's an archive
	bool bGZip = false;
	bool bZip = false;

	if (ext.CmpNoCase(_T("gz")) == 0 || ext.CmpNoCase(_T("tgz")) == 0 ||
		ext.CmpNoCase(_T("tar")) == 0)
		bGZip = true;

	if (ext.CmpNoCase(_T("zip")) == 0)
		bZip = true;

	if (bGZip || bZip)
	{
		using namespace boost::filesystem;

		// try to uncompress
		wxString2 prepend_path;
		prepend_path = GetTempFolderName(fname_in.mb_str());
		int result;
		result = vtCreateDir(prepend_path.mb_str());
		if (result == 0 && errno != EEXIST)
		{
			DisplayAndLog("Couldn't create temporary directory to hold contents of archive.");
			return;
		}
		prepend_path += _T("/");

		vtString str1 = fname_in.mb_str();
		vtString str2 = prepend_path.mb_str();

		if (bGZip)
			result = ExpandTGZ(str1, str2);
		if (bZip)
			result = ExpandZip(str1, str2);

		if (result < 1)
		{
			DisplayAndLog("Couldn't expand archive.");
		}
		else if (result == 1)
		{
			// the archive contained a single file
			for (dir_it it((std::string) (prepend_path.mb_str())); it != dir_it(); ++it)
			{
				if (get<is_directory>(it))
					continue;
				std::string name1 = *it;
				fname = prepend_path;
				wxString2 tmp = name1.c_str();
				fname += tmp;
				break;
			}
			ImportDataFromFile(ltype, fname, bRefresh);
		}
		else if (result > 1)
		{
			// probably SDTS
			// try to guess layer type from original file name
			if (fname.Contains(_T(".hy")) || fname.Contains(_T(".HY")))
				ltype = LT_WATER;
			if (fname.Contains(_T(".rd")) || fname.Contains(_T(".RD")))
				ltype = LT_ROAD;
			if (fname.Contains(_T(".dem")) || fname.Contains(_T(".DEM")))
				ltype = LT_ELEVATION;
			if (fname.Contains(_T(".ms")) || fname.Contains(_T(".MS")))
				ltype = LT_STRUCTURE;

			// look for the catalog file
			bool found = false;
			for (dir_it it((std::string) (prepend_path.mb_str())); it != dir_it(); ++it)
			{
				std::string name1 = *it;
				wxString2 fname2 = name1.c_str();
				if (fname2.Right(8).CmpNoCase(_T("catd.ddf")) == 0 ||
				    fname2.Right(4).CmpNoCase(_T(".dem")) == 0)
				{
					fname = prepend_path;
					fname += fname2;
					found= true;
					break;
				}
			}
			if (found)
				ImportDataFromFile(ltype, fname, bRefresh);
			else
				DisplayAndLog("Don't know what to do with contents of archive.");
		}

		// clean up after ourselves
		prepend_path = GetTempFolderName(fname_in.mb_str());
		vtDestroyDir(prepend_path.mb_str());
	}
	else
	{
		// simple case
		ImportDataFromFile(ltype, fname, bRefresh);
	}
}

void MainFrame::ImportDataFromFile(LayerType ltype, const wxString2 &strFileName,
								   bool bRefresh)
{
	// check to see if the file is readable
	FILE *fp = fopen(strFileName.mb_str(), "rb");
	if (!fp)
	{
		// Cannot Open File
		VTLOG("Couldn't open file %s\n", strFileName.mb_str());
		return;
	}
	fclose(fp);

	wxString2 msg = _T("Importing Data from ");
	msg += strFileName;
	VTLOG(msg.mb_str());
	VTLOG("...\n");
	OpenProgressDialog(msg);

	// check the file extension
	wxString strExt = strFileName.AfterLast('.');

	// call the appropriate reader
	vtLayerPtr pLayer = NULL;
	switch (ltype)
	{
	case LT_ELEVATION:
		pLayer = ImportElevation(strFileName);
		break;
	case LT_IMAGE:
		pLayer = ImportImage(strFileName);
		break;
	case LT_ROAD:
	case LT_WATER:
		if (!strExt.CmpNoCase(_T("dlg")))
		{
			pLayer = ImportFromDLG(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("shp")))
		{
			pLayer = ImportFromSHP(strFileName, ltype);
		}
		else if (!strFileName.Right(8).CmpNoCase(_T("catd.ddf")) ||
				 !strExt.CmpNoCase(_T("mif")) ||
				 !strExt.CmpNoCase(_T("tab")))
		{
			pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		break;
	case LT_STRUCTURE:
		if (!strExt.CmpNoCase(_T("shp")))
		{
			pLayer = ImportFromSHP(strFileName, ltype);
//			pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("gml")))
		{
			pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("bcf")))
		{
			pLayer = ImportFromBCF(strFileName);
		}
		else if (!strExt.CmpNoCase(_T("dlg")))
		{
			pLayer = ImportFromDLG(strFileName, ltype);
		}
		else if (!strFileName.Right(8).CmpNoCase(_T("catd.ddf")))
		{
			pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		break;
	case LT_VEG:
		if (!strExt.CmpNoCase(_T("gir")))
		{
			pLayer = ImportFromLULC(strFileName, ltype);
		}
		if (!strExt.CmpNoCase(_T("shp")))
		{
			pLayer = ImportFromSHP(strFileName, ltype);
		}
		break;
	case LT_UNKNOWN:
		if (!strExt.CmpNoCase(_T("gir")))
		{
			pLayer = ImportFromLULC(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("bcf")))
		{
			pLayer = ImportFromBCF(strFileName);
		}
		else if (!strExt.CmpNoCase(_T("dlg")))
		{
			pLayer = ImportFromDLG(strFileName, ltype);
		}
		else if (!strFileName.Right(8).CmpNoCase(_T("catd.ddf")))
		{
			// SDTS file: might be Elevation or Vector (SDTS-DEM or SDTS-DLG)
			// To try to distinguish, look for a file called xxxxrsdf.ddf
			// which would indicate that it is a raster.
			bool bRaster = false;
			int len = strFileName.Length();
			wxString2 strFileName2 = strFileName.Left(len - 8);
			wxString2 strFileName3 = strFileName2 + _T("rsdf.ddf");
			FILE *fp;
			fp = fopen(strFileName3.mb_str(), "rb");
			if (fp != NULL)
			{
				bRaster = true;
				fclose(fp);
			}
			else
			{
				// also try with upper-case (for Unix)
				strFileName3 = strFileName2 + _T("RSDF.DDF");
				fp = fopen(strFileName3.mb_str(), "rb");
				if (fp != NULL)
				{
					bRaster = true;
					fclose(fp);
				}
			}
		
			if (bRaster)
				pLayer = ImportElevation(strFileName);
			else
				pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("shp")))
		{
			pLayer = ImportFromSHP(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("bcf")))
		{
			pLayer = ImportFromBCF(strFileName);
		}
		else
		{
			// Many other Elevation formats are supported
			pLayer = ImportElevation(strFileName);
		}
		break;
	case LT_UTILITY:
		if(!strExt.CmpNoCase(_T("shp")))
			pLayer = ImportFromSHP(strFileName, ltype);
		break;
	case LT_RAW:
		if (!strExt.CmpNoCase(_T("shp")))
			pLayer = ImportFromSHP(strFileName, ltype);
		else if (!strExt.CmpNoCase(_T("mif")) ||
				 !strExt.CmpNoCase(_T("tab")))
		{
			pLayer = ImportRawFromOGR(strFileName);
		}
		break;
	}

	CloseProgressDialog();

	if (!pLayer)
	{
		// import failed
		VTLOG("  import failed/cancelled.\n");
		wxMessageBox(_T("Did not import any data from that file."));
		return;
	}
	VTLOG("  import succeeded.\n");
	pLayer->SetFilename(strFileName);

	bool success = AddLayerWithCheck(pLayer, true);
	if (!success)
		delete pLayer;
}

//
// type to guess layer type from a DLG file
//
LayerType MainFrame::GuessLayerTypeFromDLG(vtDLGFile *pDLG)
{
	LayerType ltype;
	DLGType dtype = pDLG->GuessFileType();

	// convert the DLG type to one of our layer types
	switch (dtype)
	{
	case DLG_HYPSO:		ltype = LT_RAW; break;
	case DLG_HYDRO:		ltype = LT_WATER; break;
	case DLG_VEG:		ltype = LT_RAW; break;
	case DLG_NONVEG:	ltype = LT_RAW; break;
	case DLG_BOUNDARIES:ltype = LT_RAW; break;
	case DLG_MARKERS:	ltype = LT_RAW; break;
	case DLG_ROAD:		ltype = LT_ROAD; break;
	case DLG_RAIL:		ltype = LT_ROAD; break;
	case DLG_MTF:		ltype = LT_RAW; break;
	case DLG_MANMADE:	ltype = LT_STRUCTURE; break;
	case DLG_UNKNOWN:
		{
			// if we can't tell from the DLG, ask the user
			ltype = AskLayerType();
		}
		break;
	}
	return ltype;
}

//
// based on the type of layer, choose which file
// types (file extensions) to allow for import
//
wxString GetImportFilterString(LayerType ltype)
{
	wxString filter = _T("All Known ");
	filter += vtLayer::LayerTypeName[ltype];
	filter += _T(" Formats||");

	switch (ltype)
	{
	case LT_RAW:
		// shp
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_MI);
		break;
	case LT_ELEVATION:
		// dem, etc.
		AddType(filter, FSTRING_ADF);
		AddType(filter, FSTRING_ASC);
		AddType(filter, FSTRING_BIL);
		AddType(filter, FSTRING_CDF);
		AddType(filter, FSTRING_DEM);
		AddType(filter, FSTRING_DTED);
		AddType(filter, FSTRING_GTOPO);
		AddType(filter, FSTRING_IMG);
		AddType(filter, FSTRING_MEM);
		AddType(filter, FSTRING_NTF);
		AddType(filter, FSTRING_PNG);
		AddType(filter, FSTRING_PGM);
		AddType(filter, FSTRING_RAW);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_Surfer);
		AddType(filter, FSTRING_TER);
		AddType(filter, FSTRING_TIF);
		AddType(filter, FSTRING_COMP);
		break;
	case LT_IMAGE:
		// doq, tif
		AddType(filter, FSTRING_BMP);
		AddType(filter, FSTRING_DOQ);
		AddType(filter, FSTRING_IMG);
		AddType(filter, FSTRING_TIF);
		break;
	case LT_ROAD:
		// dlg, shp, sdts-dlg
		AddType(filter, FSTRING_DLG);
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_COMP);
		AddType(filter, FSTRING_MI);
		break;
	case LT_STRUCTURE:
		// dlg, shp, bcf, sdts-dlg
		AddType(filter, FSTRING_GML);
		AddType(filter, FSTRING_DLG);
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_BCF);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_COMP);
		break;
	case LT_WATER:
		// dlg, shp, sdts-dlg
		AddType(filter, FSTRING_DLG);
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_COMP);
		break;
	case LT_VEG:
		// lulc, shp, sdts
		AddType(filter, FSTRING_LULC);
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_COMP);
		break;
	case LT_UTILITY:
		AddType(filter, FSTRING_SHP);
		break;
	}
	return filter;
}


vtLayerPtr MainFrame::ImportFromDLG(const wxString2 &fname_in, LayerType ltype)
{
	wxString2 strFileName = fname_in;

	vtDLGFile *pDLG = new vtDLGFile();
	bool success = pDLG->Read(strFileName.mb_str(), progress_callback);
	if (!success)
	{
		DisplayAndLog(pDLG->GetErrorMessage());
		delete pDLG;
		return NULL;
	}

	// try to guess what kind of data it is by asking the DLG object
	// to look at its attributes
	if (ltype == LT_UNKNOWN)
		ltype = GuessLayerTypeFromDLG(pDLG);

	// create the new layer
	vtLayerPtr pLayer = vtLayer::CreateNewLayer(ltype);

	// read the DLG data into the layer
	if (ltype == LT_ROAD)
	{
		vtRoadLayer *pRL = (vtRoadLayer *)pLayer;
		pRL->AddElementsFromDLG(pDLG);
		pRL->RemoveUnusedNodes();
	}
	if (ltype == LT_WATER)
	{
		vtWaterLayer *pWL = (vtWaterLayer *)pLayer;
		pWL->AddElementsFromDLG(pDLG);
	}
	if (ltype == LT_STRUCTURE)
	{
		vtStructureLayer *pSL = (vtStructureLayer *)pLayer;
		pSL->AddElementsFromDLG(pDLG);
	}
	// now we no longer need the DLG object
	delete pDLG;

	return pLayer;
}

vtLayerPtr MainFrame::ImportFromSHP(const wxString2 &strFileName, LayerType ltype)
{
	bool success;
	int nShapeType;

	SHPHandle hSHP = SHPOpen(strFileName.mb_str(), "rb");
	if (hSHP == NULL)
	{
		wxMessageBox(_T("Couldn't read that Shape file.  Perhaps it is\n")
			_T("missing its corresponding .dbf and .shx files."));
		return NULL;
	}
	else
	{
		// Get type of data
		int     nElem;
		double  dummy[4];
		SHPGetInfo(hSHP, &nElem, &nShapeType, dummy, dummy);

		// Check Shape Type, Veg Layer should be Poly data
		SHPClose(hSHP);
	}

	// if layer type unknown, ask user input
	if (ltype == LT_UNKNOWN)
		ltype = AskLayerType();

	// create the new layer
	vtLayerPtr pLayer = vtLayer::CreateNewLayer(ltype);

	// ask user for a projection
	Projection2Dlg dlg(NULL, -1, _T("Please indicate projection"), wxDefaultPosition);
	dlg.SetProjection(m_proj);

	if (dlg.ShowModal() == wxID_CANCEL)
		return NULL;
	vtProjection proj;
	dlg.GetProjection(proj);

	// read SHP data into the layer
	if (ltype == LT_ROAD)
	{
		vtRoadLayer *pRL = (vtRoadLayer *)pLayer;
		pRL->AddElementsFromSHP(strFileName, proj, progress_callback);
		pRL->RemoveUnusedNodes();
	}

	// read vegetation SHP data into the layer
	if (ltype == LT_VEG)
	{
		if (nShapeType != SHPT_POLYGON && nShapeType != SHPT_POINT)
		{
			wxMessageBox(_T("The Shapefile must have either point features (for \n")
				_T("individual plants) or polygon features (for plant distribution areas)."));
			return NULL;
		}

		vtVegLayer *pVL = (vtVegLayer *)pLayer;
		if (nShapeType == SHPT_POLYGON)
		{
			ImportVegDlg dlg(this, -1, _T("Import Vegetation Information"));
			dlg.SetShapefileName(strFileName);
			if (dlg.ShowModal() == wxID_CANCEL)
				return NULL;
			pVL->AddElementsFromSHP_Polys(strFileName, proj,
				dlg.m_fieldindex, dlg.m_datatype);
		}
		if (nShapeType == SHPT_POINT)
		{
			VegFieldsDlg dlg(this, -1, _T("Map fields to attributes"));
			dlg.SetShapefileName(strFileName);
			dlg.SetVegLayer(pVL);
			if (dlg.ShowModal() == wxID_CANCEL)
				return NULL;
			success = pVL->AddElementsFromSHP_Points(strFileName.mb_str(), proj, dlg.m_options);
			if (!success)
				return NULL;
		}
	}

	if (ltype == LT_WATER)
	{
		vtWaterLayer *pWL = (vtWaterLayer *)pLayer;
		pWL->AddElementsFromSHP(strFileName, proj);
	}

	if (ltype == LT_STRUCTURE)
	{
		vtStructureLayer *pSL = (vtStructureLayer *)pLayer;
		success = pSL->AddElementsFromSHP(strFileName, proj, m_area);
		if (!success)
			return NULL;
	}

	if (ltype == LT_UTILITY)
	{
		vtUtilityLayer *pUL = (vtUtilityLayer *)pLayer;
		pUL->ImportFromSHP(strFileName.mb_str(), proj);
	}

	if (ltype == LT_RAW)
	{
		pLayer->SetFilename(strFileName);
		if (!pLayer->OnLoad())
		{
			delete pLayer;
			pLayer = NULL;
		}
		pLayer->SetProjection(proj);
	}
	return pLayer;
}


vtLayerPtr MainFrame::ImportElevation(const wxString2 &strFileName)
{
	bool bFirst = (m_Layers.GetSize() == 0);
	wxString strExt = strFileName.AfterLast('.');

	vtElevLayer *pElev = new vtElevLayer();

	bool success = pElev->ImportFromFile(strFileName, progress_callback);

	if (success)
		return pElev;
	else
	{
		delete pElev;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportImage(const wxString2 &strFileName)
{
	vtImageLayer *pLayer = new vtImageLayer();

	pLayer->SetFilename(strFileName);
	bool success = pLayer->OnLoad();

	if (success)
		return pLayer;
	else
	{
		delete pLayer;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportFromLULC(const wxString2 &strFileName, LayerType ltype)
{
	// Read LULC file, check for errors
	vtLULCFile *pLULC = new vtLULCFile(strFileName.mb_str());
	if (pLULC->m_iError)
	{
		wxString2 msg = pLULC->GetErrorMessage();
		wxMessageBox(msg);
		delete pLULC;
		return NULL;
	}

	// If layer type unknown, assume it's veg type
	if (ltype = LT_UNKNOWN)
		ltype=LT_VEG;

	pLULC->ProcessLULCPolys();

	// Create New Layer
	vtLayerPtr pLayer = vtLayer::CreateNewLayer(ltype);

	// Read LULC data into the new Veg layer
	vtVegLayer *pVL = (vtVegLayer *)pLayer;

	pVL->AddElementsFromLULC(pLULC);

	// Now we no longer need the LULC object
	delete pLULC;

	return pLayer;
}

vtStructureLayer *MainFrame::ImportFromBCF(const wxString2 &strFileName)
{
	vtStructureLayer *pSL = new vtStructureLayer();
	if (pSL->ReadBCF(strFileName.mb_str()))
		return pSL;
	else
	{
		delete pSL;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportRawFromOGR(const wxString2 &strFileName)
{
	// create the new layer
	vtRawLayer *pRL = new vtRawLayer();
	bool success = pRL->LoadWithOGR(strFileName.mb_str(), progress_callback);

	if (success)
		return pRL;
	else
	{
		delete pRL;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportVectorsWithOGR(const wxString2 &strFileName, LayerType ltype)
{
	vtProjection Projection;

	g_GDALWrapper.RequestOGRFormats();

	OGRDataSource *datasource = OGRSFDriverRegistrar::Open(strFileName.mb_str());
	if (!datasource)
		return NULL;

	if (ltype == LT_UNKNOWN)
	{
		// TODO: Try to guess the layer type from the file
		// For now, just assume it's transportation
		ltype = LT_ROAD;
	}

	// create the new layer
	vtLayerPtr pLayer = vtLayer::CreateNewLayer(ltype);

	// read the OGR data into the layer
	if (ltype == LT_ROAD)
	{
		vtRoadLayer *pRL = (vtRoadLayer *)pLayer;
		pRL->AddElementsFromOGR(datasource, progress_callback);
//		pRL->RemoveUnusedNodes();
	}
	if (ltype == LT_WATER)
	{
		vtWaterLayer *pWL = (vtWaterLayer *)pLayer;
		pWL->AddElementsFromOGR(datasource, progress_callback);
	}
	if (ltype == LT_STRUCTURE)
	{
		ImportStructDlgOGR ImportDialog(GetMainFrame(), -1, _T("Import Structures"));

		ImportDialog.SetDatasource(datasource);

		if (ImportDialog.ShowModal() != wxID_OK)
			return false;

		if (ImportDialog.m_iType == 0)
			ImportDialog.m_opt.type = ST_BUILDING;
		if (ImportDialog.m_iType == 1)
			ImportDialog.m_opt.type = ST_BUILDING;
		if (ImportDialog.m_iType == 2)
			ImportDialog.m_opt.type = ST_LINEAR;
		if (ImportDialog.m_iType == 3)
			ImportDialog.m_opt.type = ST_INSTANCE;

		ImportDialog.m_opt.rect = m_area;

		vtStructureLayer *pSL = (vtStructureLayer *)pLayer;

		if (NULL != GetActiveElevLayer())
			ImportDialog.m_opt.pHeightField = GetActiveElevLayer()->GetHeightField();
		else if (NULL != FindLayerOfType(LT_ELEVATION))
			ImportDialog.m_opt.pHeightField = ((vtElevLayer*)FindLayerOfType(LT_ELEVATION))->GetHeightField();
		else
			ImportDialog.m_opt.pHeightField = NULL;
		pSL->AddElementsFromOGR_RAW(datasource, ImportDialog.m_opt, progress_callback);

		pSL->GetProjection(Projection);
		if (OGRERR_NONE != Projection.Validate())
		{
			// Get a projection
			Projection2Dlg dlg(GetMainFrame(), -1, _T("Please indicate projection"));
			dlg.SetProjection(m_proj);

			if (dlg.ShowModal() == wxID_CANCEL)
			{
				delete pSL;
				return NULL;
			}
			dlg.GetProjection(Projection);
			pSL->SetProjection(Projection);
		}
	}

	delete datasource;

	return pLayer;
}


void MainFrame::ImportDataFromTIGER(const wxString2 &strDirName)
{
	g_GDALWrapper.RequestOGRFormats();

	OGRDataSource *pDatasource = OGRSFDriverRegistrar::Open(strDirName.mb_str());
	if (!pDatasource)
		return;

	// create the new layers
	vtWaterLayer *pWL = new vtWaterLayer;
	pWL->SetFilename(strDirName + _T("/water"));
	pWL->SetModified(true);

	vtRoadLayer *pRL = new vtRoadLayer;
	pRL->SetFilename(strDirName + _T("/roads"));
	pRL->SetModified(true);

	int i, j, feature_count;
	OGRLayer		*pOGRLayer;
	OGRFeature		*pFeature;
	OGRGeometry		*pGeom;
//	OGRPoint		*pPoint;
	OGRLineString   *pLineString;

//	DPolyArray2		chain;
//	DLine2			*dline;
	vtWaterFeature	*wfeat;

	// Assume that this data source is a TIGER/Line file
	//
	// Iterate through the layers looking for the ones we care about
	//
	int num_layers = pDatasource->GetLayerCount();
	for (i = 0; i < num_layers; i++)
	{
		pOGRLayer = pDatasource->GetLayer(i);
		if (!pOGRLayer)
			continue;

		feature_count = pOGRLayer->GetFeatureCount();
  		pOGRLayer->ResetReading();
		OGRFeatureDefn *defn = pOGRLayer->GetLayerDefn();
		if (!defn)
			continue;

#if 1
		//Debug:
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
#endif

		// ignore all layers other than CompleteChain
		const char *layer_name = defn->GetName();
		if (strcmp(layer_name, "CompleteChain"))
			continue;

		// Get the projection (SpatialReference) from this layer
		OGRSpatialReference *pSpatialRef = pOGRLayer->GetSpatialRef();
		if (pSpatialRef)
		{
			vtProjection proj;
			proj.SetSpatialReference(pSpatialRef);
			pWL->SetProjection(proj);
			pRL->SetProjection(proj);
		}

		// Progress Dialog
		OpenProgressDialog(_T("Importing from TIGER..."));

		int index_cfcc = defn->GetFieldIndex("CFCC");
		int fcount = 0;
		while( (pFeature = pOGRLayer->GetNextFeature()) != NULL )
		{
			UpdateProgressDialog(100 * fcount / feature_count);

			pGeom = pFeature->GetGeometryRef();
			if (!pGeom) continue;

			if (!pFeature->IsFieldSet(index_cfcc))
				continue;

			const char *cfcc = pFeature->GetFieldAsString(index_cfcc);

			pLineString = (OGRLineString *) pGeom;
			int num_points = pLineString->getNumPoints();

			if (!strncmp(cfcc, "A", 1))
			{
				// Road: implicit nodes at start and end
				LinkEdit *r = (LinkEdit *) pRL->NewLink();
				bool bReject = pRL->ApplyCFCC((LinkEdit *)r, cfcc);
				if (bReject)
				{
					delete r;
					continue;
				}
				for (j = 0; j < num_points; j++)
				{
					r->Append(DPoint2(pLineString->getX(j),
						pLineString->getY(j)));
				}
				Node *n1 = pRL->NewNode();
				n1->m_p.Set(pLineString->getX(0), pLineString->getY(0));

				Node *n2 = pRL->NewNode();
				n2->m_p.Set(pLineString->getX(num_points-1), pLineString->getY(num_points-1));

				pRL->AddNode(n1);
				pRL->AddNode(n2);
				r->SetNode(0, n1);
				r->SetNode(1, n2);
				n1->AddLink(r);
				n2->AddLink(r);

				//set bounding box for the road
				r->ComputeExtent();

				pRL->AddLink(r);
			}

			if (!strncmp(cfcc, "H", 1))
			{
				// Hydrography
				wfeat = NULL;
				int num = atoi(cfcc+1);
				switch (num)
				{
				case 1:		// Shoreline of perennial water feature
				case 2:		// Shoreline of intermittent water feature
					break;
				case 11:	// Perennial stream or river
				case 12:	// Intermittent stream, river, or wash
				case 13:	// Braided stream or river
					wfeat = new vtWaterFeature();
					wfeat->m_bIsBody = false;
					break;
				case 30:	// Lake or pond
				case 31:	// Perennial lake or pond
				case 32:	// Intermittent lake or pond
				case 40:	// Reservoir
				case 41:	// Perennial reservoir
				case 42:	// Intermittent reservoir
				case 50:	// Bay, estuary, gulf, sound, sea, or ocean
				case 51:	// Bay, estuary, gulf, or sound
				case 52:	// Sea or ocean
					wfeat = new vtWaterFeature();
					wfeat->m_bIsBody = false;
					break;
				}
				if (wfeat)
				{
					wfeat->SetSize(num_points);
					for (j = 0; j < num_points; j++)
					{
						wfeat->SetAt(j, DPoint2(pLineString->getX(j),
							pLineString->getY(j)));
					}
					pWL->AddFeature(wfeat);
				}
			}

			fcount++;
		}
		CloseProgressDialog();
	}

	delete pDatasource;

	// Merge nodes
//	OpenProgressDialog("Removing redundant nodes...");
//	pRL->MergeRedundantNodes(true, progress_callback);

	// Set visual properties
	for (NodeEdit *pN = pRL->GetFirstNode(); pN; pN = pN->GetNext())
	{
		pN->DetermineVisualFromLinks();
	}

	bool success;
	success = AddLayerWithCheck(pWL, true);
	if (!success)
		delete pWL;

	success = AddLayerWithCheck(pRL, true);
	if (!success)
		delete pRL;
}


