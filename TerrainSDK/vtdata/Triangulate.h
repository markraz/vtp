
// ** THIS IS A CODE SNIPPET WHICH WILL EFFICIEINTLY TRIANGULATE ANY
// ** POLYGON/CONTOUR (without holes) AS A STATIC CLASS.  THE TRIANGULATE
// ** BASE CLASS ALSO PROVIDES TWO USEFUL HELPER METHODS, ONE WHICH
// ** COMPUTES THE AREA OF A POLYGON, AND ANOTHER WHICH DOES AN EFFICENT
// ** POINT IN A TRIANGLE TEST.
// ** SUBMITTED BY JOHN W. RATCLIFF (jratcliff@verant.com) July 22, 2000

#ifndef TRIANGULATE_H
#define TRIANGULATE_H

/****************************************************************/
/* Static class to triangulate any contour/polygon efficiently **/
/* Does not support polygons with holes.                       **/
/* This code snippet was submitted to FlipCode.com by          **/
/* John W. Ratcliff (jratcliff@verant.com) on July 22, 2000    **/
/* I did not write the original code/algorithm for this        **/
/* this triangulator, in fact, I can't even remember where I   **/
/* found it in the first place.  However, I did rework it into **/
/* the following black-box static class so you can make easy   **/
/* use of it in your own code.                                 **/
/****************************************************************/


#include "MathTypes.h"

class Triangulate_f
{
public:
	// triangulate a contour/polygon, places results in STL vector
	// as series of triangles.
	static bool Process(const FLine2 &contour, FLine2 &result);

	// compute area of a contour/polygon
	static float Area(const FLine2 &contour);

	// decide if point Px/Py is inside triangle defined by
	// (Ax,Ay) (Bx,By) (Cx,Cy)
	static bool InsideTriangle(float Ax, float Ay,
							   float Bx, float By,
							   float Cx, float Cy,
							   float Px, float Py);

private:
	static bool Snip(const FLine2 &contour,int u,int v,int w,int n,int *V);
};

class Triangulate_d
{
public:
	// triangulate a contour/polygon, places results in STL vector
	// as series of triangles.
	static bool Process(const DLine2 &contour, DLine2 &result);

	// compute area of a contour/polygon
	static double Area(const DLine2 &contour);

	// decide if point Px/Py is inside triangle defined by
	// (Ax,Ay) (Bx,By) (Cx,Cy)
	static bool InsideTriangle(double Ax, double Ay,
							   double Bx, double By,
							   double Cx, double Cy,
							   double Px, double Py);

private:
	static bool Snip(const DLine2 &contour,int u,int v,int w,int n,int *V);
};

#endif
