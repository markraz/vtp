#ifndef PTERRAINH
#define PTERRAINH

#include "vtlib/core/Terrain.h"
#include "Vehicles.h"
//#include "CarEngine.h"

class PTerrain : public vtTerrain
{
public:
	PTerrain();

	void MakePortal(vtTerrain* pTargetTerrain, vtTransform* gateway,
		char* name, int destination_index);
	void MakePortalUTM(vtTerrain* pTargetTerrain, vtTransform* gateway,
		char* name, float utm_x, float utm_y, int destination_index);

	// vehicle stuff
	void create_ground_vehicles(float fSize, float fSpeed); // dunno what to do to speed yet

	void SetupVehicles();
	void AddVehicleType(VehicleType *vt);
	Vehicle *CreateVehicle(const char *szType, const RGBf &cColor, float fSize);
#if 0
	void AddCarEngine(vtTransform *pVehicleModel,
					  float fSpeed, Node *pNode = NULL);
	void AddCarEngine(vtTransform *pVehicleModel,
					  float fSpeed, const char *filename);
#endif
	VehicleType *m_pFirstVehicleType;
};

#endif

