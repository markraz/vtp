//
// RoadMap.h
//
// Copyright (c) 2001-2006 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef ROADMAPH
#define ROADMAPH

#include "DLG.h"

#define RMFVERSION_STRING "RMFFile2.0"
#define RMFVERSION_CURRENT 2.0
#define RMFVERSION_SUPPORTED 1.7	// oldest supported version

enum SurfaceType {
	SURFT_NONE,		// 0
	SURFT_GRAVEL,	// 1
	SURFT_TRAIL,	// 2
	SURFT_2TRACK,	// 3
	SURFT_DIRT,		// 4
	SURFT_PAVED,	// 5
	SURFT_RAILROAD,	// 6
	SURFT_STONE		// 7
};

//
// This enum describes how a single road meets a node.
//
enum IntersectionType {
	IT_NONE,	// uncontrolled
	IT_LIGHT,	// a traffic light
	IT_STOPSIGN,  // a stop sign
};

enum LightStatus {
	LT_INVALID,
	LT_RED,
	LT_YELLOW,
	LT_GREEN
};

// road flags
#define RF_SIDEWALK	0x0800
#define RF_PARKING	0x0400
#define RF_MARGIN	0x0200
#define RF_FORWARD	0x0080	// true if traffic flows from node 0 to 1
#define RF_REVERSE	0x0040	// true if traffic flows from node 1 to 0
// the following are for temporary, runtime use
#define RF_HIT		0x0001

// Typical, default values for widths, in meters
#define SIDEWALK_WIDTH		1.5f
#define CURB_HEIGHT			0.15f
#define MARGIN_WIDTH		1.6f
#define LANE_WIDTH			3.3f
#define PARKING_WIDTH		LANE_WIDTH

// Nodes and Links refer to each other, so use forward declarations
class TNode;
class TLink;

/**
 * A Transporation Node is a place where 2 or more roads (links) meet.
 */
class TNode
{
public:
	TNode();
	virtual ~TNode();

	// comparison
	bool operator==(TNode &ref);

	//copies internal variables from given node.
	void Copy(TNode* node);

	TLink *GetLink(int n);
	int FindLink(int roadID);			//returns internal number of road with given ID.  -1 if not found.
	void AddLink(TLink *pR);		//adds a road to the node
	void DetachLink(TLink *pR);	//detaches the road from the node.
	void DetermineLinkAngles();			//resulting angles > 0
	void SortLinksByAngle();			//sorts the internal roads by angle.
	DPoint2 find_adjacent_roadpoint2d(TLink *pR);  //returns the 2nd point on the road from the node.

	//sets intersection type for node.  returns false if road not found
	bool SetIntersectType(TLink *road, IntersectionType type);
	bool SetIntersectType(int roadNum, IntersectionType type);  //roadNum is internal index within the node
	IntersectionType GetIntersectType(TLink *road);	//returns the intersection type of given road
	IntersectionType GetIntersectType(int roadNum); //returns the intersection type of given road index (not ID)
	LightStatus GetLightStatus(TLink *road);			//returns the light status of given road
	LightStatus GetLightStatus(int roadNum);		//returns the light status of given road index (not ID)
	bool SetLightStatus(TLink *road, LightStatus light); //sets the light status of given road
	bool SetLightStatus(int roadNum, LightStatus light); //sets the light status of given road index (not ID)

	bool HasLights();
	bool IsControlled();	// true if any stopsigns or stoplights

	//adjust the light relationship of the roads at the node (if the intersection is has a signal light.)
	void AdjustForLights();

	DPoint2 m_p;	// utm coordinates of center
	int m_iLinks;	// number of roads meeting here
	int m_id;		// only used for reading from DLG/RMF

	// angle of each road, not initialized till SortLinksByAngle is called
	float *m_fLinkAngle;

	TNode *m_pNext;
protected:
	IntersectionType *m_IntersectTypes;	//intersection types of the roads at this node.
	LightStatus *m_Lights;  //lights of the roads at this node.
	TLink **m_r;  //array of roads that intersect this node.

private:
	// Don't let unsuspecting users stumble into assuming that object
	// copy semantics will work.  Declare them private and never
	// define them,
	TNode( const TNode & );
	TNode &operator=( const TNode & );
};


/**
 * A Transportation Link a series of points, connecting one node to another.
 */
class TLink : public DLine2
{
public:
	TLink();
	TLink(TLink &ref);
	virtual ~TLink();

	// comparison
	bool operator==(TLink &ref);

	void SetNode(int n, TNode *pNode) { m_pNode[n] = pNode; }
	TNode *GetNode(int n) { return m_pNode[n]; }

	// closest distance from point to the road
	double GetLinearCoordinates(const DPoint2 &p, double &a, double &b,
		DPoint2 &closest, int &roadpoint, float &fractional, bool bAllowEnds = true);
	double DistanceToPoint(const DPoint2 &point, bool bAllowEnds = true);

	// is the road a loop?
	bool IsLoop() { return (m_pNode[0] == m_pNode[1]); }

	// accessors for flag properties
	virtual void SetFlag(int flag, bool value);
	int GetFlag(int flag);

	// Return length of road centerline.
	float Length();
	float EstimateWidth(bool bIncludeSidewalk = true);

	float	m_fWidth;		// road width in meters
	unsigned short m_iLanes; // number of lanes
	SurfaceType m_Surface;
	short	m_iHwy;			// highway number: -1 for normal roads
	TLink	*m_pNext;		// the next Link, if roads are maintained in link list form
	short	m_iFlags;		// a flag to be used to holding any addition info.
	int		m_id;			// only used during file reading
	float	m_fSidewalkWidth;
	float	m_fCurbHeight;
	float	m_fMarginWidth;
	float	m_fLaneWidth;
	float	m_fParkingWidth;

protected:
	TNode	*m_pNode[2];	// "from" and "to" nodes
	float	m_fHeight[2];
};

typedef TLink *LinkPtr;
typedef TNode *TNodePtr;

#define shortSize 4
#define intSize 4
#define floatSize 4
#define doubleSize 8

/**
 * vtRoadMap contains a sets of nodes (TNode) and links (TLink) which define
 * a transportation network.
 *
 * It can be loaded and saved from RMF files, an ad-hoc Road Map Format which
 * is overdue to be replaced by some clean, extensible standard for
 * transportation networks.  Unforunately, such a standard does not yet
 * exist.
 */
class vtRoadMap
{
public:
	vtRoadMap();
	virtual ~vtRoadMap();

	void DeleteElements();
	DRECT GetMapExtent();		// get the geographical extent of the road map
	void ComputeExtents();

	int		NumLinks() const;	// returns number of roads in the road map
	int		NumNodes() const;	// returns number of nodes in the road map

	TLink	*GetFirstLink() { return m_pFirstLink; }
	TNode	*GetFirstNode() { return m_pFirstNode; }

	virtual TNode *NewNode() { return new TNode; }
	virtual TLink *NewLink() { return new TLink; }

	void	AddNode(TNode *pNode)
	{
		pNode->m_pNext = m_pFirstNode;
		m_pFirstNode = pNode;
	}
	void	AddLink(TLink *pLink)
	{
		pLink->m_pNext = m_pFirstLink;
		m_pFirstLink = pLink;
	}

	TNode *FindNodeByID(int id);
	TNode *FindNodeAtPoint(const DPoint2 &point, double epsilon);

	// cleaning function: remove unused nodes, return the number removed
	int RemoveUnusedNodes();

	// Other clean up functions
	void RemoveNode(TNode *pNode);
	void RemoveLink(TLink *pLink);

	bool ReadRMF(const char *filename, bool bHwy, bool bPaved, bool bDirt);
	bool WriteRMF(const char *filename);

	vtProjection &GetProjection() { return m_proj; }

protected:
	DRECT	m_extents;			// the extent of the roads in the RoadMap
	bool	m_bValidExtents;	// true when extents are computed

	TLink	*m_pFirstLink;
	TNode	*m_pFirstNode;

	vtProjection	m_proj;
};

#endif	// ROADMAPH
