
#include "vtlib/vtlib.h"
#include "CarEngine.h"

#include <iostream>
#include <fstream>


//accelertion in meters per second^2 (for the car.)
#define ACCELERATION 25

#define ACCEL ACCELERATION*WORLD_SCALE*1000/60/60

//defined in ismapp.cc
extern void ism_printf(char *format, ...);

//helpers
//returns difference of 2 angles in radians.  value will be between PI and -PI.
float angleDifference(float a, float b) {
	float val = a - b;
	while (val > PIf) {
		val -= PI2f;
	}
	while (val < -PIf) {
		val += PI2f;
	}
	return val;
}

//adjust angle (in radians) so that return value will be between PI and -PI.
float angleNormal(float val) {
	while (val > PIf) {
		val -= PI2f;
	}
	while (val < -PIf) {
		val += PI2f;
	}
	return val;
}

//setup engine to drive in a straight line.
//target speed in kilometers per hour
CarEngine::CarEngine(FPoint3 pos, vtHeightField *grid, float target_speed, float wRadius) 
{
	SharedConstructor(pos, grid, target_speed, wRadius);
	m_pCurNode = NULL;
	m_eMode = STRAIGHT;
	m_fSpeed = m_fTargetSpeed;
}

//setup engine to drive in circles
//target speed in kilometers per hour
CarEngine::CarEngine(FPoint3 pos, vtHeightField *grid, float target_speed,
					 float wRadius, FPoint3 center) 
{
	SharedConstructor(pos, grid, target_speed, wRadius);
	m_vCenterPos = center;
	m_fCircleRadius = sqrtf(pow(m_vCurPos.x - m_vCenterPos.x,2) + pow(m_vCurPos.z - m_vCenterPos.z,2));
	m_pCurNode = NULL;
	m_pCurRoad = NULL;
	m_eMode = CIRCLE;
	m_fSpeed = m_fTargetSpeed;
}

//setup engine to drive on roads, starting from node n.
//ignores pos.  takes position from given node.
CarEngine::CarEngine(FPoint3 pos, vtHeightField *grid, float target_speed,
					 float wRadius, Node *n, int lane, float roadheight)
{
	SharedConstructor(pos, grid, target_speed, wRadius);
	m_pCurNode = n;
	m_fRoadHeight = roadheight*WORLD_SCALE;
	m_vCurPos.x = ((NodeGeom*)n)->m_p3.x;
	m_vCurPos.y = ((NodeGeom*)n)->m_p3.y + m_fRoadHeight;
	m_vCurPos.z = ((NodeGeom*)n)->m_p3.z;

	m_iLane = lane-1;
	PickFirstRoad ();
	m_eMode = ROAD;
}

//shared constructor
void CarEngine::SharedConstructor(FPoint3 pos, vtHeightField*  grid, float target_speed, float wRadius){
	m_vCurPos = pos;

	m_pHeightField = grid;

	//convert speed to units/second (from km/h)
	m_fTargetSpeed = (target_speed * WORLD_SCALE) *1000/60/60;
	m_fSpeed = 0;

	m_fWheelSteerRotation = 0;
	m_fCurRotation = PID2f;
	m_fCurPitch = 0;
	m_vAxis = XAXIS;
	m_fWheelRadius = wRadius * WORLD_SCALE;

	m_bFirstTime = true;

	m_fPrevTime = 0.0f;		// we will use engine time (t) from now on

	m_fCircleRadius = 0;
	m_eMode = NONE;

	m_bForwards = true;

	m_iLane = 0;
	m_fRoadHeight = 0;
	m_fStopTime = 0.0f;
	m_bStopped = false;

	m_iRCoord = 0;
	m_fRDistance = 0;
	m_pCurNode = NULL;
	m_pNextNode = NULL;
	m_pCurRoad = NULL;
	m_pNextRoad = NULL;
}

//gets the path from given file.
void CarEngine::GetPath(const char* filename, vtRoadMap3d* roadmap) {
	ifstream file(filename, ios::in);
	if (!file) {
		//error
		return;
	}
	//read in path into array.
	file >> m_iNumRoads;
	file >> m_iStartNode;
	m_iRoads = new int[m_iNumRoads];
	for (int i = 0; i < m_iNumRoads; i++) {
		file >> m_iRoads[i];
	}
	file >> m_iEndNode;
	m_iNextRoad = 0;
	//find the start node.
	m_pCurNode = roadmap->FindNodeByID(m_iStartNode);
	m_eMode = PATH;
	m_bPathReverse = false;
	PickFirstRoad();
}

//evaluate the car engine.
bool CarEngine::Eval(float t)
{
	FPoint3 vNext;
	float fDeltaTime = t - m_fPrevTime;

	vtTransform *pTarget = (vtTransform *)m_pTarget;
	if (!pTarget) return false;

	//find vNext location
	switch (m_eMode) {
		case NONE:	//go nowhere.
			vNext = m_vCurPos;
			MoveCar(vNext);
			break;
		case STRAIGHT:
			//go straight.  try to match speed.
			//time is in seconds
			vNext.x = m_vCurPos.x + fDeltaTime*m_fSpeed*cosf(m_fCurRotation);
			vNext.z = m_vCurPos.z + fDeltaTime*m_fSpeed*sinf(m_fCurRotation);
			MoveCar(vNext);
			break;
		case CIRCLE:
			//drive around in a circle, try to match speed.
			Circle(vNext,t);
			MoveCar(vNext);
			break;
		case PATH:
		case ROAD:
			if (!m_pCurRoad) {
				return false;  //safety check
			}
			if (!m_bFirstTime)
			{
				//iterate simulation over 10ms periods.
				for (; m_fPrevTime < t; m_fPrevTime +=.01f) {
					//find where the road is
					bool newroad = false;
					FPoint3 target = GetNextTarget(t);
					//adjust vehicle speed.
					AdjustSpeed(.01f);

					//if we are not stopped, then move
					if (!m_bStopped)
					{
						//undo orientation.
						UndoOrientation();

						//turn appropriately - but there is a limit on how much the car can turn.
						TurnToward(target, .01f);
						//move forward based on speed.
						vNext = m_vCurPos;
						vNext.x += .01f*m_fSpeed * cosf (m_fCurRotation);
						vNext.z += .01f*m_fSpeed * sinf (m_fCurRotation);

						//FPoint3 oldPos = m_vCurPos;
						pTarget->SetTrans(vNext);
						m_vCurPos = vNext;
						//correct orientation.
						vNext.y = SetOrientation() + m_fRoadHeight;
						//apply translation
						pTarget->SetTrans(vNext);
						//ism_printf("target: %f, %f, %f\tRotation: %f", target.x, target.y, target.z, m_fCurRotation);
						//ism_printf("curpos: %f, %f, %f\tnext: %f, %f, %f",oldPos.x,oldPos.y,oldPos.z,vNext.x, vNext.y, vNext.z);

						m_vCurPos = vNext;
					}
				}
				m_fPrevTime -=.01f;
				//ism_printf("curpos: %f, %f, %f",m_vCurPos.x, m_vCurPos.y, m_vCurPos.z);
				break;
			} else {
				vNext.x = ((RoadGeom*)m_pCurRoad)->m_p3[m_iRCoord].x;
				vNext.y = ((RoadGeom*)m_pCurRoad)->m_p3[m_iRCoord].y + m_fRoadHeight;
				vNext.z = ((RoadGeom*)m_pCurRoad)->m_p3[m_iRCoord].z;
				pTarget->SetTrans(vNext);
				m_vCurPos = vNext;
				m_bFirstTime = false;
				break;
			}
	}
	// spin the wheels, adjusted for speed.
	SpinWheels(fDeltaTime*m_fSpeed/m_fWheelRadius);  
	m_fPrevTime = t;
	return true;
}

void CarEngine::Circle(FPoint3 &vNext, gfloat t) 
{
	//calculate position
	gfloat time = t * m_fSpeed / m_fCircleRadius;
	vNext.x = m_vCenterPos.x + m_fCircleRadius * cosf(time);
	vNext.z = m_vCenterPos.z + m_fCircleRadius * sinf(time);
}

void CarEngine::UndoOrientation()
{
	vtTransform* car = (vtTransform*)m_pTarget;
	//rotate orientation back
	car->Rotate2(m_vAxis, -m_fCurPitch);
	m_fCurPitch = 0.0f;
	m_vAxis = YAXIS;
}

//sets the orientation of the car, based on tire positions
//returns height of the car.
float CarEngine::SetOrientation()
{
	vtTransform* car = (vtTransform*)m_pTarget;
	FPoint3 fL, fR, rL, rR, rM, fM, vNormal;
	FMatrix4 transform;

	UndoOrientation();

	car->TotalTransform(&transform);
	// find tire locations in current orientation
	m_pFrontLeft->TotalTransform(fL);
	m_pFrontRight->TotalTransform(fR);
	m_pRearLeft->TotalTransform(rL);
	m_pRearRight->TotalTransform(rR);
//	transform.TransformVector(m_vFL_pos, fL);
//	transform.TransformVector(m_vFR_pos, fR);
//	transform.TransformVector(m_vRL_pos, rL);
//	transform.TransformVector(m_vRR_pos, rR);
	// current tire positions.
	fL+=m_vCurPos;
	fR+=m_vCurPos;
	rL+=m_vCurPos;
	rR+=m_vCurPos;

	//float a;
	float b;

	//vNormal is used as a dummy var here
	m_pHeightField->FindAltitudeAtPoint(fL, fL.y);
	m_pHeightField->FindAltitudeAtPoint(fR, fR.y);
	m_pHeightField->FindAltitudeAtPoint(rL, rL.y);
	m_pHeightField->FindAltitudeAtPoint(rR, rR.y);
	//===============================
	//use midpoint of 2 rear tires.
	//rM.x = (rL.x + rR.x)/2;
	// do not randomly reorder the lines below!!!!
	// there  might be either a Visual C++ 6 compiler mistake or a hardware scheduling mistake if
	// rM.z is written to RIGHT BEFORE we read from it.

	/*
	//incorrect results
	rM.x = (rL.x + rR.x)/2;
	rM.y = (rL.y + rR.y)/2;
	rM.z = (rL.z + rR.z)/2;

	vNormal.x = fL.y*(rM.z-fR.z) + rM.y*(fR.z-fL.z) + fR.y*(fL.z-rM.z);
	vNormal.y = fL.z*(rM.x-fR.x) + rM.z*(fR.x-fL.x) + fR.z*(fL.x-rM.x);
	vNormal.z = fL.x*(rM.y-fR.y) + rM.x*(fR.y-fL.y) + fR.x*(fL.y-rM.y);

	a = vNormal.x;
	*/
	//correct results
	rM.y = (rL.y + rR.y)/2;
	rM.z = (rL.z + rR.z)/2;
	rM.x = (rL.x + rR.x)/2;

	vNormal.x = fL.y*(rM.z-fR.z) + rM.y*(fR.z-fL.z) + fR.y*(fL.z-rM.z);
	vNormal.y = fL.z*(rM.x-fR.x) + rM.z*(fR.x-fL.x) + fR.z*(fL.x-rM.x);
	vNormal.z = fL.x*(rM.y-fR.y) + rM.x*(fR.y-fL.y) + fR.x*(fL.y-rM.y);

	b = vNormal.x;

	//===============================

	//new orientation
	m_vAxis = vNormal.Cross(YAXIS);
	//dot product:  a.b = |a||b|cos(THETA)
	m_fCurPitch = -acosf(vNormal.Dot(YAXIS)/(vNormal.Length()*YAXIS.Length()));

	car->Rotate2(m_vAxis, m_fCurPitch);
	//find midpoint of all wheels.
	fM.x = (fL.x + fR.x)/2;
	fM.y = (fL.y + fR.y)/2;
	fM.z = (fL.z + fR.z)/2;
	/*
	if (a!=b) {
		//floating point bug???
		//TRACE("computation are different: %.8f, %.8f\n", a,b);
	}
	*/
	return (fM.y+rM.y)/2;
	//return m_vCurPos.y + (6.0f * WORLD_SCALE);

}

// assume that the car is ALWAYS going forward.
//sets orientation of car.  next_pos is modified to be new location.
void CarEngine::SetOrientationAndHeight(FPoint3 &next_pos) 
{
	vtTransform* car = (vtTransform*)m_pTarget;
	FPoint3 tempVec;
	float newangle,angle,deltax,deltaz;

	UndoOrientation();

	deltax = next_pos.x - m_vCurPos.x;
	deltaz = next_pos.z - m_vCurPos.z;
	newangle = atan2f(deltaz,deltax);

	//turn in right direction (about YAXIS only!)
	angle = m_fCurRotation - newangle;
	car->Rotate2(YAXIS, angle);
	m_fCurRotation = newangle;

	//turn the front wheels
	newangle = 2*angle - m_fWheelSteerRotation;
	m_fWheelSteerRotation += newangle;
	//TRACE("newangle: %f\t wheel: %f\t m_CurRot: %f \n",newangle, m_fWheelSteerRotation, m_fCurRotation);
	FPoint3 trans;
	trans = m_pFrontLeft->GetTrans();
	m_pFrontLeft->Translate2(-trans);
	m_pFrontLeft->Rotate2(YAXIS, newangle);
	m_pFrontLeft->Translate2(trans);
	trans = m_pFrontRight->GetTrans();
	m_pFrontRight->Translate2(-trans);
	m_pFrontRight->Rotate2(YAXIS, newangle);
	m_pFrontRight->Translate2(trans);

	//modify the orientation of the car to match the terrain
	//points of the tires
	if (m_bFirstTime) {
		m_pHeightField->FindAltitudeAtPoint(next_pos, next_pos.y);
		m_bFirstTime = false;
	} else {
		m_vCurPos = next_pos;
		next_pos.y = SetOrientation();
	}

}


//a function to see if the end of the first string matches all of the second;
bool strend (const char* first, const char* second)
{
	int i = 0;
	const char* temp;
	temp = first;
	if (!temp) {
		return false;
	}
	//find length of first string
	while (*temp) {
		i++;
		temp++;
	}
	int firstlen = i;

	i = 0;
	temp = second;
	//length of second string
	while (*temp) {
		i++;
		temp++;
	}
	int secondlen = i;

	//if second longer than first, no match
	if (firstlen < secondlen) {
		return false;
	}

	//search string backwards.
	while (secondlen >= 0) {
		if (first[firstlen] != second[secondlen]) {
			return false;
		}
		firstlen--;
		secondlen--;
	}
	return true;
}

/*	finds and sets the tire variables in the model.  assumes that the tires are under a group name ending
	in "tires" and the 4 tires are the children of the group.  the 4 tire names should end with their 
	location names: "front left" "front right" "rear left" "rear right"

  RUN BEFORE EVAL!!!!
	*/
bool CarEngine::SetTires() 
{
	vtGroup *tires = FindTires((vtTransform *) GetTarget());
	if (tires != NULL) {
		vtTransform *tModel;
		const char* tName;
		int numChild = tires->GetNumChildren();
		int i = 0;
		while (i < numChild) {
			tModel = (vtTransform *) tires->GetChild(i);
			tName = tModel->GetName2();
			if (strend (tName, "front left")) {
				m_pFrontLeft = tModel;
			} else if (strend  (tName, "front right")) {
				m_pFrontRight = tModel;
			} else if (strend (tName, "rear left")) {
				m_pRearLeft = tModel;
			} else if (strend  (tName, "rear right")) {
				m_pRearRight = tModel;
			} else {
				TRACE("INVALID TIRE IN MODEL: %s!!!!!\n",((vtTransform *) GetTarget())->GetName2());
			}
			i++;
		}
		return true;
	} else {
		TRACE("TIRES NOT FOUND IN MODEL: %s!!!!!\n", ((vtTransform *) GetTarget())->GetName2());
		return false;
	}

}

/*	find the tire group	done rem_Cursively - might want to change it to an interative version 
	for performance boost with more complex models.
*/
vtGroup* CarEngine::FindTires(vtGroup *model) 
{
#if 0
	vtGroup *tModel;
	if (strend (model->GetName2(), "tires")) {
		return model;
	}

	int numChild = model->GetNumChildren();

	if (numChild < 1) {
		return NULL;
	}

	int i = 0;
	while (i < numChild) {
		if (tModel = FindTires((vtGroup *) model->GetChild(i))) {
			return tModel;
		} else { 
			i++;
		}
	}
#endif
	return NULL;
}

//pick the first road to drive on.
void CarEngine::PickFirstRoad()
{
	int i = 0;

	// safety check
	if (!m_pCurNode)
	{
		m_pCurRoad = NULL;
		m_pNextNode = NULL;
		m_pNextRoad = NULL;
		return;
	}
	if (m_eMode == PATH) {
		//pick road based on path.
		int roadID = m_iRoads[m_iNextRoad];
		for (i = 0; i < m_pCurNode->m_iRoads; i++) {
			Road *r = m_pCurNode->GetRoad(i);
			if (r->m_id == roadID) {
				//found road.
				//determine next road to follow.
				if (!m_bPathReverse) {
					if (m_iNextRoad < m_iNumRoads - 1)
						m_iNextRoad++;
					else
						m_bPathReverse = true;
				} else {
					if (m_iNextRoad > 1)
						m_iNextRoad--;
					else 
						m_bPathReverse = false;
				}
				break;
			}
		}
	} else {
		//road following.
		//make sure car can go in the direction of the road.
		for (i = 0; i < m_pCurNode->m_iRoads; i++) {
			Road *r = m_pCurNode->GetRoad(i);
			if ((r->m_iFlags & RF_FORWARD && r->GetNode(0) == m_pCurNode)
				||
				(r->m_iFlags & RF_REVERSE && r->GetNode(1) == m_pCurNode)) {
				break;
			}
		}
	}

	//the first road to follow.
	m_pCurRoad = m_pCurNode->GetRoad(i);
	m_bForwards = (m_pCurRoad->GetNode(0) == m_pCurNode);

	//determine what's the next intersect type (so we know whether to slow down or not.)
	if (m_bForwards) {
		m_pNextNode = m_pCurRoad->GetNode(1);
		m_iNextIntersect = m_pNextNode->GetIntersectType(m_pCurRoad);
		//reset coord index
		m_iRCoord = 0;
		//m_iLane = 0;
	} else {
		m_pNextNode = m_pCurRoad->GetNode(0);
		m_iNextIntersect = m_pNextNode->GetIntersectType(m_pCurRoad);
		//reset coord index
		m_iRCoord = m_pCurRoad->GetSize()-1;
		//m_iLane = m_pCurRoad->m_iLanes - 1;
	}

	//what's the next road?
	PickNextRoad();
}

//gets the next road (which is actually already determined.)
//we have to look one road ahead so that we know if we are going to make a turn later.
void CarEngine::PickRoad()
{
	int i = 0;

	// safety check
	if (!m_pCurNode)
	{
		m_pCurRoad = NULL;
		m_pNextNode = NULL;
		m_pNextRoad = NULL;
		return;
	}

	//ism_printf("New Road: %i", m_pNextRoad->m_id);
	//figure out which lane to follow.
	/*
	m_iLane = m_pNextNode->m_pSwitch->LaneChoice(m_pCurRoad, m_iLane, m_pNextRoad);
	if (m_iLane < 0) {
		m_iLane = 0;
	}
	*/
	m_pCurRoad = m_pNextRoad;
	m_pCurNode = m_pNextNode;

	//the first road to follow.
	m_bForwards = (m_pCurRoad->GetNode(0) == m_pCurNode);

	//determine what's the next intersect type (so we know whether to slow down or not.)
	if (m_bForwards) {
		m_pNextNode = m_pCurRoad->GetNode(1);
		m_iNextIntersect = m_pNextNode->GetIntersectType(m_pCurRoad);
		//reset coord index
		m_iRCoord = 0;
	} else {
		m_pNextNode = m_pCurRoad->GetNode(0);
		m_iNextIntersect = m_pNextNode->GetIntersectType(m_pCurRoad);
		//reset coord index
		m_iRCoord = m_pCurRoad->GetSize()-1;
	}

	//get the next road to follow.
	PickNextRoad();
}

//picks the next road.
void CarEngine::PickNextRoad() {
	int i;
	if (m_eMode == PATH) {
		//pick road based on path.
		int roadID = m_iRoads[m_iNextRoad];
		for (i = 0; i < m_pNextNode->m_iRoads; i++) {
			Road *r = m_pNextNode->GetRoad(i);
			if (r->m_id == roadID) {
				//found road.
				//determine next road to follow.
				if (!m_bPathReverse) {
					if (m_iNextRoad < m_iNumRoads - 1)
						m_iNextRoad++;
					else
						m_bPathReverse = true;
				} else {
					if (m_iNextRoad > 1)
						m_iNextRoad--;
					else 
						m_bPathReverse = false;
				}
				break;
			}
		}
	} else {
		//select the next road to follow.
		if (m_pNextNode->m_iRoads != 1)
		{
			i = m_pNextNode->m_iRoads;
			// pick vNext available road
			if (m_pCurRoad->m_iHwy > 0)
			{
				// special logic: follow the highway
				for (i = 0; i < m_pNextNode->m_iRoads; i++)
				{
					Road *r = m_pNextNode->GetRoad(i);
					if ((r != m_pCurRoad && r->m_iHwy > 0)
						&&
						((r->m_iFlags & RF_FORWARD && r->GetNode(0) == m_pNextNode)
							||
						(r->m_iFlags & RF_REVERSE && r->GetNode(1) == m_pNextNode))) 
					{
						break;
					}
				}
				// if no highway, do normal logic
			}
			if (i == m_pNextNode->m_iRoads)
			{
				//find index for current road
				int r_index;
				for (i = 0; i < m_pNextNode->m_iRoads; i++)
				{
					if (m_pNextNode->GetRoad(i) == m_pCurRoad)
					{
						r_index = i;
						i++;
						//wrap around
						i %= m_pNextNode->m_iRoads;
						break;
					}
				}
				int temp_i = -1;
				//find next road available with traffic going out from the node.
				for (i = 1; i < m_pNextNode->m_iRoads; i++)
				{
					//wrap around
					temp_i = (r_index + i)% m_pNextNode->m_iRoads;
					Road *r = m_pNextNode->GetRoad(temp_i);
					if ((r->m_iFlags & RF_FORWARD && r->GetNode(0) == m_pNextNode)
						||
						(r->m_iFlags & RF_REVERSE && r->GetNode(1) == m_pNextNode))
					{
						i = temp_i;
						break;
					}
				}
			}
		}
	}
	if (i < m_pNextNode->m_iRoads) {
		m_pNextRoad = m_pNextNode->GetRoad(i);
	} else {
		m_pNextRoad = m_pCurRoad;
	}
	/*
	now that we know the next road, how sharp of an angle do we have to turn to 
	transition from the current road to the next road?
	we only look at the 2D case.
	*/

	FPoint3 center(((NodeGeom*)m_pNextNode)->m_p3.x, 
					0,
					((NodeGeom*)m_pNextNode)->m_p3.z);
	int index;
	if (m_bForwards) {
		index = m_pCurRoad->GetSize()-2;
	} else {
		index = 1;
	}
	FPoint3 curVec(((RoadGeom*)m_pCurRoad)->m_p3[index].x,
					0,
					((RoadGeom*)m_pCurRoad)->m_p3[index].z);

	if (m_pNextRoad->GetNode(0) == m_pNextNode) {
		//we're going forward
		index = 1;
	} else {
		//going from node 1 to 0
		index = m_pNextRoad->GetSize()-2;
	}
	FPoint3 nextVec(((RoadGeom*)m_pNextRoad)->m_p3[index].x,
					0,
					((RoadGeom*)m_pNextRoad)->m_p3[index].z);

	//find the angle between curVec and nextVec
	m_fAngle = fabsf(Angle(center, curVec, nextVec));
	//ism_printf("angle:%f", m_fAngle);
}

//a simple lane picking algorithm
int CarEngine::PickLane() {
	if (m_bForwards) {
		return m_iLane;
	} else {
		return m_pCurRoad->m_iLanes - m_iLane -1;
	}
}

void CarEngine::MoveCar(FPoint3 vNext) {
	vtTransform *pTarget = (vtTransform *) GetTarget();

	//move the car so that the wheels will be set at new location
	pTarget->SetTrans(vNext);

	vNext = pTarget->GetTrans();

	//also now need to move tire coordinates.
	//Currently, steering of wheels is done in here too.
	//rotate body to head toward vNext location
	SetOrientationAndHeight(vNext);

	m_vCurPos = vNext;
	pTarget->SetTrans(vNext);

}

//spin the wheels base on how much we've driven
void CarEngine::SpinWheels(float dist) {
	m_pFrontLeft->RotateLocal(XAXIS, dist);
	m_pFrontRight->RotateLocal(XAXIS, dist);
	m_pRearLeft->RotateLocal(XAXIS, dist);
	m_pRearRight->RotateLocal(XAXIS, dist);
}

//get next point to drive toward
FPoint3 CarEngine::GetNextTarget(float fCurTime) {
	int lane = PickLane();
//	ism_printf("%i of %i",m_iRCoord, m_pCurRoad->GetSize());
	FPoint3 nextPoint = ((RoadGeom*)m_pCurRoad)->m_pLanes[lane].m_p3[m_iRCoord];

	//if we're stopped, figure out if we can move!
	if (m_bStopped) {
		if (m_iNextIntersect == IT_STOPSIGN) {
			//have we waited long enough?
			//ism_printf("stopped at %f.  it's now %f", m_fStopTime, fCurTime);
			if (fCurTime - m_fStopTime > 3.0f) //stop for three seconds
			{
				m_bStopped = false;
				PickRoad();
				nextPoint = ((RoadGeom*)m_pCurRoad)->m_pLanes[lane].m_p3[m_iRCoord];
			} else {
				FPoint3 r(nextPoint.x, nextPoint.y, nextPoint.z);
				return r;
			}
		}
		if (m_iNextIntersect == IT_LIGHT) {
			//go only if green
			if (m_pNextNode->GetLightStatus(m_pCurRoad) == LT_GREEN) {
				m_bStopped = false;
				PickRoad();
				nextPoint = ((RoadGeom*)m_pCurRoad)->m_pLanes[lane].m_p3[m_iRCoord];
			} else {
				FPoint3 r(nextPoint.x, nextPoint.y, nextPoint.z);
				return r;
			}
		}
	}

	int endIndex;
	if (m_bForwards) {
		endIndex = m_pCurRoad->GetSize() -1;
	} else {
		endIndex = 0;
	}
//	FPoint3 endPoint = ((RoadGeom*)m_pCurRoad)->m_pLanes[lane].m_p3[endIndex];
	FPoint3 curPos(m_vCurPos.x, m_vCurPos.y, m_vCurPos.z);
	FPoint3 delta = nextPoint - curPos;
//	FPoint3 delta2 = endPoint - curPos;

//	float dist2end = delta2.Length();
	float dist = delta.Length();

	//if we're getting close, look at the next point to reach.
	float threshold;
	threshold = 2.5f*WORLD_SCALE;

	//ism_printf("%f", m_fRDistance);
	if (dist < threshold || (dist > 1.125f*m_fRDistance  && m_fRDistance >=0)) {
		//assume that we've past target, get next target.
		bool newroad = false;
		if (m_bForwards) {
			m_iRCoord++;
			if (m_iRCoord == m_pCurRoad->GetSize()) {
				newroad = true;
			}
		} else {
			m_iRCoord--;
			if (m_iRCoord < 0) {
				newroad = true;
			}
		}
		if(newroad) {
			//out of coords, need to look at next road.
			if (m_iNextIntersect == IT_STOPSIGN || 
				(m_iNextIntersect == IT_LIGHT &&
				m_pNextNode->GetLightStatus(m_pCurRoad) == LT_RED)) {
				m_bStopped = true;
				m_fStopTime = fCurTime;
				//ism_printf("Stopped!");
				//do NOT pick a new road just yet.
			} else {
				PickRoad();
			}
		}
		lane = PickLane();
		nextPoint = ((RoadGeom*)m_pCurRoad)->m_pLanes[lane].m_p3[m_iRCoord];
		m_fRDistance = -1;
	} else {
		m_fRDistance = dist;
	}

	FPoint3 r(nextPoint.x, nextPoint.y, nextPoint.z);
	return r;
}

//turn toward coordinates, but there will be a limit on how much of a turn.
void CarEngine::TurnToward(FPoint3 target, float time) {
	vtTransform *car = (vtTransform *) GetTarget();
	float newangle,angle,deltax,deltaz;

	UndoOrientation();

	deltax = target.x - m_vCurPos.x;
	deltaz = target.z - m_vCurPos.z;
	newangle = atan2f(deltaz,deltax);

	//restrict how much the car can turn per sec.
	float speed = m_fSpeed;

	/* can't navigate some of the turns with this limit.
	//15000/60/60 = 4.167
	if (speed > 12.0f * WORLD_SCALE) {
		speed = 12.0f * WORLD_SCALE;
	}
	*/
	float angleRange = 10.0f*PIf*time*speed;

	float diff = angleDifference(newangle, m_fCurRotation);

	if (diff > angleRange) {
		newangle = m_fCurRotation + angleRange;
		m_fSpeed -= ACCEL * time * WORLD_SCALE;
	} else if (diff < -angleRange) {
		newangle = m_fCurRotation - angleRange;
		m_fSpeed -= ACCEL * time * WORLD_SCALE;
	}

	newangle = angleNormal(newangle);

	//turn in right direction (about YAXIS only!)
	angle = m_fCurRotation - newangle;

	car->Rotate2(YAXIS, angle);
	m_fCurRotation = newangle;

	//turn the front wheels
	newangle = 20*angle - m_fWheelSteerRotation;
	m_fWheelSteerRotation += newangle;
	//ism_printf("newangle: %f\t wheel: %f\t m_CurRot: %f \n",newangle, m_fWheelSteerRotation, m_fCurRotation);
	FPoint3 trans;
	trans = m_pFrontLeft->GetTrans();
	m_pFrontLeft->Translate2(-trans);
	m_pFrontLeft->Rotate2(YAXIS, newangle);
	m_pFrontLeft->Translate2(trans);
	trans = m_pFrontRight->GetTrans();
	m_pFrontRight->Translate2(-trans);
	m_pFrontRight->Rotate2(YAXIS, newangle);
	m_pFrontRight->Translate2(trans);
}

#define ANGLETOLERANCE PI2f/3

void CarEngine::AdjustSpeed(float fDeltaTime) {
	if (m_bStopped) {
		m_fSpeed = 0;
		return;
	}

	//calculate distance to go.
	FPoint3 endOfRoad;
	endOfRoad.x = ((NodeGeom*)m_pNextNode)->m_p3.x;
	endOfRoad.y = ((NodeGeom*)m_pNextNode)->m_p3.y;
	endOfRoad.z = ((NodeGeom*)m_pNextNode)->m_p3.z;

	FPoint3 deltaPoint = endOfRoad-m_vCurPos;
	deltaPoint.y = 0;
	float distToGo = deltaPoint.Length();
	switch (m_iNextIntersect) {
	case IT_NONE:
		//if the turn up ahead is too steep, slow down 
		if (m_fAngle < ANGLETOLERANCE && distToGo < (m_fSpeed * m_fSpeed) / (ACCEL)) {
				//getting to close.  slow down.
				m_fSpeed -= (m_fSpeed*m_fSpeed)/distToGo*fDeltaTime;
				//ism_printf("dead end");
		} else if (m_fSpeed < m_fTargetSpeed) {
			m_fSpeed += ACCEL*fDeltaTime;
			//ism_printf("vroom!");
		}
		break;
	case IT_LIGHT:
		//adjust speed based on light condition.
		switch (m_pNextNode->GetLightStatus(m_pCurRoad)) {
		case LT_RED:
			//stop if we're getting close
			//ism_printf("RED");
			if (distToGo < (m_fSpeed * m_fSpeed) / (ACCEL)) {
				//getting to close.  slow down.  (stop ASAP!)
				m_fSpeed -= (m_fSpeed*m_fSpeed)/distToGo*fDeltaTime;
				//ism_printf("Red: slowing down");
			} else if (m_fSpeed < m_fTargetSpeed) {
				m_fSpeed += ACCEL*fDeltaTime;
			}
			break;
		case LT_YELLOW:
			//ism_printf("YELLOW");
			//can we stop in time?  if not, plow on through
			if (distToGo > m_fSpeed * (LT_SWITCH_TIME2) &&
				distToGo < (.5f *m_fSpeed * m_fSpeed) / (ACCEL)
				) {
				//slow down.
				//ism_printf("Yellow: slowing down");
				m_fSpeed -= ACCEL*fDeltaTime;
			} else if (m_fSpeed < m_fTargetSpeed) {
				m_fSpeed += ACCEL*fDeltaTime;
			}
			break;
		case LT_GREEN:
			//ism_printf("GREEN");
			//if the turn up ahead is too much, slow down 
			if (m_fAngle < ANGLETOLERANCE && distToGo < (m_fSpeed * m_fSpeed) / (ACCEL)) {
				//getting to close.  slow down.
				m_fSpeed -= (m_fSpeed*m_fSpeed)/distToGo*fDeltaTime;
			}  else if (m_fSpeed < m_fTargetSpeed) {
				//speed up if necessary
				m_fSpeed += ACCEL*fDeltaTime;
				//ism_printf("Green: speeding up");
			}
			break;
		}
	case IT_STOPSIGN:
		//ism_printf("Stop Sign");
		if (distToGo < (.5 * m_fSpeed * m_fSpeed) / (ACCEL)) {
			//getting to close.  slow down.
			//ism_printf("Stop: slowing down");
			m_fSpeed -= ACCEL*fDeltaTime;
		} else if (m_fSpeed < m_fTargetSpeed) {
			//ism_printf("Stop: speeding up");
			m_fSpeed += ACCEL*fDeltaTime;
		}
		break;
	default:
		//uh oh!
		break;
	}
	//bounds on speed.
	if (m_fSpeed > m_fTargetSpeed) {
		m_fSpeed = m_fTargetSpeed;
	}
	//don't actually stop completely.
	if (m_fSpeed < 0) {
		m_fSpeed = .1f*WORLD_SCALE*1000/60/60;
	}
}

//calculates the angle between the 2 vectors, in x-z plane.
float CarEngine::Angle(FPoint3 center, FPoint3 curVec, FPoint3 nextVec) {
	curVec -= center;
	nextVec -= center;
	if (curVec == nextVec) {
		return 0;
	}
	//dot product:  a.b = |a||b|cos(THETA)
	return acosf(curVec.Dot(nextVec)/(curVec.Length()*nextVec.Length()));
}

