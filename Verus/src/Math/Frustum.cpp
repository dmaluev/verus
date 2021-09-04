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

Frustum Frustum::MakeFromMatrix(RcMatrix4 matVP)
{
	Frustum ret;
	ret._matInvVP = VMath::inverse(matVP);
	const Vector4 corners[10] =
	{
		Vector4(-1, -1, 0, 1),
		Vector4(+1, -1, 0, 1),
		Vector4(-1, +1, 0, 1),
		Vector4(+1, +1, 0, 1),
		Vector4(-1, -1, 1, 1),
		Vector4(+1, -1, 1, 1),
		Vector4(-1, +1, 1, 1),
		Vector4(+1, +1, 1, 1),
		Vector4(+0, +0, 0, 1), // At zNear.
		Vector4(+0, +0, 1, 1) // At zFar.
	};
	VERUS_FOR(i, 10)
	{
		Vector4 inv = ret._matInvVP * corners[i];
		inv /= inv.getW();
		ret._corners[i] = inv.getXYZ();
	};
	const Matrix4 mat = VMath::transpose(matVP);
	ret._planes[+Name::left]     /**/ = mat.getCol3() + mat.getCol0();
	ret._planes[+Name::right]    /**/ = mat.getCol3() - mat.getCol0();
	ret._planes[+Name::bottom]   /**/ = mat.getCol3() + mat.getCol1();
	ret._planes[+Name::top]      /**/ = mat.getCol3() - mat.getCol1();
	ret._planes[+Name::nearest]  /**/ = mat.getCol3() + mat.getCol2();
	ret._planes[+Name::furthest] /**/ = mat.getCol3() - mat.getCol2();
	VERUS_FOR(i, 6)
		ret._planes[i].Normalize();
	return ret;
}

RFrustum Frustum::FromMatrix(RcMatrix4 matVP)
{
	*this = MakeFromMatrix(matVP);
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

Relation Frustum::ContainsAabb(RcBounds bounds) const
{
	int totalIn = 0, inCount, allPointsIn;
	Vector4 res0, res1;
	Matrix4 m[2];
	bounds.GetCorners(m);
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
