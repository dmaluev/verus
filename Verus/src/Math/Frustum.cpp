// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Math;

Frustum::Frustum()
{
	VERUS_ZERO_MEM(_planes);
	VERUS_ZERO_MEM(_corners);
}

Frustum::~Frustum()
{
}

Frustum Frustum::MakeFromMatrix(RcMatrix4 m)
{
	Frustum f;
	f._matI = VMath::inverse(m);
	const float z = 0;
	const Vector4 corners[10] =
	{
		Vector4(+1, +1, z, 1),
		Vector4(-1, +1, z, 1),
		Vector4(+1, -1, z, 1),
		Vector4(-1, -1, z, 1),
		Vector4(+1, +1, 1, 1),
		Vector4(-1, +1, 1, 1),
		Vector4(+1, -1, 1, 1),
		Vector4(-1, -1, 1, 1),
		Vector4(+0, +0, z, 1),
		Vector4(+0, +0, 1, 1)
	};
	VERUS_FOR(i, 10)
	{
		Vector4 inv = f._matI * corners[i];
		inv /= inv.getW();
		f._corners[i] = inv.getXYZ();
	};
	const Matrix4 mvp = VMath::transpose(m);
	f._planes[+Name::left]     /**/ = mvp.getCol3() + mvp.getCol0();
	f._planes[+Name::right]    /**/ = mvp.getCol3() - mvp.getCol0();
	f._planes[+Name::bottom]   /**/ = mvp.getCol3() + mvp.getCol1();
	f._planes[+Name::top]      /**/ = mvp.getCol3() - mvp.getCol1();
	f._planes[+Name::nearest]  /**/ = mvp.getCol3() + mvp.getCol2();
	f._planes[+Name::furthest] /**/ = mvp.getCol3() - mvp.getCol2();
	VERUS_FOR(i, 6)
		f._planes[i].Normalize();
	return f;
}

RFrustum Frustum::FromMatrix(RcMatrix4 m)
{
	*this = MakeFromMatrix(m);
	return *this;
}

Relation Frustum::ContainsSphere(RcSphere sphere) const
{
	const Point3 c = sphere.GetCenter();
	const float r = sphere.GetRadius();
	VERUS_FOR(i, 6)
	{
		const float dist = _planes[i].DistanceTo(c);
		if (dist <= -r)
			return Relation::outside;
		if (abs(dist) < r)
			return Relation::intersect;
	}
	return Relation::inside;
}

Relation Frustum::ContainsAabb(RcBounds aabb) const
{
	int totalIn = 0, inCount, allPointsIn;
	Vector4 res0, res1;
	Matrix4 m[2];
	aabb.GetCorners(m);
	VERUS_FOR(plane, 6)
	{
		inCount = 8;

		res0 = m[0] * _planes[plane];
		res1 = m[1] * _planes[plane];

		if (res0.getX() < 0) inCount--;
		if (res0.getY() < 0) inCount--;
		if (res0.getZ() < 0) inCount--;
		if (res0.getW() < 0) inCount--;
		if (res1.getX() < 0) inCount--;
		if (res1.getY() < 0) inCount--;
		if (res1.getZ() < 0) inCount--;
		if (res1.getW() < 0) inCount--;

		allPointsIn = inCount >> 3;
		if (!inCount)
			return Relation::outside; // For some plane all points are outside.
		totalIn += allPointsIn;
	}
	if (6 == totalIn)
		return Relation::inside; // All points are inside.
	return Relation::intersect;
}

void Frustum::Draw()
{
	VERUS_QREF_DD;

	dd.Begin(CGI::DebugDraw::Type::lines, nullptr, false);

	const UINT32 color = VERUS_COLOR_RGBA(0, 0, 0, 255);

	dd.AddLine(_corners[0], _corners[1], color);
	dd.AddLine(_corners[1], _corners[3], color);
	dd.AddLine(_corners[3], _corners[2], color);
	dd.AddLine(_corners[2], _corners[0], color);

	dd.AddLine(_corners[4], _corners[5], color);
	dd.AddLine(_corners[5], _corners[7], color);
	dd.AddLine(_corners[7], _corners[6], color);
	dd.AddLine(_corners[6], _corners[4], color);

	dd.AddLine(_corners[0], _corners[4], color);
	dd.AddLine(_corners[1], _corners[5], color);
	dd.AddLine(_corners[2], _corners[6], color);
	dd.AddLine(_corners[3], _corners[7], color);

	dd.AddLine(_corners[8], _corners[9], color);

	dd.End();
}

Vector4 Frustum::GetBounds(RcMatrix4 m, float& zNear, float& zFar) const
{
	Vector4 ret(
		+FLT_MAX,
		+FLT_MAX,
		-FLT_MAX,
		-FLT_MAX);
	zNear = -FLT_MAX;
	zFar = FLT_MAX;
	VERUS_FOR(i, 8)
	{
		const Vector4 matSpace = m * _corners[i];
		ret = Vector4(
			Math::Min<float>(ret.getX(), matSpace.getX()),
			Math::Min<float>(ret.getY(), matSpace.getY()),
			Math::Max<float>(ret.getZ(), matSpace.getX()),
			Math::Max<float>(ret.getW(), matSpace.getY()));

		// In RH-system closer objects have larger z values!
		zNear = Math::Max<float>(zNear, matSpace.getZ());
		zFar = Math::Min<float>(zFar, matSpace.getZ());
	}
	return ret;
}

RFrustum Frustum::SetNearPlane(RcPoint3 eye, RcVector3 front, float zNear)
{
	_planes[+Name::nearest] = Plane(front, eye + front * zNear);
	return *this;
}

RFrustum Frustum::SetFarPlane(RcPoint3 eye, RcVector3 front, float zFar)
{
	_planes[+Name::furthest] = Plane(-front, eye + front * zFar);
	return *this;
}
