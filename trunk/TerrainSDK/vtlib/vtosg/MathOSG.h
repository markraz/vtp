//
// Copyright (c) 2001 Virtual Terrain Project
// Free for all uses, see license.txt for details.
//

#ifndef VTOSG_MATHH
#define VTOSG_MATHH

///////////////////////
// math helpers

inline void v2s(const FPoint2 &f, osg::Vec2 &s) { s[0] = f.x; s[1] = f.y; }
inline void v2s(const FPoint3 &f, osg::Vec3 &s) { s[0] = f.x; s[1] = f.y; s[2] = f.z; }
inline void v2s(const RGBf &f, osg::Vec4 &s) { s[0] = f.r; s[1] = f.g; s[2] = f.b; s[3] = 1.0f; }

inline osg::Vec3 v2s(const FPoint3 &f)
{
	osg::Vec3 s;
	s[0] = f.x; s[1] = f.y; s[2] = f.z;
	return s;
}

inline osg::Vec4 v2s2(const FPoint3 &f)
{
	osg::Vec4 s;
	s[0] = f.x; s[1] = f.y; s[2] = f.z; s[3] = 0.0f;
	return s;
}

inline osg::Vec4 v2s(const RGBf &f)
{
	osg::Vec4 s;
	s[0] = f.r; s[1] = f.g; s[2] = f.b; s[3] = 1.0f;
	return s;
}

inline void s2v(osg::Vec3 &s, FPoint3 &f) { f.x = s[0]; f.y = s[1]; f.z = s[2]; }
inline void s2v(osg::Vec2 &s, FPoint2 &f) { f.x = s[0]; f.y = s[1]; }
inline void s2v(osg::Vec4 &s, RGBf &f) { f.r = s[0]; f.g = s[1]; f.b = s[2]; }

inline void s2v(osg::BoundingSphere &bs, FSphere &sph)
{
	s2v(bs._center, sph.center);
	sph.radius = bs._radius;
}

inline void ConvertMatrix4(const osg::Matrix *mat_osg, FMatrix4 *mat)
{
	const float *ptr = mat_osg->ptr();
	int i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			mat->Set(j, i, ptr[(i<<2)+j]);
		}
}

inline void ConvertMatrix4(const FMatrix4 *mat, osg::Matrix *mat_osg)
{
	float *ptr = mat_osg->ptr();
	int i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			ptr[(i<<2)+j] = mat->Get(j, i);
		}
}

#endif
