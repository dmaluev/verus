// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Math;

Bounds::Bounds()
{
	Reset();
}

Bounds::Bounds(RcPoint3 mn, RcPoint3 mx) :
	_min(mn),
	_max(mx)
{
	VERUS_RT_ASSERT(glm::all(glm::lessThanEqual(_min.GLM(), _max.GLM())));
}

Bounds::~Bounds()
{
}

bool Bounds::IsNull() const
{
	const Point3 mn(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	const Point3 mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	return
		!memcmp(&mn, &_min, sizeof(float) * 3) &&
		!memcmp(&mx, &_max, sizeof(float) * 3);
}

RBounds Bounds::Set(RcPoint3 mn, RcPoint3 mx)
{
	_min = mn;
	_max = mx;
	VERUS_RT_ASSERT(glm::all(glm::lessThanEqual(_min.GLM(), _max.GLM())));
	return *this;
}

RBounds Bounds::Set(float mn, float mx, int axis)
{
	VERUS_RT_ASSERT(mn <= mx);
	_min.setElem(axis, mn);
	_max.setElem(axis, mx);
	return *this;
}

RBounds Bounds::Set(RcBounds that, int axis)
{
	_min.setElem(axis, that._min.getElem(axis));
	_max.setElem(axis, that._max.getElem(axis));
	return *this;
}

RBounds Bounds::Reset()
{
	_min = Point3(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	_max = Point3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	return *this;
}

RBounds Bounds::Include(RcPoint3 point)
{
	_min = VMath::minPerElem(_min, point);
	_max = VMath::maxPerElem(_max, point);
	return *this;
}

RBounds Bounds::CombineWith(RcBounds that)
{
	_min = VMath::minPerElem(_min, that._min);
	_max = VMath::maxPerElem(_max, that._max);
	return *this;
}

RBounds Bounds::MoveBy(RcVector3 offset)
{
	_min += offset;
	_max += offset;
	return *this;
}

Bounds Bounds::MakeFromCenterExtents(RcPoint3 center, RcVector3 extents)
{
	return Bounds(center - extents, center + extents);
}

Bounds Bounds::MakeFromOrientedBox(RcBounds that, RcTransform3 tr)
{
	Point3 points[8];
	that.GetCorners(points);
	VERUS_FOR(i, 8)
		points[i] = tr * points[i];
	Bounds bounds(points[7], points[7]);
	VERUS_FOR(i, 7)
		bounds.Include(points[i]);
	return bounds;
}

RBounds Bounds::ScaleBy(float scale)
{
	*this = MakeFromCenterExtents(GetCenter(), GetExtents() * scale);
	return *this;
}

RBounds Bounds::FattenBy(float d)
{
	_min -= Vector3(d, d, d);
	_max += Vector3(d, d, d);
	return *this;
}

bool Bounds::IsInside(RcPoint3 point) const
{
	if ((point.getX() < _min.getX()) || (point.getX() >= _max.getX())) return false;
	if ((point.getY() < _min.getY()) || (point.getY() >= _max.getY())) return false;
	if ((point.getZ() < _min.getZ()) || (point.getZ() >= _max.getZ())) return false;
	return true;
}

bool Bounds::IsInside2D(RcPoint3 point) const
{
	if ((point.getX() < _min.getX()) || (point.getX() >= _max.getX())) return false;
	if ((point.getY() < _min.getY()) || (point.getY() >= _max.getY())) return false;
	return true;
}

bool Bounds::IsOverlappingWith(RcBounds that) const
{
	if ((that._min.getX() >= _max.getX()) || (_min.getX() >= that._max.getX())) return false;
	if ((that._min.getY() >= _max.getY()) || (_min.getY() >= that._max.getY())) return false;
	if ((that._min.getZ() >= _max.getZ()) || (_min.getZ() >= that._max.getZ())) return false;
	return true;
}

bool Bounds::IsOverlappingWith2D(RcBounds that, int axis) const
{
	static const int i[3] = { 1, 0, 0 };
	static const int j[3] = { 2, 2, 1 };
	const int ii = i[axis];
	const int jj = j[axis];
	if ((that._min.getElem(ii) >= _max.getElem(ii)) || (_min.getElem(ii) >= that._max.getElem(ii))) return false;
	if ((that._min.getElem(jj) >= _max.getElem(jj)) || (_min.getElem(jj) >= that._max.getElem(jj))) return false;
	return true;
}

void Bounds::GetCorners(PPoint3 points) const
{
	VERUS_FOR(i, 8)
	{
		points[i] = Point3(
			((i >> 0) & 0x1) ? _max.getX() : _min.getX(),
			((i >> 1) & 0x1) ? _max.getY() : _min.getY(),
			((i >> 2) & 0x1) ? _max.getZ() : _min.getZ());
	}
}

void Bounds::GetCorners(PMatrix4 m) const
{
	float minmax[8];
	memcpy(minmax + 0, _min.ToPointer(), 12);
	memcpy(minmax + 4, _max.ToPointer(), 12);
	float data[8][4];
	VERUS_FOR(i, 4)
	{
		const UINT32 i0 = i;
		const UINT32 i1 = i + 4;
		data[0][i] = minmax[0 + (((i0 >> 0) & 0x1) << 2)];
		data[1][i] = minmax[1 + (((i0 >> 1) & 0x1) << 2)];
		data[2][i] = minmax[2 + (((i0 >> 2) & 0x1) << 2)];
		data[3][i] = 1;
		data[4][i] = minmax[0 + (((i1 >> 0) & 0x1) << 2)];
		data[5][i] = minmax[1 + (((i1 >> 1) & 0x1) << 2)];
		data[6][i] = minmax[2 + (((i1 >> 2) & 0x1) << 2)];
		data[7][i] = 1;
	}
	memcpy(m, data, 128);
}

Sphere Bounds::GetSphere() const
{
	return Sphere(GetCenter(), VMath::length(GetExtents()));
}

Matrix4 Bounds::GetMatrix() const
{
	const Matrix4 matT = Matrix4::translation(
		Vector3(GetCenter() - VMath::mulPerElem(GetExtents(), Vector3(0, 1, 0))));
	return VMath::appendScale(matT, GetDimensions());
}

Transform3 Bounds::GetDrawTransform() const
{
	return Math::BoundsDrawMatrix(_min, _max);
}

RBounds Bounds::MirrorY()
{
	const float miny = _min.getY();
	const float maxy = _max.getY();
	_min.setY(-maxy);
	_max.setY(-miny);
	return *this;
}

RBounds Bounds::Wrap()
{
	_min = glm::fract(_min.GLM());
	_max = glm::fract(_max.GLM());
	return *this;
}

void Bounds::GetQuadrant2D(int id, RBounds that) const
{
	const Point3 center = GetCenter();
	switch (id)
	{
	case 0: // 000:
		that._min = Point3(_min.getX(), _min.getY());
		that._max = Point3(center.getX(), center.getY());
		break;
	case 1: // 001:
		that._min = Point3(center.getX(), _min.getY());
		that._max = Point3(_max.getX(), center.getY());
		break;
	case 2: // 010:
		that._min = Point3(_min.getX(), center.getY());
		that._max = Point3(center.getX(), _max.getY());
		break;
	case 3: // 011:
		that._min = Point3(center.getX(), center.getY());
		that._max = Point3(_max.getX(), _max.getY());
		break;
	}
}

void Bounds::GetQuadrant3D(int id, RBounds that) const
{
	const Point3 center = GetCenter();
	GetQuadrant2D(id & 0x3, that);
	switch (id & 0x4)
	{
	case 0: // 0xx:
		that._min.setZ(_min.getZ());
		that._max.setZ(center.getZ());
		break;
	case 4: // 1xx:
		that._min.setZ(center.getZ());
		that._max.setZ(_max.getZ());
		break;
	}
}

float Bounds::GetDiagonal() const
{
	return VMath::length(GetDimensions());
}

float Bounds::GetAverageSize() const
{
	const float scale = 1 / 3.f;
	return VMath::dot(GetDimensions(), Vector3::Replicate(scale));
}

float Bounds::GetMaxSide() const
{
	const Vector3 d = GetDimensions();
	const float* p = d.ToPointer();
	return *std::max_element(p, p + 3);
}

Vector3 Bounds::GetExtentsFromOrigin(RcVector3 scale) const
{
	const Vector3 mn = VMath::mulPerElem(Vector3(_min), scale);
	const Vector3 mx = VMath::mulPerElem(Vector3(_max), scale);
	return VMath::maxPerElem(
		VMath::absPerElem(mn),
		VMath::absPerElem(mx));
}

float Bounds::GetMaxExtentFromOrigin(RcVector3 scale) const
{
	const Vector3 d = GetExtentsFromOrigin(scale);
	const float* p = d.ToPointer();
	return *std::max_element(p, p + 3);
}

RBounds Bounds::ToUnitBounds()
{
	const float mn = Math::Min<float>(_min.getX(), _min.getZ());
	const float mx = Math::Max<float>(_max.getX(), _max.getZ());
	_min.setX(mn);
	_min.setZ(mn);
	_max.setX(mx);
	_max.setZ(mx);
	return *this;
}
