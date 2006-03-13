//
// Name: LayerDlg.cpp
//
// Copyright (c) 2003-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifdef __GNUG__
	#pragma implementation "LayerDlg.cpp"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#include "vtlib/vtlib.h"
#include "vtlib/core/Terrain.h"
#include "vtlib/core/Globe.h"
#include "vtui/wxString2.h"
#include "vtdata/vtLog.h"
#include "EnviroGUI.h"  // for GetCurrentTerrain

#include "LayerDlg.h"

#if defined(__WXGTK__) || defined(__WXMOTIF__) || defined(__WXMAC__)
#  include "building.xpm"
#  include "road.xpm"
#  include "veg1.xpm"
#  include "raw.xpm"
#  include "fence.xpm"
#  include "instance.xpm"
#  include "icon8.xpm"
#endif

#define ICON_BUILDING	0
#define ICON_ROAD		1
#define ICON_VEG1		2
#define ICON_RAW		3
#define ICON_FENCE		4
#define ICON_INSTANCE	5
#define ICON_TOP		6

/////////////////////////////

// WDR: class implementations

//----------------------------------------------------------------------------
// LayerDlg
//----------------------------------------------------------------------------

// WDR: event table for LayerDlg

BEGIN_EVENT_TABLE(LayerDlg,wxDialog)
	EVT_INIT_DIALOG (LayerDlg::OnInitDialog)
	EVT_TREE_SEL_CHANGED( ID_LAYER_TREE, LayerDlg::OnSelChanged )
	EVT_CHECKBOX( ID_SHOW_ALL, LayerDlg::OnShowAll )
	EVT_CHECKBOX( ID_LAYER_VISIBLE, LayerDlg::OnVisible )
	EVT_CHECKBOX( ID_SHADOW_VISIBLE, LayerDlg::OnShadowVisible )
	EVT_BUTTON( ID_LAYER_ZOOM_TO, LayerDlg::OnZoomTo )
	EVT_BUTTON( ID_LAYER_SAVE, LayerDlg::OnLayerSave )
	EVT_BUTTON( ID_LAYER_SAVE_AS, LayerDlg::OnLayerSaveAs )
	EVT_BUTTON( ID_LAYER_CREATE, LayerDlg::OnLayerCreate )
	EVT_BUTTON( ID_LAYER_REMOVE, LayerDlg::OnLayerRemove )
END_EVENT_TABLE()

LayerDlg::LayerDlg( wxWindow *parent, wxWindowID id, const wxString &title,
	const wxPoint &position, const wxSize& size, long style ) :
	wxDialog( parent, id, title, position, size, style )
{
	// WDR: dialog function LayerDialogFunc for LayerDlg
	LayerDialogFunc( this, TRUE );

	m_pTree = GetTree();
	m_bShowAll = false;
	m_imageListNormal = NULL;

	CreateImageList(16);
}

LayerDlg::~LayerDlg()
{
	delete m_imageListNormal;
}

void LayerDlg::CreateImageList(int size)
{
	delete m_imageListNormal;

	if ( size == -1 )
	{
		m_imageListNormal = NULL;
		return;
	}
	// Make an image list containing small icons
	m_imageListNormal = new wxImageList(size, size, TRUE);

	wxIcon icons[7];
	icons[0] = wxICON(building);
	icons[1] = wxICON(road);
	icons[2] = wxICON(veg1);
	icons[3] = wxICON(raw);
	icons[4] = wxICON(fence);
	icons[5] = wxICON(instance);
	icons[6] = wxICON(icon8);

	int sizeOrig = icons[0].GetWidth();
	for ( size_t i = 0; i < WXSIZEOF(icons); i++ )
	{
		if ( size == sizeOrig )
			m_imageListNormal->Add(icons[i]);
		else
			m_imageListNormal->Add(wxBitmap(wxBitmap(icons[i]).ConvertToImage().Rescale(size, size)));
	}
	m_pTree->SetImageList(m_imageListNormal);
}


void LayerDlg::SetShowAll(bool bTrue)
{
	m_bShowAll = bTrue;
	GetShowAll()->SetValue(bTrue);
}

//
// For an item in the tree which corresponds to an actual structure,
//  return the node associated with that structure.
//
vtNode *LayerDlg::GetNodeFromItem(wxTreeItemId item, bool bContainer)
{
	if (!item.IsOk())
		return NULL;

	LayerItemData *data = (LayerItemData *)m_pTree->GetItemData(item);
	if (!data)
		return NULL;
	if (data->m_item == -1)
		return NULL;

	vtStructure3d *str3d = data->m_sa->GetStructure3d(data->m_item);
	vtStructure *str = data->m_sa->GetAt(data->m_item);
	vtStructureType typ = str->GetType();

	if (bContainer && typ != ST_LINEAR)
		return str3d->GetContainer();
	else
		// always get contained geometry for linears; they have no container
		return str3d->GetContained();
}

vtStructureArray3d *LayerDlg::GetStructureArray3dFromItem(wxTreeItemId item)
{
	if (!item.IsOk())
		return NULL;
	LayerItemData *data = (LayerItemData *)m_pTree->GetItemData(item);
	if (!data)
		return NULL;
	if (data->m_item == -1)
		return data->m_sa;
	return NULL;
}

LayerItemData *LayerDlg::GetLayerDataFromItem(wxTreeItemId item)
{
	if (!item.IsOk())
		return NULL;
	LayerItemData *data = (LayerItemData *)m_pTree->GetItemData(item);
	return data;
}

void LayerDlg::OnInitDialog(wxInitDialogEvent& event)
{
	RefreshTreeContents();
	m_item = m_pTree->GetSelection();
	UpdateEnabling();

	wxWindow::OnInitDialog(event);
}

void LayerDlg::RefreshTreeContents()
{
	if (!m_pTree)
		return;

	// start with a blank slate
	m_pTree->DeleteAllItems();

	switch (g_App.m_state)
	{
	case AS_Terrain:
		RefreshTreeTerrain();
		break;
	case AS_Orbit:
		RefreshTreeSpace();
		break;
	default:
		break;
	}
}

void LayerDlg::RefreshTreeTerrain()
{
	g_pLayerSizer1->Show(g_pLayerSizer2, true);
	g_pLayerSizer1->Layout();

	vtTerrain *terr = GetCurrentTerrain();
	if (!terr)
		return;

	m_root = m_pTree->AddRoot(_("Layers"), ICON_TOP, ICON_TOP);

	wxString2 str;
	vtString vs;
	unsigned int i, j;
	StructureSet &set = terr->GetStructureSet();
	vtStructureArray3d *sa;
	for (i = 0; i < set.GetSize(); i++)
	{
		sa = set[i];

		str = sa->GetFilename();
		wxTreeItemId hLayer = m_pTree->AppendItem(m_root, str, ICON_BUILDING, ICON_BUILDING);
		if (sa == terr->GetStructures())
			m_pTree->SetItemBold(hLayer, true);
		m_pTree->SetItemData(hLayer, new LayerItemData(sa, i, -1));

		wxTreeItemId hItem;
		if (m_bShowAll)
		{
			for (j = 0; j < sa->GetSize(); j++)
			{
				if (sa->GetBuilding(j))
					hItem = m_pTree->AppendItem(hLayer, _("Building"), ICON_BUILDING, ICON_BUILDING);
				if (sa->GetFence(j))
					hItem = m_pTree->AppendItem(hLayer, _("Fence"), ICON_FENCE, ICON_FENCE);
				if (vtStructInstance *inst = sa->GetInstance(j))
				{
					vs = inst->GetValueString("filename", true, true);
					if (vs != "")
					{
						str = "File ";
						str += vs;
					}
					else
					{
						vs = inst->GetValueString("itemname", false, true);
						str = "Item ";
						str += vs;
					}
					hItem = m_pTree->AppendItem(hLayer, str, ICON_INSTANCE, ICON_INSTANCE);
				}
				m_pTree->SetItemData(hItem, new LayerItemData(sa, i, j));
			}
		}
		else
		{
			int bld = 0, fen = 0, inst = 0;
			for (j = 0; j < sa->GetSize(); j++)
			{
				if (sa->GetBuilding(j)) bld++;
				if (sa->GetFence(j)) fen++;
				if (sa->GetInstance(j)) inst++;
			}
			if (bld)
			{
				str.Printf(_("Buildings: %d"), bld);
				hItem = m_pTree->AppendItem(hLayer, str, ICON_BUILDING, ICON_BUILDING);
				m_pTree->SetItemData(hItem, new LayerItemData(sa, i, -1));
			}
			if (fen)
			{
				str.Printf(_("Fences: %d"), fen);
				hItem = m_pTree->AppendItem(hLayer, str, ICON_FENCE, ICON_FENCE);
				m_pTree->SetItemData(hItem, new LayerItemData(sa, i, -1));
			}
			if (inst)
			{
				str.Printf(_("Instances: %d"), inst);
				hItem = m_pTree->AppendItem(hLayer, str, ICON_INSTANCE, ICON_INSTANCE);
				m_pTree->SetItemData(hItem, new LayerItemData(sa, i, -1));
			}
		}
		m_pTree->Expand(hLayer);
	}

	// Now, abstract layers
	vtAbstractLayers &raw = terr->GetAbstractLayers();
	for (i = 0; i < raw.GetSize(); i++)
	{
		vtFeatureSet *set = raw[i];

		vs = set->GetFilename();
		str.from_utf8(vs);

		str += _(" (Type: ");
		str += OGRGeometryTypeToName(set->GetGeomType());

		str += _(", Features: ");
		vs.Format("%d", set->GetNumEntities());
		str += vs;
		str += _T(")");

		wxTreeItemId hLayer = m_pTree->AppendItem(m_root, str, ICON_RAW, ICON_RAW);
		//if (sa == terr->GetStructures())
		//	m_pTree->SetItemBold(hLayer, true);
		//m_pTree->SetItemData(hLayer, new LayerItemData(sa, i, -1));
	}

	// Vegetation
	if (terr->GetPlantList())
	{
		vtPlantInstanceArray3d &pia = terr->GetPlantInstances();
		if (pia.GetNumEntities() > 0)
		{
			vs = pia.GetFilename();
			str.from_utf8(vs);

			str += _(" (Plants: ");
			vs.Format("%d", pia.GetNumEntities());
			str += vs;
			str += _T(")");

			wxTreeItemId hLayer = m_pTree->AppendItem(m_root, str, ICON_VEG1, ICON_VEG1);
		}
	}

	m_pTree->Expand(m_root);
}

void LayerDlg::RefreshTreeStateTerrain()
{
	vtTerrain *terr = GetCurrentTerrain();
	if (!terr)
		return;

	StructureSet &set = terr->GetStructureSet();

	wxTreeItemIdValue cookie;
	wxTreeItemId id;
	int count = 0;
	for (id = m_pTree->GetFirstChild(m_root, cookie);
		id.IsOk();
		id = m_pTree->GetNextChild(m_root, cookie))
	{
		if (set[count] == terr->GetStructures())
			m_pTree->SetItemBold(id, true);
		else
			m_pTree->SetItemBold(id, false);
		count++;
	}
}

void LayerDlg::RefreshTreeSpace()
{
	g_pLayerSizer1->Show(g_pLayerSizer2, false);
	g_pLayerSizer1->Layout();

	IcoGlobe *globe = g_App.GetGlobe();
	if (!globe)
		return;

	wxTreeItemId hRoot = m_pTree->AddRoot(_("Layers"));

	vtFeaturesSet &feats = globe->GetFeaturesSet();
	for (unsigned int i = 0; i < feats.GetSize(); i++)
	{
		wxString2 str;
		vtFeatureSet *feat = feats[i];

		str = feat->GetFilename();
		wxTreeItemId hItem = m_pTree->AppendItem(hRoot, str, -1, -1);

		OGRwkbGeometryType type = feat->GetGeomType();
		int num = feat->GetNumEntities();
		str.Printf(_T("%d "), num);
		if (type == wkbPoint)
			str += _T("Point");
		if (type == wkbPoint25D)
			str += _T("PointZ");
		if (type == wkbLineString)
			str += _T("Arc");
		if (type == wkbPolygon)
			str += _T("Polygon");
		str += _T(" Feature");
		if (num != 1)
			str += _T("s");
		m_pTree->AppendItem(hItem, str, -1, -1);
		m_pTree->Expand(hItem);
	}
	m_pTree->Expand(hRoot);
}

// WDR: handler implementations for LayerDlg

void LayerDlg::OnLayerRemove( wxCommandEvent &event )
{
	LayerItemData *data = GetLayerDataFromItem(m_item);
	if (!data)
		return;

	if (data->m_sa != NULL)
	{
		GetCurrentTerrain()->DeleteStructureSet(data->m_index);
		RefreshTreeContents();
	}
}

void LayerDlg::OnLayerCreate( wxCommandEvent &event )
{
	vtTerrain *pTerr = GetCurrentTerrain();
	if (!pTerr)
		return;

	vtStructureArray3d *sa = pTerr->NewStructureArray();
	sa->SetFilename("Untitled.vtst");
	sa->m_proj = pTerr->GetProjection();
	RefreshTreeContents();
}

void LayerDlg::OnLayerSave( wxCommandEvent &event )
{
	g_App.SaveStructures(false);	// don't ask for filename
	RefreshTreeContents();
}

void LayerDlg::OnLayerSaveAs( wxCommandEvent &event )
{
	g_App.SaveStructures(true);		// ask for filename
	RefreshTreeContents();
}

void LayerDlg::OnZoomTo( wxCommandEvent &event )
{
	vtNode *pThing = GetNodeFromItem(m_item, true);	// get container
	if (pThing)
	{
		FSphere sphere;
		pThing->GetBoundSphere(sphere, true);   // get global bounds
		vtCamera *pCam = vtGetScene()->GetCamera();

		// Put the camera a bit back from the sphere; sufficiently so that
		//  the whole volume of the bounding sphere is visible.
		float smallest = min(pCam->GetFOV(), pCam->GetVertFOV());
		float alpha = smallest / 2.0f;
		float distance = sphere.radius / tanf(alpha);
		pCam->Identity();
		pCam->Rotate2(FPoint3(1,0,0), -PID2f/2);	// tilt down a little
		pCam->Translate1(sphere.center);
		pCam->TranslateLocal(FPoint3(0.0f, 0.0f, distance));
	}
}


void LayerDlg::OnShadowVisible( wxCommandEvent &event)
{
	bool bVis = event.IsChecked();

	vtNode *pThing = GetNodeFromItem(m_item);
	if (pThing) {
		vtGetScene()->ShadowVisibleNode(pThing, bVis);
	}

	vtStructureArray3d *sa = GetStructureArray3dFromItem(m_item);
	if (sa) {
		for (unsigned int j = 0; j < sa->GetSize(); j++) {
			vtStructure3d *str3d = sa->GetStructure3d(j);
			if (str3d) {
				pThing = str3d->GetContained();
				if (pThing)
					vtGetScene()->ShadowVisibleNode(pThing, bVis);
			}
		}
		LayerItemData *data = GetLayerDataFromItem(m_item);
		if (data)
			data->shadow_last_visible = bVis;
	}
}

void LayerDlg::OnVisible( wxCommandEvent &event )
{
	bool bVis = event.IsChecked();

	vtNode *pThing = GetNodeFromItem(m_item);
	if (pThing)
	{
		pThing->SetEnabled(bVis);
		return;
	}
	vtStructureArray3d *sa = GetStructureArray3dFromItem(m_item);
	if (sa)
	{
		sa->SetEnabled(bVis);
		LayerItemData *data = GetLayerDataFromItem(m_item);
		if (data)
			data->last_visible = bVis;
	}
}

void LayerDlg::OnShowAll( wxCommandEvent &event )
{
	m_bShowAll = event.IsChecked();
	RefreshTreeContents();
	m_item = m_pTree->GetSelection();
	UpdateEnabling();
}

void LayerDlg::OnSelChanged( wxTreeEvent &event )
{
	m_item = event.GetItem();

	LayerItemData *data = GetLayerDataFromItem(m_item);
	VTLOG("OnSelChanged, item %d, data %d\n", m_item.m_pItem, data);
	if (data && data->m_sa != NULL)
	{
		int newindex = data->m_index;
		int oldindex = GetCurrentTerrain()->GetStructureIndex();
		if (newindex != oldindex)
		{
			GetCurrentTerrain()->SetStructureIndex(newindex);
			RefreshTreeStateTerrain();
		}
	}

	UpdateEnabling();
}

void LayerDlg::UpdateEnabling()
{
	vtNode *pThing = GetNodeFromItem(m_item);
	vtStructureArray3d *sa = GetStructureArray3dFromItem(m_item);
	LayerItemData *data = GetLayerDataFromItem(m_item);

	GetZoomTo()->Enable(pThing != NULL);
	GetVisible()->Enable((pThing != NULL) || (sa != NULL));
	GetShadow()->Enable((pThing != NULL) || (sa != NULL));

	if (pThing)
		GetVisible()->SetValue(pThing->GetEnabled());
	if (sa)
	{
		if (data) {
			GetVisible()->SetValue(data->last_visible);
			GetShadow()->SetValue(data->shadow_last_visible);
		}
	}

	GetLayerRemove()->Enable(sa != NULL);
	GetLayerSave()->Enable(sa != NULL);
	GetLayerSaveAs()->Enable(sa != NULL);
}

