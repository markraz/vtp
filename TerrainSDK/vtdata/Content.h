//
// Content.h
//
// Header for the Content Management classes.
//
// Copyright (c) 2001 Virtual Terrain Project.
// Free for all uses, see license.txt for details.
//

#ifndef VTDATA_CONTENTH
#define VTDATA_CONTENTH

#include "vtString.h"
#include "Array.h"
#include "Array.inl"

/**
 * vtModel contains a reference to a 3d polygonal model: a filename, the
 * scale of the model, and the distance at which this LOD should be used.
 */
class vtModel
{
public:
	vtModel()
	{
		m_distance = 0.0f;
		m_scale = 1.0f;
		m_attempted_load = false;
	}

	vtString	m_filename;
	float		m_distance;
	float		m_scale;	// meters per unit (e.g. cm = .01)
	bool		m_attempted_load;
};

/**
 * Each tag has two strings: a Name and a Value.
 * This is similar to the concept of a tag in XML.
 */
struct vtTag
{
	vtString name;
	vtString value;
};

/**
 * A simple set of tags.  Each tag (vtTag) has two strings: a Name and a Value.
 * This is similar to the concept of a tag in XML.
 * \par
 * If this gets used for something more performance-sensitive, we could replace
 * the linear lookup with a hash map.
 */
class vtTagArray
{
public:
	void AddTag(vtTag *pTag)
	{
		m_tags.Append(pTag);
	}
	vtTag *FindTag(const char *szTagName);
	vtTag *GetTag(int index)
	{
		return m_tags.GetAt(index);
	}
	int NumTags()
	{
		return m_tags.GetSize();
	}
	void RemoveTag(int index)
	{
		m_tags.RemoveAt(index);
	}

	void SetValue(const char *szTagName, const char *szValue);
	const char *GetValue(const char *szTagName);

protected:
	Array<vtTag*>	m_tags;
};

/**
 * Represents a "culture" item.  A vtItem has a name and any number of tags
 * which provide description.  It also contains a set of models (vtModel)
 * which are polygonal models of the item at various LOD.
 */
class vtItem : public vtTagArray
{
public:
	~vtItem();

	void Empty();
	void AddModel(vtModel *item) { m_models.Append(item); }
	void RemoveModel(vtModel *model);
	int NumModels() { return m_models.GetSize(); }
	vtModel *GetModel(int i) { return m_models.GetAt(i); }

	vtString		m_name;

protected:
	Array<vtModel*>	m_models;
};

/**  The vtContentManager class keeps a list of 3d models,
 * along with information about what they are and how they should be loaded.
 * It consists of a set of Content Items (vtItem) which each represent a
 * particular object, which in turn consist of Models (vtModel) which are a
 * particular 3d geometry for that Item.  An Item can have several Models
 * which represent different levels of detail (LOD).
 * \par
 * To load a set of content  from a file, first create an empty
 * vtContentManager object, then call ReadXML() with the name of name of a
 * VT Content file (.vtco).
 */
class vtContentManager
{
public:
	~vtContentManager();

	void ReadXML(const char *filename);
	void WriteXML(const char *filename);

	void Empty();
	void AddItem(vtItem *item) { m_items.Append(item); }
	void RemoveItem(vtItem *item);
	int NumItems() { return m_items.GetSize(); }
	vtItem *GetItem(int i) { return m_items.GetAt(i); }
	vtItem *FindItemByName(const char *name);
	vtItem *FindItemByType(const char *type, const char *subtype);

protected:
	Array<vtItem*>	m_items;
};

#endif // VTDATA_CONTENTH

