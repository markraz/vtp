//
// Name:		frame.h
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef FRAMEH
#define FRAMEH

#include <wx/dnd.h>			// for wxFileDropTarget
#include <wx/splitter.h>	// for wxSplitterWindow
#include "vtdata/Content.h"
#include "vtdata/FilePath.h"
#include "vtui/wxString2.h"
#include <map>

class vtGLCanvas;
class MyTreeCtrl;
class vtNode;
class vtTransform;
class SceneGraphDlg;
class PropDlg;
class ModelDlg;
class vtGroup;
class vtLOD;
class vtGeom;
class ItemGroup;
class vtFont;

class Splitter2 : public wxSplitterWindow
{
public:
	Splitter2(wxWindow *parent, wxWindowID id = -1,
			  const wxPoint& pos = wxDefaultPosition,
			  const wxSize& size = wxDefaultSize,
			  long style = wxSP_3D|wxCLIP_CHILDREN,
			  const wxString& name = _T("splitter")) :
		wxSplitterWindow(parent, id, pos, size, style, name)
	{
		bResetting = false;
	}
	virtual void SizeWindows();

	bool bResetting;
	int m_last;
};


// some shortcuts
#define ADD_TOOL(id, bmp, tooltip, tog)	 \
	m_pToolbar->AddTool(id, bmp, wxNullBitmap, tog, -1, -1, (wxObject *)0, tooltip, tooltip)

class vtFrame: public wxFrame
{
public:
	vtFrame(wxFrame *frame, const wxString& title, const wxPoint& pos, const wxSize& size,
		long style = wxDEFAULT_FRAME_STYLE);
	~vtFrame();

protected:
	void CreateMenus();
	void CreateToolbar();
	void ReadINI();

	// command handlers
	void OnClose(wxCloseEvent &event);
	void OnOpen(wxCommandEvent& event);
	void OnSave(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnTestXML(wxCommandEvent& event);
	void OnSetDataPath(wxCommandEvent& event);
	void OnItemNew(wxCommandEvent& event);
	void OnItemDelete(wxCommandEvent& event);
	void OnItemAddModel(wxCommandEvent& event);
	void OnItemRemoveModel(wxCommandEvent& event);
	void OnItemSaveSOG(wxCommandEvent& event);
	void OnSceneGraph(wxCommandEvent& event);
	void OnViewOrigin(wxCommandEvent& event);
	void OnUpdateViewOrigin(wxUpdateUIEvent& event);
	void OnViewRulers(wxCommandEvent& event);
	void OnUpdateViewRulers(wxUpdateUIEvent& event);
	void OnHelpAbout(wxCommandEvent& event);

	void OnUpdateItemAddModel(wxUpdateUIEvent& event);
	void OnUpdateItemRemoveModel(wxUpdateUIEvent& event);
	void OnUpdateItemSaveSOG(wxUpdateUIEvent& event);

	void LoadContentsFile(const wxString2 &fname);
	void SaveContentsFile(const wxString2 &fname);
	void FreeContents();

	void DisplayMessageBox(const wxString2 &str);

public:
	vtGLCanvas		*m_canvas;
	wxToolBar		*m_pToolbar;

	wxSplitterWindow *m_splitter;
	Splitter2		 *m_splitter2;
	MyTreeCtrl		*m_pTree;		// left child of splitter

	// Modeless dialogs
	SceneGraphDlg	*m_pSceneGraphDlg;
	PropDlg			*m_pPropDlg;
	ModelDlg		*m_pModelDlg;

public:
	void RenderingPause();
	void RenderingResume();
	void AddModelFromFile(const wxString2 &fname);
	int GetModelTriCount(vtModel *model);
	void OnChar(wxKeyEvent& event);

public:
	void		UpdateCurrentModelLOD();
	void		UpdateScale(vtModel *model);
	void		UpdateTransform(vtModel *model);
	void		RefreshTreeItems();

	// Models
	void		SetCurrentItemAndModel(vtItem *item, vtModel *model);
	void		SetCurrentItem(vtItem *item);
	void		SetCurrentModel(vtModel *mod);
	vtModel		*AddModel(const wxString2 &fname);
	vtTransform	*AttemptLoad(vtModel *model);
	ItemGroup	*GetItemGroup(vtItem *item);
	void		UpdateItemGroup(vtItem *item);
	void		ShowItemGroupLOD(bool bTrue);
	void		AddNewItem();
	void		DisplayCurrentModel();
	void		ZoomToCurrentModel();
	void		ZoomToModel(vtModel *model);
	void		UpdateWidgets();

	void		DisplayCurrentItem();
	void		ZoomToCurrentItem();

	vtContentManager	m_Man;
	vtItem				*m_pCurrentItem;
	vtModel				*m_pCurrentModel;
	vtFont				*m_pFont;

	std::map<vtItem *, ItemGroup *> m_itemmap;
	std::map<vtModel *, vtTransform *> m_nodemap;

	static vtStringArray m_DataPaths;

	bool m_bShowOrigin;
	bool m_bShowRulers;

	DECLARE_EVENT_TABLE()
};

class DnDFile : public wxFileDropTarget
{
public:
	virtual bool OnDropFiles(wxCoord x, wxCoord y,
		const wxArrayString &filenames);
};

// Helper
extern vtFrame *GetMainFrame();

#endif	// FRAMEH

