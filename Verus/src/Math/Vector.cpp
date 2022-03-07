// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

// Vector3:

Vector3 Vector3::MakeFromPointer(const float* p)
{
	return Vector3(p[0], p[1], p[2]);
}

bool Vector3::IsZero() const
{
	const Vector3 zero(0);
	return !memcmp(this, &zero, sizeof(float) * 3);
}

bool Vector3::IsEqual(RcVector3 that, float e) const
{
	return
		glm::epsilonEqual(static_cast<float>(getX()), static_cast<float>(that.getX()), e) &&
		glm::epsilonEqual(static_cast<float>(getY()), static_cast<float>(that.getY()), e) &&
		glm::epsilonEqual(static_cast<float>(getZ()), static_cast<float>(that.getZ()), e);
}

bool Vector3::IsLessThan(RcVector3 that, float e) const
{
	if (!glm::epsilonEqual(static_cast<float>(getX()), static_cast<float>(that.getX()), e)) return getX() < that.getX();
	if (!glm::epsilonEqual(static_cast<float>(getY()), static_cast<float>(that.getY()), e)) return getY() < that.getY();
	if (!glm::epsilonEqual(static_cast<float>(getZ()), static_cast<float>(that.getZ()), e)) return getZ() < that.getZ();
	return false;
}

Vector3 Vector3::Clamp(float mn, float mx) const
{
	return Vector3(
		Math::Clamp(static_cast<float>(getX()), mn, mx),
		Math::Clamp(static_cast<float>(getY()), mn, mx),
		Math::Clamp(static_cast<float>(getZ()), mn, mx));
}

Vector3 Vector3::Clamp(RcVector3 mn, RcVector3 mx) const
{
	return Vector3(
		Math::Clamp(static_cast<float>(getX()), static_cast<float>(mn.getX()), static_cast<float>(mx.getX())),
		Math::Clamp(static_cast<float>(getY()), static_cast<float>(mn.getY()), static_cast<float>(mx.getY())),
		Math::Clamp(static_cast<float>(getZ()), static_cast<float>(mn.getZ()), static_cast<float>(mx.getZ())));
}

Vector3 Vector3::Floor() const
{
	return Vector3(
		floor(getX()),
		floor(getY()),
		floor(getZ()));
}

Vector3 Vector3::Pow(float p) const
{
	return Vector3(
		pow(getX(), p),
		pow(getY(), p),
		pow(getZ(), p));
}

Vector3 Vector3::Mod(float x) const
{
	return Vector3(
		fmod(static_cast<float>(getX()), x),
		fmod(static_cast<float>(getY()), x),
		fmod(static_cast<float>(getZ()), x));
}

Vector3 Vector3::Mod(RcVector3 that) const
{
	return Vector3(
		fmod(static_cast<float>(getX()), static_cast<float>(that.getX())),
		fmod(static_cast<float>(getY()), static_cast<float>(that.getY())),
		fmod(static_cast<float>(getZ()), static_cast<float>(that.getZ())));
}

String Vector3::ToString(bool compact) const
{
	char txt[96];
	const float x = getX();
	const float y = getY();
	const float z = getZ();
	sprintf_s(txt, compact ? "%g %g %g" : "%f %f %f", x, y, z);
	return txt;
}

String Vector3::ToString2(bool compact) const
{
	char txt[64];
	const float x = getX();
	const float y = getY();
	sprintf_s(txt, compact ? "%g %g" : "%f %f", x, y);
	return txt;
}

RVector3 Vector3::FromString(CSZ sz)
{
	float x = 0, y = 0, z = 0;
	if (sz)
		sscanf(sz, "%f %f %f", &x, &y, &z);
	*this = Vector3(x, y, z);
	return *this;
}

void Vector3::EulerFromQuaternion(RcQuat q)
{
	*this = glm::eulerAngles(q.GLM());
}

void Vector3::EulerToQuaternion(RQuat q) const
{
	q = glm::quat(GLM());
}

void Vector3::LimitDot(RcVector3 v, float d)
{
	const float wasDot = VMath::dot(*this, v);
	if (wasDot >= d)
		return;
	const float delta = d - wasDot;
	const Point3 p0 = *this + v * delta;
	const Point3 p1 = v * d;
	const float len = VMath::dist(p0, p1);
	if (len < VERUS_FLOAT_THRESHOLD)
		return;
	const float targetLen = sqrt(1 - d * d);
	const float ratio = targetLen / len;
	const Vector3 perp = p0 - p1;
	*this = p1 + perp * ratio;
}

Vector3 Vector3::Reflect(RcVector3 normal, float bounce) const
{
	return (*this) - normal * ((1 + bounce) * VMath::dot(*this, normal));
}

Vector3 Vector3::Perpendicular() const
{
	Vector3 axis = VMath::cross(Vector3(1, 0, 0), (*this));
	if (VMath::lengthSqr(axis) < VERUS_FLOAT_THRESHOLD * VERUS_FLOAT_THRESHOLD)
		axis = VMath::cross(Vector3(0, 1, 0), (*this));
	return axis;
}

// Vector4:

Vector4 Vector4::MakeFromPointer(const float* p)
{
	return Vector4(p[0], p[1], p[2], p[3]);
}

bool Vector4::IsZero() const
{
	const Vector4 zero(0);
	return !memcmp(this, &zero, sizeof(float) * 4);
}

Vector4 Vector4::Clamp(float mn, float mx) const
{
	return Vector4(
		Math::Clamp(static_cast<float>(getX()), mn, mx),
		Math::Clamp(static_cast<float>(getY()), mn, mx),
		Math::Clamp(static_cast<float>(getZ()), mn, mx),
		Math::Clamp(static_cast<float>(getW()), mn, mx));
}

Vector4 Vector4::Clamp(RcVector4 mn, RcVector4 mx) const
{
	return Vector4(
		Math::Clamp(static_cast<float>(getX()), static_cast<float>(mn.getX()), static_cast<float>(mx.getX())),
		Math::Clamp(static_cast<float>(getY()), static_cast<float>(mn.getY()), static_cast<float>(mx.getY())),
		Math::Clamp(static_cast<float>(getZ()), static_cast<float>(mn.getZ()), static_cast<float>(mx.getZ())),
		Math::Clamp(static_cast<float>(getW()), static_cast<float>(mn.getW()), static_cast<float>(mx.getW())));
}

Vector4 Vector4::Mod(float x) const
{
	return Vector4(
		fmod(static_cast<float>(getX()), x),
		fmod(static_cast<float>(getY()), x),
		fmod(static_cast<float>(getZ()), x),
		fmod(static_cast<float>(getW()), x));
}

Vector4 Vector4::Mod(const Vector4& that) const
{
	return Vector4(
		fmod(static_cast<float>(getX()), static_cast<float>(that.getX())),
		fmod(static_cast<float>(getY()), static_cast<float>(that.getY())),
		fmod(static_cast<float>(getZ()), static_cast<float>(that.getZ())),
		fmod(static_cast<float>(getW()), static_cast<float>(that.getW())));
}

String Vector4::ToString(bool compact) const
{
	char txt[128];
	const float x = getX();
	const float y = getY();
	const float z = getZ();
	const float w = getW();
	sprintf_s(txt, compact ? "%g %g %g %g" : "%f %f %f %f", x, y, z, w);
	return txt;
}

RVector4 Vector4::FromString(CSZ sz)
{
	float x = 0, y = 0, z = 0, w = 0;
	if (sz)
		sscanf(sz, "%f %f %f %f", &x, &y, &z, &w);
	*this = Vector4(x, y, z, w);
	return *this;
}

Vector4 Vector4::MakeFromColor(UINT32 color, bool sRGB)
{
	Vector4 c;
	c.FromColor(color, sRGB);
	return c;
}

RVector4 Vector4::FromColor(UINT32 color, bool sRGB)
{
	float rgba[4];
	Convert::ColorInt32ToFloat(color, rgba, sRGB);
	*this = MakeFromPointer(rgba);
	return *this;
}

UINT32 Vector4::ToColor(bool sRGB) const
{
	return Convert::ColorFloatToInt32(ToPointer(), sRGB);
}

RVector4 Vector4::FromColorString(CSZ sz, bool sRGB)
{
	float rgba[4];
	Convert::ColorTextToFloat4(sz, rgba, sRGB);
	*this = MakeFromPointer(rgba);
	return *this;
}

String Vector4::ToColorString(bool sRGB)
{
	const UINT32 color = Convert::ColorFloatToInt32(ToPointer(), sRGB);
	char txt[16];
	sprintf_s(txt, "%02X%02X%02X%02X",
		(color >> 0) & 0xFF,
		(color >> 8) & 0xFF,
		(color >> 16) & 0xFF,
		(color >> 24) & 0xFF);
	return txt;
}

Vector4 Vector4::MakeRectangle(float l, float t, float r, float b)
{
	return Vector4(l, t, r - l, b - t);
}

float Vector4::Left() const
{
	return getX();
}

float Vector4::Top() const
{
	return getY();
}

float Vector4::Width() const
{
	return getZ();
}

float Vector4::Height() const
{
	return getW();
}

float Vector4::Right() const
{
	return getX() + getZ();
}

float Vector4::Bottom() const
{
	return getY() + getW();
}

bool Vector4::IsInsideRect(float x, float y) const
{
	return
		x >= getX() &&
		y >= getY() &&
		x < Right() &&
		y < Bottom();
}

Vector4 Vector4::ToGuiCoord() const
{
	Vector4 res = *this;
	res /= res.getW();
	return VMath::mulPerElem(res, Vector4(0.5f, -0.5f, 1, 1)) + Vector4(0.5f, 0.5f);
}

// Point3:

Point3 Point3::MakeFromPointer(const float* p)
{
	return Point3(p[0], p[1], p[2]);
}

void Point3::ToArray3(float* p) const
{
	memcpy(p, this, sizeof(float) * 3);
}

bool Point3::IsZero() const
{
	const Point3 zero(0);
	return !memcmp(this, &zero, sizeof(float) * 3);
}

bool Point3::IsEqual(RcPoint3 that, float e) const
{
	return
		glm::epsilonEqual(static_cast<float>(getX()), static_cast<float>(that.getX()), e) &&
		glm::epsilonEqual(static_cast<float>(getY()), static_cast<float>(that.getY()), e) &&
		glm::epsilonEqual(static_cast<float>(getZ()), static_cast<float>(that.getZ()), e);
}

bool Point3::IsLessThan(RcPoint3 that, float e) const
{
	if (!glm::epsilonEqual(static_cast<float>(getX()), static_cast<float>(that.getX()), e)) return getX() < that.getX();
	if (!glm::epsilonEqual(static_cast<float>(getY()), static_cast<float>(that.getY()), e)) return getY() < that.getY();
	if (!glm::epsilonEqual(static_cast<float>(getZ()), static_cast<float>(that.getZ()), e)) return getZ() < that.getZ();
	return false;
}

String Point3::ToString(bool compact) const
{
	char txt[96];
	const float x = getX();
	const float y = getY();
	const float z = getZ();
	sprintf_s(txt, compact ? "%g %g %g" : "%f %f %f", x, y, z);
	return txt;
}

String Point3::ToString2(bool compact) const
{
	char txt[64];
	const float x = getX();
	const float y = getY();
	sprintf_s(txt, compact ? "%g %g" : "%f %f", x, y);
	return txt;
}

RPoint3 Point3::FromString(CSZ sz)
{
	float x = 0, y = 0, z = 0;
	if (sz)
		sscanf(sz, "%f %f %f", &x, &y, &z);
	*this = Point3(x, y, z);
	return *this;
}
