//
// Import.cpp - MainFrame methods for importing data
//
// Copyright (c) 2001-2006 Virtual Terrain Project
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
#include "vtdata/FilePath.h"
#include "vtdata/vtLog.h"
#include "vtdata/Building.h"
#include "vtui/Helper.h"
#include "vtui/ProjectionDlg.h"

#include "Frame.h"
#include "Helper.h"
#include "FileFilters.h"
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
#include "ImportStructDlgOGR.h"
#include "ImportPointDlg.h"

#include "ogrsf_frmts.h"

//
// remember a set of directories, one for each layer type
//
wxString ImportDirectory[LAYER_TYPES];


// Helper
wxString GetTempFolderName(const char *base)
{
	// first determine where to put our temporary directory
	vtString path;

	const char *temp = getenv("TEMP");
	if (temp)
		path = temp;
	else
#if WIN32
		path = "C:/TEMP";
#else
		path = "/tmp";
#endif
	path += "/";

	// the create a folder named after the file in the full path "base"
	vtString base2 = StartOfFilename(base);
	path += base2;

	// appended with the word _temp
	path += "_temp";

	return wxString(path, wxConvUTF8);
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
	wxFileDialog loadFile(NULL, _("Import Data"), ImportDirectory[ltype], _T(""), filter, wxOPEN | wxMULTIPLE);
	bool bResult = (loadFile.ShowModal() == wxID_OK);
	if (!bResult)
		return;

	//multiple selection
	wxArrayString strFileNameArray;
	loadFile.GetPaths(strFileNameArray);

	// remember the directory they used
	ImportDirectory[ltype] = loadFile.GetDirectory();

	// TESTING code here
//	ImportDataFromS57(strFileName);

	for (unsigned int i=0; i<strFileNameArray.GetCount(); ++i)
		ImportDataFromArchive(ltype, strFileNameArray.Item(i), true);
}

/**
 * Import data of a given type from a file, which can potentially be an
 * archive file.  If it's an archive, it will be unarchived to a temporary
 * folder, and the contents will be imported.
 */
void MainFrame::ImportDataFromArchive(LayerType ltype, const wxString &fname_in,
									  bool bRefresh)
{
	// check file extension
	wxString fname = fname_in;
	wxString ext = fname.AfterLast('.');

	// check if it's an archive
	bool bGZip = false;
	bool bTGZip = false;
	bool bZip = false;

	if (ext.CmpNoCase(_T("gz")) == 0 || ext.CmpNoCase(_T("bz2")) == 0)
	{
		// We could expand .gz and .bz2 files into a temporary folder, but it
		//  would be inefficient as many of the file readers used gzopen etc.
		//  hence they already support gzipped input efficiently.
		bGZip = true;
	}
	if (ext.CmpNoCase(_T("tgz")) == 0 || ext.CmpNoCase(_T("tar")) == 0 ||
		fname.Right(7).CmpNoCase(_T(".tar.gz")) == 0)
		bTGZip = true;

	if (ext.CmpNoCase(_T("zip")) == 0)
		bZip = true;

	if (!bTGZip && !bZip)
	{
		// simple case
		ImportDataFromFile(ltype, fname, bRefresh);
		return;
	}

	// try to uncompress
	wxString path, prepend_path;
	path = GetTempFolderName(fname_in.mb_str(wxConvUTF8));
	int result;
	result = vtCreateDir(path.mb_str(wxConvUTF8));
	if (result == 0 && errno != EEXIST)
	{
		DisplayAndLog("Couldn't create temporary directory to hold contents of archive.");
		return;
	}
	prepend_path = path + _T("/");

	vtString str1 = fname_in.mb_str(wxConvUTF8);
	vtString str2 = prepend_path.mb_str(wxConvUTF8);

	OpenProgressDialog(_("Expanding archive"), false, this);
	if (bTGZip)
		result = ExpandTGZ(str1, str2);
	if (bZip)
		result = ExpandZip(str1, str2, progress_callback);
	CloseProgressDialog();

	if (result < 1)
	{
		DisplayAndLog("Couldn't expand archive.");
	}
	else if (result == 1)
	{
		// the archive contained a single file
		std::string pathname = prepend_path.mb_str(wxConvUTF8);
		wxString internal_name;
		for (dir_iter it(pathname); it != dir_iter(); ++it)
		{
			if (it.is_directory())
				continue;
			std::string name1 = it.filename();
			fname = prepend_path;
			internal_name = wxString(name1.c_str(), wxConvUTF8);
			fname += internal_name;
			break;
		}
		vtLayer *pLayer = ImportDataFromFile(ltype, fname, bRefresh);
		if (pLayer)
		{
			// use the internal filename, not the archive filename which is temporary
			pLayer->SetLayerFilename(internal_name);
			pLayer->SetImportedFrom(fname_in);
		}
	}
	else if (result > 1)
	{
		int layer_count = 0;
		vtArray<vtLayer *> LoadedLayers;
		vtLayer *pLayer;

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

		// look for an SDTS catalog file
		bool found_cat = false;
		bool found_hdr = false;
		bool found_rt1 = false;
		wxString fname2;
		std::string pathname = prepend_path.mb_str(wxConvUTF8);
		for (dir_iter it(pathname); it != dir_iter(); ++it)
		{
			if (it.is_directory()) continue;
			fname2 = wxString(it.filename().c_str(), wxConvUTF8);
			if (fname2.Right(8).CmpNoCase(_T("catd.ddf")) == 0)
			{
				fname = prepend_path;
				fname += fname2;
				found_cat = true;
				break;
			}
			if (fname2.Right(4).CmpNoCase(_T(".hdr")) == 0)
			{
				ltype = LT_ELEVATION;
				fname = prepend_path;
				fname += fname2;
				found_hdr = true;
				break;
			}
			if (fname2.Right(4).CmpNoCase(_T(".rt1")) == 0)
			{
				found_rt1 = true;
				break;
			}
		}
		if (found_cat || found_hdr)
		{
			pLayer = ImportDataFromFile(ltype, fname, bRefresh);
			if (pLayer)
			{
				pLayer->SetLayerFilename(fname2);
				LoadedLayers.Append(pLayer);
				layer_count++;
			}
		}
		else if (found_rt1)
		{
			layer_count = ImportDataFromTIGER(path);
		}
		else
		{
			// Look through archive for individual files (like .dem)
			std::string path = (const char *) prepend_path.mb_str(wxConvUTF8);
			for (dir_iter it(path); it != dir_iter(); ++it)
			{
				if (it.is_directory()) continue;
				fname2 = wxString(it.filename().c_str(), wxConvUTF8);

				fname = prepend_path;
				fname += fname2;

				// Try importing w/o warning on failure, since it could just
				// be some harmless files in there.
				pLayer = ImportDataFromFile(ltype, fname, bRefresh, false);
				if (pLayer)
				{
					pLayer->SetLayerFilename(fname2);
					LoadedLayers.Append(pLayer);
					layer_count++;
				}
			}
		}
		if (layer_count == 0)
			DisplayAndLog("Don't know what to do with contents of archive.");

		// set the original imported filename
		for (unsigned int i = 0; i < LoadedLayers.GetSize(); i++)
			LoadedLayers[i]->SetImportedFrom(fname_in);
	}

	// clean up after ourselves
	prepend_path = GetTempFolderName(fname_in.mb_str(wxConvUTF8));
	vtDestroyDir(prepend_path.mb_str(wxConvUTF8));
}

/**
 * ImportDataFromFile: the main import method.
 *
 * \param ltype			The Layer type suspected.
 * \param strFileName	The filename.
 * \param bRefresh		True if the GUI should be refreshed after import.
 * \param bWarn			True if the GUI should be warned on failure.
 *
 * \return	True if a layer was created from the given file, or false if
 *			nothing importable was found in the file.
 */
vtLayer *MainFrame::ImportDataFromFile(LayerType ltype, const wxString &strFileName,
								   bool bRefresh, bool bWarn)
{
	VTLOG1("ImportDataFromFile '");
	VTLOG1(strFileName);
	VTLOG1("', type '");
	VTLOG1(GetLayerTypeName(ltype));
	VTLOG1("'\n");

	// check to see if the file is readable
	vtString fname = strFileName.mb_str(wxConvUTF8);
	FILE *fp = vtFileOpen(fname, "rb");
	if (!fp)
	{
		// Cannot Open File
		VTLOG("Couldn't open file %s\n", fname);
		return false;
	}
	fclose(fp);

	wxString msg = _("Importing Data from ");
	msg += strFileName;
	VTLOG(msg.mb_str(wxConvUTF8));
	VTLOG("...\n");
	OpenProgressDialog(msg, true, this);

	// check the file extension
	wxString strExt = strFileName.AfterLast('.');

	// call the appropriate reader
	vtLayerPtr pLayer = NULL;
	switch (ltype)
	{
	case LT_ELEVATION:
		pLayer = ImportElevation(strFileName, bWarn);
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
			wxString strFileName2 = strFileName.Left(len - 8);
			wxString strFileName3 = strFileName2 + _T("rsdf.ddf");
			FILE *fp;
			fp = vtFileOpen(strFileName3.mb_str(wxConvUTF8), "rb");
			if (fp != NULL)
			{
				bRaster = true;
				fclose(fp);
			}
			else
			{
				// also try with upper-case (for Unix)
				strFileName3 = strFileName2 + _T("RSDF.DDF");
				fp = vtFileOpen(strFileName3.mb_str(wxConvUTF8), "rb");
				if (fp != NULL)
				{
					bRaster = true;
					fclose(fp);
				}
			}
		
			if (bRaster)
				pLayer = ImportElevation(strFileName, bWarn);
			else
				pLayer = ImportVectorsWithOGR(strFileName, ltype);
		}
		else if (!strExt.CmpNoCase(_T("shp")) ||
				 !strExt.CmpNoCase(_T("igc")))
		{
			pLayer = new vtRawLayer;
			pLayer->SetLayerFilename(strFileName);
			if (!pLayer->OnLoad())
			{
				delete pLayer;
				pLayer = NULL;
			}
		}
		else if (!strExt.CmpNoCase(_T("bcf")))
		{
			pLayer = ImportFromBCF(strFileName);
		}
		else if (!strExt.CmpNoCase(_T("jpg")))
		{
			pLayer = ImportImage(strFileName);
		}
		else if (!strExt.Left(3).CmpNoCase(_T("ppm")))
		{
			pLayer = ImportImage(strFileName);
		}
		else
		{
			// Many other Elevation formats are supported
			pLayer = ImportElevation(strFileName, bWarn);
		}
		break;
	case LT_UTILITY:
		if(!strExt.CmpNoCase(_T("shp")))
			pLayer = ImportFromSHP(strFileName, ltype);
		break;
	case LT_RAW:
		if (!strExt.CmpNoCase(_T("shp")))
			pLayer = ImportFromSHP(strFileName, ltype);
		else if (!strExt.CmpNoCase(_T("dxf")))
			pLayer = ImportFromDXF(strFileName, ltype);
		else
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
		if (bWarn)
			wxMessageBox(_("Did not import any data from that file."));
		return NULL;
	}
	VTLOG("  import succeeded.\n");

	wxString layer_fname = pLayer->GetLayerFilename();
	if (layer_fname.IsEmpty() || !layer_fname.Cmp(_("Untitled")))
		pLayer->SetLayerFilename(strFileName);

	bool success = AddLayerWithCheck(pLayer, true);
	if (success)
		return pLayer;
	else
	{
		delete pLayer;
		return NULL;
	}
}

//
// type to guess layer type from a DLG file
//
LayerType MainFrame::GuessLayerTypeFromDLG(vtDLGFile *pDLG)
{
	LayerType ltype = LT_UNKNOWN;
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
	filter += vtLayer::LayerTypeNames[ltype];
	filter += _T(" Formats|");

	switch (ltype)
	{
	case LT_RAW:
		// abstract GIS data
		AddType(filter, FSTRING_DXF);
		AddType(filter, FSTRING_IGC);
		AddType(filter, FSTRING_MI);
		AddType(filter, FSTRING_NTF);
		AddType(filter, FSTRING_SHP);
		break;
	case LT_ELEVATION:
		// dem, etc.
		AddType(filter, FSTRING_3TX);
		AddType(filter, FSTRING_ADF);
		AddType(filter, FSTRING_ASC);
		AddType(filter, FSTRING_BIL);
		AddType(filter, FSTRING_CDF);
		AddType(filter, FSTRING_DEM);
		AddType(filter, FSTRING_DTED);
		AddType(filter, FSTRING_DXF);
		AddType(filter, FSTRING_GTOPO);
		AddType(filter, FSTRING_HGT);
		AddType(filter, FSTRING_IMG);
		AddType(filter, FSTRING_MEM);
		AddType(filter, FSTRING_NTF);
		AddType(filter, FSTRING_PGM);
		AddType(filter, FSTRING_PNG);
		AddType(filter, FSTRING_RAW);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_Surfer);
		AddType(filter, FSTRING_TER);
		AddType(filter, FSTRING_TIF);
		AddType(filter, FSTRING_TXT);
		AddType(filter, FSTRING_XYZ);
		AddType(filter, FSTRING_COMP);
		break;
	case LT_IMAGE:
		// bmp, doq, img, ppm, tif
		AddType(filter, FSTRING_BMP);
		AddType(filter, FSTRING_DOQ);
		AddType(filter, FSTRING_IMG);
		AddType(filter, FSTRING_PPM);
		AddType(filter, FSTRING_TIF);
		break;
	case LT_ROAD:
		// dlg, shp, sdts-dlg
		AddType(filter, FSTRING_DLG);
		AddType(filter, FSTRING_SHP);
		AddType(filter, FSTRING_SDTS);
		AddType(filter, FSTRING_MI);
		AddType(filter, FSTRING_COMP);
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


vtLayerPtr MainFrame::ImportFromDLG(const wxString &fname_in, LayerType ltype)
{
	vtDLGFile *pDLG = new vtDLGFile();
	bool success = pDLG->Read(fname_in.mb_str(wxConvUTF8), progress_callback);
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
/*	if (ltype == LT_RAW)
	{
		vtRawLayer *pRL = (vtRawLayer *)pLayer;
		pRL->AddElementsFromDLG(pDLG);
	}
*/
	// now we no longer need the DLG object
	delete pDLG;

	return pLayer;
}

vtLayerPtr MainFrame::ImportFromSHP(const wxString &strFileName, LayerType ltype)
{
	bool success;
	int nShapeType;

	// SHPOpen doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(strFileName.mb_str(wxConvUTF8));

	SHPHandle hSHP = SHPOpen(fname_local, "rb");
	if (hSHP == NULL)
	{
		wxMessageBox(_("Couldn't read that Shape file.  Perhaps it is\nmissing its corresponding .dbf and .shx files."));
		return NULL;
	}
	else
	{
		// Get type of data
		SHPGetInfo(hSHP, NULL, &nShapeType, NULL, NULL);

		// Check Shape Type, Veg Layer should be Poly data
		SHPClose(hSHP);
	}

	// if layer type unknown, ask user input
	if (ltype == LT_UNKNOWN)
	{
		ltype = AskLayerType();
		if (ltype == LT_UNKNOWN)	// User cancelled the operation
			return NULL;
	}

	// create the new layer
	vtLayerPtr pLayer = vtLayer::CreateNewLayer(ltype);

	// Does SHP already have a projection?
	vtProjection proj;
	if (proj.ReadProjFile(strFileName.mb_str(wxConvUTF8)))
	{
		// OK, we'll use it
	}
	else
	{
		// ask user for a projection
		ProjectionDlg dlg(NULL, -1, _("Please indicate projection"));
		dlg.SetProjection(m_proj);

		if (dlg.ShowModal() == wxID_CANCEL)
			return NULL;
		dlg.GetProjection(proj);
	}

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
			wxMessageBox(_("The Shapefile must have either point features\n(for individual plants) or polygon features\n (for plant distribution areas)."));
			return NULL;
		}

		vtVegLayer *pVL = (vtVegLayer *)pLayer;
		if (nShapeType == SHPT_POLYGON)
		{
			ImportVegDlg dlg(this, -1, _("Import Vegetation Information"));
			dlg.SetShapefileName(strFileName);
			if (dlg.ShowModal() == wxID_CANCEL)
				return NULL;
			success = pVL->AddElementsFromSHP_Polys(strFileName, proj,
				dlg.m_fieldindex, dlg.m_datatype);
			if (!success)
				return NULL;
		}
		if (nShapeType == SHPT_POINT)
		{
			VegFieldsDlg dlg(this, -1, _("Map fields to attributes"));
			dlg.SetShapefileName(strFileName);
			dlg.SetVegLayer(pVL);
			if (dlg.ShowModal() == wxID_CANCEL)
				return NULL;
			success = pVL->AddElementsFromSHP_Points(strFileName, proj, dlg.m_options);
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
		pUL->ImportFromSHP(strFileName.mb_str(wxConvUTF8), proj);
	}

	if (ltype == LT_RAW)
	{
		pLayer->SetLayerFilename(strFileName);
		if (pLayer->OnLoad())
			pLayer->SetProjection(proj);
		else
		{
			delete pLayer;
			pLayer = NULL;
		}
	}
	return pLayer;
}

vtLayerPtr MainFrame::ImportFromDXF(const wxString &strFileName, LayerType ltype)
{
	if (ltype == LT_ELEVATION)
	{
		vtElevLayer *pEL = new vtElevLayer;
		if (pEL->ImportFromFile(strFileName))
			return pEL;
		else
			return NULL;
	}
	if (ltype == LT_RAW)
	{
		vtFeatureLoader loader;
		vtFeatureSet *pSet = loader.LoadFromDXF(strFileName.mb_str(wxConvUTF8));
		if (!pSet)
			return NULL;

		// We should ask for a CRS
		vtProjection &Proj = pSet->GetAtProjection();
		if (!GetMainFrame()->ConfirmValidCRS(&Proj))
		{
			delete pSet;
			return NULL;
		}
		vtRawLayer *pRL = new vtRawLayer;
		pRL->SetFeatureSet(pSet);
		return pRL;
	}
	return NULL;
}

vtLayerPtr MainFrame::ImportElevation(const wxString &strFileName, bool bWarn)
{
	vtElevLayer *pElev = new vtElevLayer;

	bool success = pElev->ImportFromFile(strFileName, progress_callback);

	if (success)
		return pElev;
	else
	{
		if (bWarn)
			DisplayAndLog("Couldn't import data from that file.");
		delete pElev;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportImage(const wxString &strFileName)
{
	vtImageLayer *pLayer = new vtImageLayer;

	bool success = pLayer->ImportFromFile(strFileName);

	if (success)
		return pLayer;
	else
	{
		delete pLayer;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportFromLULC(const wxString &strFileName, LayerType ltype)
{
	// Read LULC file, check for errors
	vtLULCFile *pLULC = new vtLULCFile(strFileName.mb_str(wxConvUTF8));
	if (pLULC->m_iError)
	{
		wxString msg(pLULC->GetErrorMessage(), wxConvUTF8);
		wxMessageBox(msg);
		delete pLULC;
		return NULL;
	}

	// If layer type unknown, assume it's veg type
	if (ltype == LT_UNKNOWN)
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

vtStructureLayer *MainFrame::ImportFromBCF(const wxString &strFileName)
{
	vtStructureLayer *pSL = new vtStructureLayer;
	if (pSL->ReadBCF(strFileName.mb_str(wxConvUTF8)))
		return pSL;
	else
	{
		delete pSL;
		return NULL;
	}
}

//
// Import from a Garmin MapSource GPS export file (.txt)
//
void MainFrame::ImportFromMapSource(const char *fname)
{
	FILE *fp = vtFileOpen(fname, "r");
	if (!fp)
		return;

	vtArray<vtRawLayer *> layers;
	char buf[200];
	bool bUTM = false;
	fscanf(fp, "Grid %s\n", buf);
	if (!strcmp(buf, "UTM"))
		bUTM = true;
	fgets(buf, 200, fp); // assume "Datum   WGS 84"

	vtProjection proj;

	bool bGotSRS = false;
	char ch;
	int i;
	vtRawLayer *pRL=NULL;

	while (fgets(buf, 200, fp))	// get a line
	{
		if (!strncmp(buf, "Track\t", 6))
		{
			pRL = new vtRawLayer;
			pRL->SetGeomType(wkbPoint);
			layers.Append(pRL);
			bGotSRS = false;

			// parse name
			char name[40];
			for (i = 6; ; i++)
			{
				ch = buf[i];
				if (ch == '\t' || ch == 0)
					break;
				name[i-6] = ch;
			}
			name[i] = 0;
			pRL->SetLayerFilename(wxString(name, wxConvUTF8));
		}
		if (!strncmp(buf, "Trackpoint", 10))
		{
			int zone;
			DPoint2 p;
			sscanf(buf+10, "%d %c %lf %lf", &zone, &ch, &p.x, &p.y);

			if (!bGotSRS)
			{
				proj.SetWellKnownGeogCS("WGS84");
				if (bUTM)
					proj.SetUTMZone(zone);
				pRL->SetProjection(proj);
				bGotSRS = true;
			}
			pRL->AddPoint(p);
		}
	}

	// Display the list of imported tracks to the user
	int n = layers.GetSize();
	wxString *choices = new wxString[n];
	wxArrayInt selections;
	wxString str;
	for (i = 0; i < n; i++)
	{
		choices[i] = layers[i]->GetLayerFilename();

		choices[i] += _T(" (");
		if (bUTM)
		{
			layers[i]->GetProjection(proj);
			str.Printf(_T("zone %d, "), proj.GetUTMZone());
			choices[i] += str;
		}
		str.Printf(_T("points %d"), layers[i]->GetFeatureSet()->GetNumEntities());
		choices[i] += str;
		choices[i] += _T(")");
	}

	int nsel = wxGetMultipleChoices(selections, _("Which layers to import?"),
		_("Import Tracks"), n, choices);

	// for each of the layers the user wants, add them to our project
	for (i = 0; i < nsel; i++)
	{
		int sel = selections[i];
		AddLayerWithCheck(layers[sel]);
		layers[sel]->SetModified(false);
	}
	// for all the rest, delete 'em
	for (i = 0; i < n; i++)
	{
		if (layers[i]->GetModified())
			delete layers[i];
	}
	delete [] choices;
}

// Helper for following method
double ExtractValue(DBFHandle db, int iRec, int iField, DBFFieldType ftype,
					int iStyle, bool bEasting, bool bFlipEasting)
{
	const char *string;
	switch (ftype)
	{
	case FTString:
		string = DBFReadStringAttribute(db, iRec, iField);
		if (iStyle == 0)	// decimal
		{
			return atof(string);
		}
		else if (iStyle == 1)	// packed DMS
		{
			int deg, min, sec, frac;
			if (bEasting)
			{
				deg  = GetIntFromString(string, 3);
				min  = GetIntFromString(string+3, 2);
				sec  = GetIntFromString(string+5, 2);
				frac = GetIntFromString(string+7, 2);
				if (deg > 180)
				{
					deg  = GetIntFromString(string, 2);
					min  = GetIntFromString(string+2, 2);
					sec  = GetIntFromString(string+4, 2);
					frac = 0;
				}
			}
			else
			{
				deg  = GetIntFromString(string, 2);
				min  = GetIntFromString(string+2, 2);
				sec  = GetIntFromString(string+4, 2);
				frac = GetIntFromString(string+6, 2);
			}
			double secs = sec + (frac/100.0);
			double val = deg + (min/60.0) + (secs/3600.0);
			if (bFlipEasting)
				val = -val;
			return val;
		}
		break;
	case FTInteger:
		return DBFReadIntegerAttribute(db, iRec, iField);
	case FTDouble:
		return DBFReadDoubleAttribute(db, iRec, iField);
	default:
		return 0.0;
	}
	return 0.0;
}

//
// Import point data from a tabular data source such as a .dbf or .csv
//  (Currently, only handles DBF)
//
void MainFrame::ImportDataPointsFromTable(const char *fname)
{
	// DBFOpen doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(fname);

	// Open DBF File
	DBFHandle db = DBFOpen(fname_local, "rb");
	if (db == NULL)
		return;

	ImportPointDlg dlg(this, -1, _("Point Data Import"));

	// default to the current CRS
	dlg.SetCRS(m_proj);

	// Fill the DBF field names into the "Use Field" controls
	int *pnWidth = 0, *pnDecimals = 0;
	char pszFieldName[32];
	int iFields = DBFGetFieldCount(db);
	int i;
	vtArray<DBFFieldType> m_fieldtypes;
	for (i = 0; i < iFields; i++)
	{
		DBFFieldType fieldtype = DBFGetFieldInfo(db, i, pszFieldName,
			pnWidth, pnDecimals );
		wxString str(pszFieldName, wxConvUTF8);

		dlg.GetEasting()->Append(str);
		dlg.GetNorthing()->Append(str);

		m_fieldtypes.Append(fieldtype);
		//if (fieldtype == FTString)
		//	GetChoiceFileField()->Append(str);
		//if (fieldtype == FTInteger || fieldtype == FTDouble)
		//	GetChoiceHeightField()->Append(str);
	}
	if (dlg.ShowModal() != wxID_OK)
	{
		DBFClose(db);
		return;
	}
	int iEast = dlg.m_iEasting;
	int iNorth = dlg.m_iNorthing;
	int iStyle = dlg.m_bFormat2 ? 1 : 0;

	// Now import
	vtFeatureSetPoint2D *pSet = new vtFeatureSetPoint2D();
	pSet->SetProjection(dlg.m_proj);

	int iRecords = DBFGetRecordCount(db);
	for (i = 0; i < iRecords; i++)
	{
		DPoint2 p;
		p.x = ExtractValue(db, i, iEast, m_fieldtypes[iEast], iStyle, true, dlg.m_bLongitudeWest);
		p.y = ExtractValue(db, i, iNorth, m_fieldtypes[iNorth], iStyle, false, false);
		pSet->AddPoint(p);
	}
	DBFClose(db);

	// Also copy along the corresponding DBF data into the new featureset
	pSet->SetFilename(fname);
	pSet->LoadDataFromDBF(fname);

	vtRawLayer *pRaw = new vtRawLayer;
	pRaw->SetFeatureSet(pSet);
	AddLayerWithCheck(pRaw);
}

vtLayerPtr MainFrame::ImportRawFromOGR(const wxString &strFileName)
{
	// create the new layer
	vtRawLayer *pRL = new vtRawLayer;
	bool success = pRL->LoadWithOGR(strFileName.mb_str(wxConvUTF8), progress_callback);

	if (success)
		return pRL;
	else
	{
		delete pRL;
		return NULL;
	}
}

vtLayerPtr MainFrame::ImportVectorsWithOGR(const wxString &strFileName, LayerType ltype)
{
	vtProjection Projection;

	g_GDALWrapper.RequestOGRFormats();

	// OGR doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(strFileName.mb_str(wxConvUTF8));

	OGRDataSource *datasource = OGRSFDriverRegistrar::Open(fname_local);
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
		ImportStructDlgOGR ImportDialog(GetMainFrame(), -1, _("Import Structures"));

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
		pSL->AddElementsFromOGR(datasource, ImportDialog.m_opt, progress_callback);

		pSL->GetProjection(Projection);
		if (OGRERR_NONE != Projection.Validate())
		{
			// Get a projection
			ProjectionDlg dlg(GetMainFrame(), -1, _("Please indicate projection"));
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


//
//Import from TIGER, returns number of layers imported
//
int MainFrame::ImportDataFromTIGER(const wxString &strDirName)
{
	g_GDALWrapper.RequestOGRFormats();

	// OGR doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(strDirName.mb_str(wxConvUTF8));

	OGRDataSource *pDatasource = OGRSFDriverRegistrar::Open(fname_local);
	if (!pDatasource)
		return 0;

	int i, j, feature_count;
	OGRLayer		*pOGRLayer;
	OGRFeature		*pFeature;
	OGRGeometry		*pGeom;
	OGRLineString   *pLineString;
	vtWaterFeature	wfeat;

	// Assume that this data source is a TIGER/Line file
	//
	// Iterate through the layers looking for the ones we care about
	//
	int num_layers = pDatasource->GetLayerCount();
	vtString layername = pDatasource->GetName();

	// create the new layers
	vtWaterLayer *pWL = new vtWaterLayer;
	pWL->SetLayerFilename(wxString(layername + "_water", wxConvUTF8));
	pWL->SetModified(true);

	vtRoadLayer *pRL = new vtRoadLayer;
	pRL->SetLayerFilename(wxString(layername + "_roads", wxConvUTF8));
	pRL->SetModified(true);

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

#if VTDEBUG
		VTLOG("Layer %d/%d, '%s'\n", i, num_layers, defn->GetName());

		// Debug: iterate through the fields
		int field_count1 = defn->GetFieldCount();
		for (j = 0; j < field_count1; j++)
		{
			OGRFieldDefn *field_def1 = defn->GetFieldDefn(j);
			if (field_def1)
			{
				const char *fnameref = field_def1->GetNameRef();
				OGRFieldType ftype = field_def1->GetType();
				VTLOG("  field '%s' type %d\n", fnameref, ftype);
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
		OpenProgressDialog(_("Importing from TIGER..."));

		int index_cfcc = defn->GetFieldIndex("CFCC");
		int fcount = 0;
		while( (pFeature = pOGRLayer->GetNextFeature()) != NULL )
		{
			// make sure we delete the feature no matter how the loop exits
			std::auto_ptr<OGRFeature> ensure_deletion(pFeature);

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
				TNode *n1 = pRL->NewNode();
				n1->m_p.Set(pLineString->getX(0), pLineString->getY(0));

				TNode *n2 = pRL->NewNode();
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
				int num = atoi(cfcc+1);
				bool bSkip = true;
				switch (num)
				{
				case 1:		// Shoreline of perennial water feature
				case 2:		// Shoreline of intermittent water feature
					break;
				case 11:	// Perennial stream or river
				case 12:	// Intermittent stream, river, or wash
				case 13:	// Braided stream or river
					wfeat.m_bIsBody = false;
					bSkip = false;
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
					wfeat.m_bIsBody = true;
					bSkip = false;
					break;
				}
				if (!bSkip)
				{
					wfeat.SetSize(num_points);
					for (j = 0; j < num_points; j++)
					{
						wfeat.SetAt(j, DPoint2(pLineString->getX(j),
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

	int layer_count = 0;
	bool success;
	success = AddLayerWithCheck(pWL, true);
	if (!success)
		delete pWL;
	else
		layer_count++;

	success = AddLayerWithCheck(pRL, true);
	if (!success)
		delete pRL;
	else
		layer_count++;

	return layer_count;
}


void MainFrame::ImportDataFromNTF(const wxString &strFileName)
{
	g_GDALWrapper.RequestOGRFormats();

	// OGR doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(strFileName.mb_str(wxConvUTF8));

	OGRDataSource *pDatasource = OGRSFDriverRegistrar::Open(fname_local);
	if (!pDatasource)
		return;

	// Progress Dialog
	OpenProgressDialog(_T("Importing from NTF..."));

	OGRFeature *pFeature;
	OGRGeometry		*pGeom;
	OGRLineString   *pLineString;
	OGRSpatialReference *pSpatialRef = NULL;

	// create the (potential) new layers
	vtRoadLayer *pRL = new vtRoadLayer;
	pRL->SetLayerFilename(strFileName + _T(";roads"));
	pRL->SetModified(true);

	vtStructureLayer *pSL = new vtStructureLayer;
	pSL->SetLayerFilename(strFileName + _T(";structures"));
	pSL->SetModified(true);

	// Iterate through the layers looking for the ones we care about?
	//
	int i, num_layers = pDatasource->GetLayerCount();
	for (i = 0; i < num_layers; i++)
	{
		OGRLayer *pOGRLayer = pDatasource->GetLayer(i);
		if (!pOGRLayer)
			continue;

		if (pSpatialRef == NULL)
		{
			pSpatialRef = pOGRLayer->GetSpatialRef();
			if (pSpatialRef)
			{
				vtProjection proj;
				proj.SetSpatialReference(pSpatialRef);
				pRL->SetProjection(proj);
				pSL->SetProjection(proj);
			}
		}
#if 0
		// Simply create a raw layer from each OGR layer
		vtRawLayer *pRL = new vtRawLayer;
		if (pRL->CreateFromOGRLayer(pOGRLayer))
		{
			wxString layname = strFileName;
			layname += wxString::Format(_T(";%d"), i);
			pRL->SetLayerFilename(layname);

			bool success = AddLayerWithCheck(pRL, true);
		}
		else
			delete pRL;
#else
		int feature_count = pOGRLayer->GetFeatureCount();
  		pOGRLayer->ResetReading();
		OGRFeatureDefn *defn = pOGRLayer->GetLayerDefn();
		if (!defn)
			continue;
		vtString layer_name = defn->GetName();

		// We depend on feature codes
		int index_fc = defn->GetFieldIndex("FEAT_CODE");
		if (index_fc == -1)
			continue;

		// Points
		if (layer_name == "LANDLINE_POINT" || layer_name == "LANDLINE99_POINT")
		{
		}
		// Lines
		if (layer_name == "LANDLINE_LINE" || layer_name == "LANDLINE99_LINE")
		{
			int fcount = 0;
			while( (pFeature = pOGRLayer->GetNextFeature()) != NULL )
			{
				// make sure we delete the feature no matter how the loop exits
				std::auto_ptr<OGRFeature> ensure_deletion(pFeature);

				UpdateProgressDialog(100 * fcount / feature_count);
				fcount++;

				pGeom = pFeature->GetGeometryRef();
				if (!pGeom) continue;

				if (!pFeature->IsFieldSet(index_fc))
					continue;

				vtString fc = pFeature->GetFieldAsString(index_fc);

				pLineString = (OGRLineString *) pGeom;

				if (fc == "0001")	// Building outline
				{
					vtBuilding *bld = pSL->AddBuildingFromLineString(pLineString);
					if (bld)
					{
						vtBuilding *pDefBld = GetClosestDefault(bld);
						if (pDefBld)
							bld->CopyFromDefault(pDefBld, true);
						else
						{
							bld->SetStories(1);
							bld->SetRoofType(ROOF_FLAT);
						}
					}
				}
				if (fc == "0098")	// Road centerline
				{
					LinkEdit *pLE = pRL->AddRoadSegment(pLineString);
					// some defaults..
					pLE->m_iLanes = 2;
					pLE->m_fWidth = 6.0f;
					pLE->SetFlag(RF_MARGIN, true);
				}
			}
		}
		// Names
		if (layer_name == "LANDLINE_NAME" || layer_name == "LANDLINE99_NAME")
		{
		}
#endif
	}

	bool success;
	success = AddLayerWithCheck(pRL, true);
	if (!success)
		delete pRL;

	success = AddLayerWithCheck(pSL, true);
	if (!success)
		delete pSL;

	delete pDatasource;

	CloseProgressDialog();
}


void MainFrame::ImportDataFromS57(const wxString &strDirName)
{
	g_GDALWrapper.RequestOGRFormats();

	// OGR doesn't yet support utf-8 or wide filenames, so convert
	vtString fname_local = UTF8ToLocal(strDirName.mb_str(wxConvUTF8));

	OGRDataSource *pDatasource = OGRSFDriverRegistrar::Open(fname_local);
	if (!pDatasource)
		return;

	// create the new layers
	vtWaterLayer *pWL = new vtWaterLayer;
	pWL->SetLayerFilename(strDirName + _T("/water"));
	pWL->SetModified(true);

	int i, j, feature_count;
	OGRLayer		*pOGRLayer;
	OGRFeature		*pFeature;
	OGRGeometry		*pGeom;
	OGRLineString   *pLineString;

	vtWaterFeature	wfeat;

	// Assume that this data source is a S57 file
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

#if 0
		//Debug: interate throught the fields
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

		// Get the projection (SpatialReference) from this layer
		OGRSpatialReference *pSpatialRef = pOGRLayer->GetSpatialRef();
		if (pSpatialRef)
		{
			vtProjection proj;
			proj.SetSpatialReference(pSpatialRef);
			pWL->SetProjection(proj);
		}

		// Progress Dialog
		OpenProgressDialog(_T("Importing from S-57..."));

		// Get line features
		const char *layer_name = defn->GetName();
		if (strcmp(layer_name, "Line"))
			continue;

		int fcount = 0;
		while( (pFeature = pOGRLayer->GetNextFeature()) != NULL )
		{
			// make sure we delete the feature no matter how the loop exits
			std::auto_ptr<OGRFeature> ensure_deletion(pFeature);

			UpdateProgressDialog(100 * fcount / feature_count);

			pGeom = pFeature->GetGeometryRef();
			if (!pGeom) continue;

			pLineString = (OGRLineString *) pGeom;
			int num_points = pLineString->getNumPoints();

			if (!strcmp(layer_name, "Line"))
			{
				// Hydrography
				wfeat.SetSize(num_points);
				for (j = 0; j < num_points; j++)
				{
					wfeat.SetAt(j, DPoint2(pLineString->getX(j),
						pLineString->getY(j)));
				}
				pWL->AddFeature(wfeat);
			}

			fcount++;
		}
		CloseProgressDialog();
	}
	delete pDatasource;

	bool success;
	success = AddLayerWithCheck(pWL, true);
	if (!success)
		delete pWL;
}


//
//Import from SCC Viewer Export Format
//
int MainFrame::ImportDataFromSCC(const char *filename)
{
	FILE *fp = vtFileOpen(filename, "rb");
	if (!fp)
		return 0;

	vtString shortname = StartOfFilename(filename);
	RemoveFileExtensions(shortname);

	vtProjection proj;
	proj.SetGeogCSFromDatum(EPSG_DATUM_WGS84);
	proj.SetUTM(1);

	// create the new layers
	vtElevLayer *pEL = new vtElevLayer;
	pEL->SetLayerFilename(wxString(shortname + "_tin", wxConvUTF8));
	pEL->SetModified(true);

	vtStructureLayer *pSL = new vtStructureLayer;
	pSL->SetLayerFilename(wxString(shortname + "_structures", wxConvUTF8));
	pSL->SetModified(true);
	pSL->SetProjection(proj);

	vtVegLayer *pVL = new vtVegLayer;
	pVL->SetModified(true);
	pVL->SetVegType(VLT_Instances);
	pVL->SetLayerFilename(wxString(shortname + "_vegetation", wxConvUTF8));
	pVL->SetProjection(proj);
	vtPlantInstanceArray *pia = pVL->GetPIA();
	pia->SetPlantList(&m_PlantList);
	int id = m_PlantList.GetSpeciesIdByCommonName("Ponderosa Pine");
	vtPlantSpecies *ps = m_PlantList.GetSpecies(id);

	// Progress Dialog
	OpenProgressDialog(_("Importing from SCC..."));

	vtTin2d *tin = new vtTin2d;

	int state = 0;
	int num_mesh, num_tri, color, v, vtx = 0, mesh = 0;
	char buf[400];
	vtString object_type;
	float height;
	vtFence *linear;

	while (fgets(buf, 400, fp) != NULL)
	{
		if (state == 0)	// start of file: number of TIN meshes
		{
			sscanf(buf, "%d", &num_mesh);
			state = 1;
		}
		else if (state == 1)	// header line of mesh
		{
			// Each mesh starts with a header giving the mesh name, number of
			//  triangles in the mesh, mesh color, and mesh texture name
			const char *word = strtok(buf, ",");
			vtString name = word;
			word = strtok(NULL, ",");
			sscanf(word, "%d", &num_tri);
			word = strtok(NULL, ",");
			sscanf(word, "%d", &color);
			word = strtok(NULL, ",");
			state = 2;
			v = 0;
		}
		else if (state == 2)	// vertex of a TIN
		{
			// A vertex comprises of X,Y,Z ordinates, and X, Y, and Z vertex
			//  normals.
			DPoint2 p;
			float z;
			sscanf(buf, "%lf,%lf,%f", &p.x, &p.y, &z);
			tin->AddVert(p, z);
			if ((vtx%3)==2)
				tin->AddTri(vtx-2, vtx-1, vtx);
			vtx++;
			v++;

			UpdateProgressDialog(100 * v / (num_tri*3));

			if (v == num_tri*3)	// last vtx of this mesh
			{
				state = 1;
				mesh++;
				if (mesh == num_mesh)
					state = 4;
			}
		}
		else if (state == 4)	// CONTOUR etc.
		{
			if (!strncmp(buf, "CONTOUR", 7))
			{
				state = 5;
			}
			else if (!strncmp(buf, "OBJECTS", 7))
			{
				// The header line contains the objects layer name, the object
				//  name, the color, and the objects type description.
				const char *word = strtok(buf, ",");
				// first: OBJECTS

				word = strtok(NULL, ",");
				vtString layer_name = word;

				word = strtok(NULL, ",");
				vtString object_name = word;

				word = strtok(NULL, ",");
				sscanf(word, "%d", &color);

				word = strtok(NULL, ",");
				object_type = word;

				state = 6;
			}
			else if (!strncmp(buf, "OBJLINE", 7))
			{
				// the objects layer name, the object name, the color, the
				// objects size in X, Y and Z, and the objects type description.
				const char *word = strtok(buf, ",");
				// first: OBJLINE

				word = strtok(NULL, ",");
				vtString layer_name = word;

				word = strtok(NULL, ",");
				vtString object_name = word;

				word = strtok(NULL, ",");
				sscanf(word, "%d", &color);

				FPoint2 size;
				word = strtok(NULL, ",");
				sscanf(word, "%f", &size.x);
				word = strtok(NULL, ",");
				sscanf(word, "%f", &size.y);
				word = strtok(NULL, ",");
				sscanf(word, "%f", &height);

				word = strtok(NULL, ",");
				object_type = word;

				// Begin a new linear structure
				linear = pSL->AddNewFence();
				if (object_type.Left(5) == "Fence")
					linear->ApplyStyle(FS_WOOD_POSTS_WIRE);
				else if (object_type.Left(5) == "Hedge")
					linear->ApplyStyle(FS_PRIVET);
				else
					// unknown type; use a railing as a placeholder
					linear->ApplyStyle(FS_RAILING_ROW);

				// Apply height and spacing
				vtLinearParams &params = linear->GetParams();
				params.m_fPostHeight = height;
				params.m_fPostSpacing = size.y;

				state = 7;
			}
		}
		else if (state == 5)	// CONTOUR
		{
			if (!strncmp(buf, "ENDCONTOUR", 10))
				state = 4;
		}
		else if (state == 6)	// OBJECTS
		{
			if (!strncmp(buf, "ENDOBJECTS", 10))
				state = 4;
			else
			{
				// X,Y,Z insertion point, followed by the objects size in X, Y
				//  and Z, and object orientation in X,Y, and Z axes.
				DPoint2 p;
				float z;
				FPoint3 size;
				sscanf(buf, "%lf,%lf,%f,%f,%f,%f", &p.x, &p.y, &z,
					&size.x, &size.y, &size.z);

				if (object_type.Left(4) == "Tree")
				{
					pia->AddPlant(p, size.z, ps);
				}
			}
		}
		else if (state == 7)	// OBJLINE
		{
			if (!strncmp(buf, "ENDOBJLINE", 10))
			{
				// Close linear structure
				state = 4;
			}
			else
			{
				// Add X,Y,Z point to linear structure
				DPoint2 p;
				float z;
				sscanf(buf, "%lf,%lf,%f", &p.x, &p.y, &z);
				if (linear)
					linear->AddPoint(p);
			}
		}
	}
	fclose(fp);
	CloseProgressDialog();

	tin->ComputeExtents();
	tin->CleanupClockwisdom();
	pEL->SetTin(tin);
	pEL->SetProjection(proj);

	int layer_count = 0;
	bool success;
	success = AddLayerWithCheck(pEL);
	if (!success)
		delete pEL;
	else
		layer_count++;

	success = AddLayerWithCheck(pSL);
	if (!success)
		delete pSL;
	else
		layer_count++;

	success = AddLayerWithCheck(pVL);
	if (!success)
		delete pVL;
	else
		layer_count++;

	return layer_count;
}

