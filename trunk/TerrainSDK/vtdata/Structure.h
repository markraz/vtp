//
// Structure.h
//
// Implements the vtStructure class which represents a single built structure.
//
// Copyright (c) 2001-2003 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//
/** \file Structure.h */


#ifndef STRUCTUREH
#define STRUCTUREH

#include "MathTypes.h"
#include "Selectable.h"
#include "Content.h"

class vtBuilding;
class vtFence;
class vtStructInstance;

// Well known material names
#define BMAT_NAME_PLAIN			"Plain"
#define BMAT_NAME_WOOD			"Wood"
#define BMAT_NAME_SIDING		"Siding"
#define BMAT_NAME_BRICK			"Brick"
#define BMAT_NAME_PAINTED_BRICK	"Painted-Brick"
#define BMAT_NAME_ROLLED_ROOFING "Rolled-Roofing"
#define BMAT_NAME_CEMENT		"Cement"
#define BMAT_NAME_CORRUGATED	"Corrugated"
#define BMAT_NAME_DOOR			"Door"
#define BMAT_NAME_WINDOW		"Window"
#define BMAT_NAME_WINDOWWALL	"WindowWall"

enum vtMaterialColorEnum
{
	VT_MATERIAL_COLOURED,
	VT_MATERIAL_SELFCOLOURED_TEXTURE,
	VT_MATERIAL_COLOURABLE_TEXTURE
};

/**
 * This class encapsulates the description of a shared material
 */
class vtMaterialDescriptor
{
public:
	vtMaterialDescriptor();
	vtMaterialDescriptor(const char *name,
					const vtString &SourceName,
					const vtMaterialColorEnum Colorable = VT_MATERIAL_SELFCOLOURED_TEXTURE,
					const float UVScale = 0.0,
					RGBi Color = RGBi(0,0,0));
	~vtMaterialDescriptor();

	void SetName(const vtString& Name)
	{
		m_pName = &Name;
	}
	const vtString& GetName() const
	{
		return *m_pName;
	}
	void SetType(int type)
	{
		m_Type = type;
	}
	int GetType() const
	{
		return m_Type;
	}
	void SetUVScale(const float fScale)
	{
		m_fUVScale = fScale;
	}
	const float GetUVScale() const
	{
		return m_fUVScale;
	}
	void SetMaterialIndex(const int Index)
	{
		m_iMaterialIndex = Index;
	}
	const int GetMaterialIndex() const
	{
		return m_iMaterialIndex;
	}
	void SetColorable(const vtMaterialColorEnum Type)
	{
		m_Colorable = Type;
	}
	const vtMaterialColorEnum GetColorable() const
	{
		return m_Colorable;
	}
	void SetSourceName(const vtString &SourceName)
	{
		m_SourceName = SourceName;
	}
	const vtString& GetSourceName() const
	{
		return m_SourceName;
	}
	void SetRGB(const RGBi Color)
	{
		m_RGB = Color;
	}
	const RGBi GetRGB() const
	{
		return m_RGB;
	}
	// Operator  overloads
	bool operator == (const vtMaterialDescriptor& rhs) const
	{
		return(*this->m_pName == *rhs.m_pName);
	}
	bool operator == (const vtMaterialDescriptor& rhs)
	{
		return(*this->m_pName == *rhs.m_pName);
	}
	friend std::ostream &operator << (std::ostream & Output, const vtMaterialDescriptor &Input)
	{
		const RGBi &rgb = Input.m_RGB;
		Output << "\t<MaterialDescriptor Name=\""<< (pcchar)*Input.m_pName << "\""
			<< " Colorable=\"" << (Input.m_Colorable == VT_MATERIAL_COLOURABLE_TEXTURE) << "\""
			<< " Source=\"" << (pcchar)Input.m_SourceName << "\""
			<< " Scale=\"" << Input.m_fUVScale << "\""
			<< " RGB=\"" << rgb.r << " " << rgb.g << " " << rgb.b << "\""
			<< "/>" << std::endl;
		return Output;
	}
private:
	const vtString *m_pName; // Name of material
	int m_Type;		// 0 for surface materials, >0 for classification type
	vtMaterialColorEnum m_Colorable;
	vtString m_SourceName; // Source of material
	float m_fUVScale; // Texel scale;
	RGBi m_RGB; // Color for VT_MATERIAL_RGB

	// The following field is only used in 3d construction, but it's not
	//  enough distinction to warrant creating a subclass to contain it.
	int m_iMaterialIndex; // Starting or only index of this material in the shared materials array
};

class vtMaterialDescriptorArray : public Array<vtMaterialDescriptor*>
{
public:
	virtual ~vtMaterialDescriptorArray() { Empty(); free(m_Data); m_Data = NULL; m_MaxSize = 0; }
	void DestructItems(int first, int last)
	{
		for (int i = first; i <= last; i++)
			delete GetAt(i);
	}

	friend std::ostream &operator << (std::ostream & Output, vtMaterialDescriptorArray &Input)
	{
		int iSize = Input.GetSize();
		Output << "<?xml version=\"1.0\"?>" << std::endl;
		Output << "<MaterialDescriptorArray>" << std::endl;
		for (int i = 0; i < iSize; i++)
			Output << *Input.GetAt(i);
		Output << "</MaterialDescriptorArray>" << std::endl;
		return Output;
	}
	bool LoadExternalMaterials(const vtStringArray &paths);
	bool Load(const char *FileName);
	const vtString *FindName(const char *matname);
};

/**
 * Structure type.
 */
enum vtStructureType
{
	ST_BUILDING,	/**< A Building (vtBuilding) */
	ST_LINEAR,		/**< A Linear (vtFence) */
	ST_INSTANCE,	/**< A Structure Instance (vtStructInstance) */
	ST_NONE
};

/**
 * The vtStructure class represents any "built structure".  These are
 * generally immobile, artificial entities of human-scale and larger, such
 * as buildings and fences.
 * \par
 * Structures are implemented as 3 types:
 *  - Buildings (vtBuilding)
 *  - Fences and walls (vtFence)
 *  - Instances (vtStructInstance)
 * \par
 * For enclosed and linear structures which can be well-described
 * parametrically, vtBuilding and vtFence provide efficient data
 * representation.  For other structures which are not easily reduced to
 * parameters, the Instance type allows you to reference any external model,
 * such as a unique building which has been created in a 3D Modelling Tool.
 */
class vtStructure : public Selectable, public vtTagArray
{
public:
	vtStructure();
	virtual ~vtStructure();

	void SetType(vtStructureType t) { m_type = t; }
	vtStructureType GetType() { return m_type; }

	void SetElevationOffset(float fOffset) { m_fElevationOffset = fOffset; }
	float GetElevationOffset() const { return m_fElevationOffset; }
	void SetOriginalElevation(float fOffset) { m_fOriginalElevation = fOffset; }
	float GetOriginalElevation() const { return m_fOriginalElevation; }

	vtBuilding *GetBuilding() { if (m_type == ST_BUILDING) return (vtBuilding *)this; else return NULL; }
	vtFence *GetFence() { if (m_type == ST_LINEAR) return (vtFence *)this; else return NULL; }
	vtStructInstance *GetInstance() { if (m_type == ST_INSTANCE) return (vtStructInstance *)this; else return NULL; }

	virtual bool GetExtents(DRECT &rect) const = 0;
	virtual bool IsContainedBy(const DRECT &rect) const = 0;
	virtual void WriteXML(FILE *fp, bool bDegrees) = 0;
	virtual void WriteXML_Old(FILE *fp, bool bDegrees) = 0;

	void WriteTags(FILE *fp);

// VIAVTDATA
	bool m_bIsVIAContributor;
	bool m_bIsVIATarget;
// VIAVTDATA

protected:
	vtStructureType m_type;

	// Offset that the structure should be moved up or down relative to its
	// default position on the ground
	// for buildings this is (lowest corner of its base footprint)
	// for linear features this is the lowest point of the feature.
	// for instances this is the datum point
	float		m_fElevationOffset;

	// Original elevation information if any (meters)
	float		m_fOriginalElevation;

private:
	// Don't let unsuspecting users stumble into assuming that object
	// copy semantics will work.  Declare them private and never
	// define them,

	vtStructure( const vtStructure & );
	vtStructure &operator=( const vtStructure & );
};

/**
 * This class represents a reference to an external model, such as a unique
 * building which has been created in a 3D Modelling Tool.  It is derived from
 * vtTagArray which provides a set of arbitrary tags (name/value pairs).
 * At least one of the following two tags should be present:
 * - filename, which contains a resolvable path to an external 3d model file.
 *	 An example is filename="MyModels/GasStation.3ds"
 * - itemname, which contains the name of a content item which will be resolved
 *	 by a list maintained by a vtContentManager.  An example is
 *	 itemname="stopsign"
 */
class vtStructInstance : public vtStructure
{
public:
	vtStructInstance();

	void WriteXML(FILE *fp, bool bDegrees);
	void WriteXML_Old(FILE *fp, bool bDegrees);
	void Offset(const DPoint2 &delta);

	bool GetExtents(DRECT &rect) const;
	bool IsContainedBy(const DRECT &rect) const;

	DPoint2	m_p;			// earth position
	float	m_fRotation;	// in radians
	float	m_fScale;		// meters per unit
};

bool LoadGlobalMaterials(const vtStringArray &paths);
void SetGlobalMaterials(vtMaterialDescriptorArray *mats);
vtMaterialDescriptorArray *GetGlobalMaterials();
void FreeGlobalMaterials();

#endif // STRUCTUREH

