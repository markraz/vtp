//
// Plants.h
//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef PLANTSH
#define PLANTSH

#include "Projections.h"
#include "vtString.h"
#include "Array.h"
#include "MathTypes.h"

class vtPlantDensity
{
public:
	vtString	m_common_name;
	float		m_plant_per_m2;

	int			m_list_index;		// for faster lookup

	float		m_amount;			// these two fields are using during the
	int			m_iNumPlanted;		// plant distribution process
};

class vtBioType
{
public:
	vtBioType();
	~vtBioType();

	void AddPlant(int i, const char *common_name, float plant_per_m2);

	Array<vtPlantDensity *> m_Densities;

	vtString	m_name;
};

class vtBioRegion
{
public:
	vtBioRegion();
	~vtBioRegion();

	bool Read(const char *fname);
	bool Write(const char *fname);
	void AddType(vtBioType *bt) { m_Types.Append(bt); }
	int FindBiotypeIdByName(const char *name);

	Array<vtBioType *> m_Types;
};

enum AppearType {
	AT_XFROG,
	AT_BILLBOARD,
	AT_MODEL
};

class vtPlantAppearance
{
public:
	vtPlantAppearance();
	vtPlantAppearance(AppearType type, const char *filename, float width,
		float height, float shadow_radius, float shadow_darkness);
	virtual ~vtPlantAppearance();

	AppearType	m_eType;
	vtString	m_filename;
	float		m_width;
	float		m_height;
	float		m_shadow_radius;
	float		m_shadow_darkness;
	static float s_fTreeScale;
};

class vtPlantSpecies {
public:
	vtPlantSpecies();
	virtual ~vtPlantSpecies();

	// copy
	vtPlantSpecies &operator=(const vtPlantSpecies &v);

	void SetSpecieID(short SpecieID) { m_iSpecieID = SpecieID; }
	short GetSpecieID() const { return m_iSpecieID; }

	void SetCommonName(const char *CommonName);
	const char *GetCommonName() const { return m_szCommonName; }

	void SetSciName(const char *SciName);
	const char *GetSciName() const { return m_szSciName; }

	void SetMaxHeight(float f) { m_fMaxHeight = f; }
	float GetMaxHeight() const { return m_fMaxHeight; }

	virtual void AddAppearance(AppearType type, const char *filename,
		float width, float height, float shadow_radius, float shadow_darkness)
	{
		vtPlantAppearance *pApp = new vtPlantAppearance(type, filename,
			width, height, shadow_radius, shadow_darkness);
		m_Apps.Append(pApp);
	}

	int NumAppearances() const { return m_Apps.GetSize(); }
	vtPlantAppearance *GetAppearance(int i) const { return m_Apps[i]; }

protected:
	short		m_iSpecieID;
	vtString	m_szCommonName;
	vtString	m_szSciName;
	float		m_fMaxHeight;
	Array<vtPlantAppearance*> m_Apps;
};


class vtPlantList
{
public:
	vtPlantList();
	virtual ~vtPlantList();

	bool Read(const char *fname);
	bool Write(const char *fname);

	bool ReadXML(const char *fname);
	bool WriteXML(const char *fname);

	void LookupPlantIndices(vtBioType *pvtBioType);
	int NumSpecies() const { return m_Species.GetSize();  }
	vtPlantSpecies *GetSpecies(int i) const
	{
		if (i >= 0 && i < m_Species.GetSize())
			return m_Species[i];
		else
			return NULL;
	}
	int GetSpeciesIdByCommonName(const char *name);
	virtual void AddSpecies(int SpecieID, const char *common_name,
		const char *SciName, float max_height);
	void Append(vtPlantSpecies *pSpecies)
	{
		m_Species.Append(pSpecies);
	}

protected:
	Array<vtPlantSpecies*> m_Species;
};


struct vtPlantInstance {
	DPoint2 m_p;
	float size;
	short species_id;
};

class vtPlantInstanceArray : public Array<vtPlantInstance>
{
public:
	void AddInstance(DPoint2 &pos, float size, short species_id);

	bool ReadVF(const char *fname);
	bool WriteVF(const char *fname);
	bool FindClosestPlant(const DPoint2 &pos, double error_meters,
		int &plant, double &distance);

	vtProjection m_proj;
};

#endif
