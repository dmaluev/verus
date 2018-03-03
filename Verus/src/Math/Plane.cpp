#include "verus.h"

using namespace verus;
using namespace verus::Math;

Plane::Plane()
{
}

Plane::Plane(RcVector4 that) :
	Vector4(that)
{
}

Plane::Plane(RcVector3 normal, float d) :
	Vector4(normal, d)
{
}

Plane::Plane(RcVector3 normal, RcPoint3 point) :
	Vector4(normal, -VMath::dot(normal, Vector3(point)))
{
}

Plane::Plane(RcPoint3 pointA, RcPoint3 pointB, RcPoint3 pointC)
{
	const Vector3 normal = Math::TriangleNormal(pointA, pointB, pointC);
	*this = Plane(normal, pointA);
}

Plane::~Plane()
{
}

float Plane::DistanceTo(RcPoint3 point) const
{
	return VMath::dot(*this, Vector4(Vector3(point), 1));
}

Plane& Plane::Normalize()
{
	*this = *this / VMath::length(getXYZ());
	return *this;
}

bool Plane::IntersectSegment(RcPoint3 pointA, RcPoint3 pointB, RPoint3 point, float* t)
{
	float distA = DistanceTo(pointA);
	float distB = DistanceTo(pointB);
	if ((distA >= 0 && distB >= 0) || (distA < 0 && distB < 0))
		return false;
	distA = abs(distA);
	distB = abs(distB);
	const float tt = distA / (distA + distB);
	const Vector3 edge = pointB - pointA;
	const Vector3 offset = edge * tt;
	point = pointA + offset;
	if (t)
		*t = tt;
	return true;
}
