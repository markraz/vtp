//
// Name:	 EnviroApp.cpp
// Purpose:  The application class for our wxWindows application.
//
// Copyright (c) 2001-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "vtlib/vtlib.h"
#include "vtlib/core/Terrain.h"
#include "../Options.h"
#include "EnviroGUI.h"		// for g_App, GetTerrainScene
#include "vtui/Helper.h"	// for LogWindowsVersion, ProgressDialog
#include "vtdata/vtLog.h"
#include "xmlhelper/easyxml.hpp"

#include "EnviroApp.h"
#include "EnviroFrame.h"
#include "canvas.h"
#include "StartupDlg.h"
#include "TParamsDlg.h"

// Allow customized versions of Enviro to provide their own Frame
#ifdef FRAME_NAME
  #include FRAME_INCLUDE
#else
  #define FRAME_NAME EnviroFrame
  #define LoadAppCatalog(locale)
#endif

IMPLEMENT_APP(EnviroApp)


EnviroApp::EnviroApp()
{
	m_bShowStartupDialog = true;
}

void EnviroApp::Args(int argc, wxChar **argv)
{
	for (int i = 0; i < argc; i++)
	{
		wxString str1 = argv[i];
		vtString str = (const char *) str1.mb_str(wxConvUTF8);
		if (str == "-no_startup_dialog")
			m_bShowStartupDialog = false;
		else if (str.Left(9) == "-terrain=")
			m_bShowStartupDialog = false;
		else if (str.Left(8) == "-locale=")
			m_locale_name = (const char *) str + 8;

		// also let the core application check the command line
		g_App.StartupArgument(i, str);
	}
}


void EnviroApp::SetupLocale()
{
	wxLog::SetVerbose(true);
//	wxLog::AddTraceMask(_T("i18n"));

	// Locale stuff
	int lang = wxLANGUAGE_DEFAULT;
	int default_lang = m_locale.GetSystemLanguage();

	const wxLanguageInfo *info = wxLocale::GetLanguageInfo(default_lang);
	VTLOG("Default language: %d (%s)\n",
		default_lang, (const char *) info->Description.mb_str(wxConvUTF8));

	// After wx2.4.2, wxWidgets looks in the application's directory for
	//  locale catalogs, not the current directory.  Here we force it to
	//  look in the current directory as well.
	wxString cwd = wxGetCwd();
	m_locale.AddCatalogLookupPathPrefix(cwd);

	bool bSuccess=false;
	if (m_locale_name != "")
	{
		VTLOG("Looking up language: %s\n", (const char *) m_locale_name);
		lang = GetLangFromName(wxString(m_locale_name, wxConvUTF8));
		if (lang == wxLANGUAGE_UNKNOWN)
		{
			VTLOG(" Unknown, falling back on default language.\n");
			lang = wxLANGUAGE_DEFAULT;
		}
		else
		{
			info = m_locale.GetLanguageInfo(lang);
			VTLOG("Initializing locale to language %d, Canonical name '%s', Description: '%s':\n",
				lang,
				(const char *) info->CanonicalName.mb_str(wxConvUTF8),
				(const char *) info->Description.mb_str(wxConvUTF8));
			bSuccess = m_locale.Init(lang, wxLOCALE_CONV_ENCODING);
		}
	}
	if (lang == wxLANGUAGE_DEFAULT)
	{
		VTLOG("Initializing locale to default language:\n");
		bSuccess = m_locale.Init(wxLANGUAGE_DEFAULT, wxLOCALE_CONV_ENCODING);
		if (bSuccess)
			lang = default_lang;
	}
	if (bSuccess)
		VTLOG(" succeeded.\n");
	else
		VTLOG(" failed.\n");

	if (lang != wxLANGUAGE_ENGLISH_US)
	{
		VTLOG("Attempting to load the 'Enviro.mo' catalog for the current locale.\n");
		bSuccess = m_locale.AddCatalog(wxT("Enviro"));
		if (bSuccess)
			VTLOG(" succeeded.\n");
		else
			VTLOG(" not found.\n");
		VTLOG("\n");
	}

	// Load any other catalogs which may be specific to this application.
	LoadAppCatalog(m_locale);

	// Test it
//	wxString test = _("&File");

	wxLog::SetVerbose(false);
}

class LogCatcher : public wxLog
{
	void DoLogString(const wxChar *szString, time_t t)
	{
		VTLOG1(" wxLog: ");
		VTLOG1(szString);
		VTLOG1("\n");
	}
};

//
// Initialize the app object
//
bool EnviroApp::OnInit()
{
#if WIN32 && defined(_MSC_VER) && defined(_DEBUG) && IF_NEEDED
	// sometimes, MSVC seems to need to be told to show unfreed memory on exit
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	g_App.Startup();	// starts log

	VTLOG("Specific application name: %s\n", STRING_APPNAME);
	VTLOG("Application framework: wxWindows v" wxVERSION_NUM_DOT_STRING "\n");
#if WIN32
	VTLOG1(" Running on: ");
	LogWindowsVersion();
#endif
	VTLOG1("Build date: ");
	VTLOG1(__DATE__);
	VTLOG1("\n\n");

	if (!g_Options.ReadXML(STRING_APPNAME ".xml"))
	{
		// Look for older .ini file
		g_Options.ReadINI(STRING_APPNAME ".ini");

		// We will always save to xml
		g_Options.m_strFilename = STRING_APPNAME ".xml";
	}

	VTLOG("Datapaths:\n");
	int i, n = g_Options.m_DataPaths.size();
	if (n == 0)
		VTLOG("   none.\n");
	for (i = 0; i < n; i++)
		VTLOG("   %s\n", (const char *) g_Options.m_DataPaths[i]);
	VTLOG1("\n");

	// Redirect the wxWindows log messages to our own logging stream
	wxLog *logger = new LogCatcher();
	wxLog::SetActiveTarget(logger);

	Args(argc, argv);

	SetupLocale();

	// Try to guess GDAL and PROJ.4 data paths, in case the user doesn't have
	//  their GDAL_DATA and PROJ_LIB environment variables set.
	g_GDALWrapper.GuessDataPaths();

/*	class AA { public: virtual void func() {} };
	class BB : public AA {};
	VTLOG("Testing the ability to use dynamic_cast to downcast...\n");
	BB *b = new BB;
	AA *a = (AA *) b;
	BB *result1 = dynamic_cast<BB *>(a);
	VTLOG("  successful (%lx)\n", result1); */

	//
	// Create and show the Startup Dialog
	//
	if (m_bShowStartupDialog)
	{
		// Look for all terrains on all data paths, so that we have a list
		//  of them even before we call vtlib.
		RefreshTerrainList();

		VTLOG("Opening the Startup dialog.\n");
		wxString appname(STRING_APPNAME, wxConvUTF8);
		appname += _(" Startup");
		StartupDlg StartDlg(NULL, -1, appname, wxDefaultPosition);

		StartDlg.GetOptionsFrom(g_Options);
		StartDlg.CenterOnParent();
		int result = StartDlg.ShowModal();
		if (result == wxID_CANCEL)
			return FALSE;

		StartDlg.PutOptionsTo(g_Options);
		g_Options.WriteXML();
	}

	// Now we can create vtTerrain objects for each terrain
	g_App.LoadTerrainDescriptions();

	// Load the global content file, if there is one
	VTLOG("Looking for global content file '%s'\n", (const char *)g_Options.m_strContentFile);
	vtString fname = FindFileOnPaths(g_Options.m_DataPaths, g_Options.m_strContentFile);
	if (fname != "")
	{
		VTLOG("  Loading content file.\n");
		try {
			vtGetContent().ReadXML(fname);
		}
		catch (xh_io_exception &e) {
			string str = e.getFormattedMessage();
			VTLOG("  Error: %s\n", str.c_str());
		}
	}
	else
		VTLOG("  Couldn't find it.\n");

	//
	// Create the main frame window
	//
	wxString title(STRING_APPORG, wxConvUTF8);
#if VTLIB_PSM
	title += _T(" PSM");
#elif VTLIB_OSG
	title += _T(" OSG");
#elif VTLIB_OPENSG
	title += _T(" OpenSG");
#elif VTLIB_SGL
	title += _T(" SGL");
#elif VTLIB_SSG
	title += _T(" SSG");
#endif
	VTLOG1("Creating the frame window.\n");
	wxPoint pos(g_Options.m_WinPos.x, g_Options.m_WinPos.y);
	wxSize size(g_Options.m_WinSize.x, g_Options.m_WinSize.y);
	EnviroFrame *frame = new FRAME_NAME(NULL, title, pos, size);

	// Now we can realize the toolbar
	VTLOG1("Realize toolbar.\n");
	frame->m_pToolbar->Realize();

	// Allow the frame to do something after it's created
	frame->PostConstruction();

	// process some idle messages... let frame open a bit
	bool go = true;
	while (go)
		go = ProcessIdle();

	// Initialize the VTP scene
	vtGetScene()->Init(g_Options.m_bStereo, g_Options.m_iStereoMode);

	// Make sure the scene knows the size of the canvas
	//  (on wxGTK, the first size events arrive too early before the Scene exists)
	int width, height;
	frame->m_canvas->GetClientSize(& width, & height);
	vtGetScene()->SetWindowSize(width, height);

	if (g_Options.m_bLocationInside)
	{
		// they specified the inside (client) location of the window
		// so look at the difference between frame and client sizes
		wxSize size1 = frame->GetSize();
		wxSize size2 = frame->GetClientSize();
		int dx = size1.x - size2.x;
		int dy = size1.y - size2.y;
		frame->SetSize(-1, -1, size1.x + dx, size1.y + dy);
	}

	// Also let the frame see the command-line arguments
	for (int i = 0; i < argc; i++)
	{
		wxString str = argv[i];
		frame->FrameArgument(i, str.mb_str(wxConvUTF8));
	}

	go = true;
	while (go)
		go = ProcessIdle();

	g_App.StartControlEngine();

	if (g_Options.m_bFullscreen)
		frame->SetFullScreen(true);

	return true;
}

int EnviroApp::OnExit()
{
	VTLOG("App Exit\n");
#ifdef VTLIB_PSM
	PSWorld3D::Get()->Stop();
	PSGetScene()->SetWindow(NULL);
#endif
	g_App.Shutdown();
	vtGetScene()->Shutdown();

	return wxApp::OnExit();
}

//
// Look for all terrains on all data paths
//
void EnviroApp::RefreshTerrainList()
{
	vtStringArray &paths = g_Options.m_DataPaths;

	VTLOG("RefreshTerrainList, %d paths:\n", paths.size());

	terrain_files.clear();
	terrain_paths.clear();
	terrain_names.clear();

	bool bShowProgess = paths.size() > 1;
	if (bShowProgess)
		OpenProgressDialog(_("Scanning data paths for terrains"), false, NULL);
	for (unsigned int i = 0; i < paths.size(); i++)
	{
		if (bShowProgess)
			UpdateProgressDialog(i * 100 / paths.size());

		vtString directory = paths[i] + "Terrains";
		for (dir_iter it((const char *)directory); it != dir_iter(); ++it)
		{
			if (it.is_hidden() || it.is_directory())
				continue;

			std::string name1 = it.filename();
			vtString name = name1.c_str();

			// only look for terrain parameters files
			vtString ext = GetExtension(name, false);
			if (ext != ".xml")
				continue;

			TParams params;
			vtString path = directory + "/" + name;
			if (params.LoadFrom(path))
			{
				terrain_files.push_back(name);
				terrain_paths.push_back(path);
				terrain_names.push_back(params.GetValueString(STR_NAME));
			}
		}
	}
	VTLOG("RefreshTerrainList done.\n");
	if (bShowProgess)
		CloseProgressDialog();
}

//
// Ask the user to choose from a list of all loaded terrain.
//
bool EnviroApp::AskForTerrainName(wxWindow *pParent, wxString &strTerrainName)
{
	vtTerrainScene *ts = vtGetTS();
	int num = 0, first_idx = 0;
	std::vector<wxString> choices;

	for (unsigned int i = 0; i < ts->NumTerrains(); i++)
	{
		vtTerrain *terr = ts->GetTerrain(i);
		wxString wstr(terr->GetName(), wxConvUTF8);
		choices.push_back(wstr);
		if (wstr.Cmp(strTerrainName) == 0)
			first_idx = num;
		num++;
	}

	if (!num)
	{
		wxMessageBox(_("No terrains found (datapath/Terrains/*.xml)"));
		return false;
	}

	wxSingleChoiceDialog dlg(pParent, _("Please choose a terrain"),
		_("Select Terrain"), num, &(choices.front()));
	dlg.SetSelection(first_idx);

	if (dlg.ShowModal() == wxID_OK)
	{
		strTerrainName = dlg.GetStringSelection();
		return true;
	}
	else
		return false;
}

vtString EnviroApp::GetIniFileForTerrain(const vtString &name)
{
	for (unsigned int i = 0; i < terrain_files.size(); i++)
	{
		if (name == terrain_names[i])
			return terrain_paths[i];
	}
	return vtString("");
}

int EditTerrainParameters(wxWindow *parent, const char *filename)
{
	VTLOG("EditTerrainParameters '%s'\n", filename);

	vtString fname = filename;

	TParamsDlg dlg(parent, -1, _("Terrain Creation Parameters"), wxDefaultPosition);
	dlg.SetDataPaths(g_Options.m_DataPaths);

	TParams Params;
	if (!Params.LoadFrom(fname))
	{
		wxMessageBox(_("Couldn't load from that file."));
		return wxID_CANCEL;
	}
	dlg.SetParams(Params);
	dlg.CenterOnParent();
	int result = dlg.ShowModal();
	if (result == wxID_OK)
	{
		dlg.GetParams(Params);

		vtString ext = GetExtension(fname, false);
		if (ext.CompareNoCase(".ini") == 0)
		{
			wxString str = _("Upgrading the .ini to a .xml file.\n");
			str += _("Deleting old file: ");
			str += wxString(fname, wxConvUTF8);
			wxMessageBox(str);

			// Try to get rid of it.  Hope they aren't on read-only FS.
			vtDeleteFile(fname);

			fname = fname.Left(fname.GetLength()-4) + ".xml";
		}

		if (!Params.WriteToXML(fname, STR_TPARAMS_FORMAT_NAME))
		{
			wxString str;
			str.Printf(_("Couldn't save to file %hs.\n"), (const char *)fname);
			str += _("Please make sure the file is not read-only.");
			wxMessageBox(str);
			result = wxID_CANCEL;
		}
	}
	return result;
}
