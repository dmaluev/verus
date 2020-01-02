#include "verus.h"

using namespace verus;
using namespace verus::Math;

bool Sphere::IsInside(RcPoint3 point) const
{
	return VMath::distSqr(GetCenter(), point) < GetRadiusSq();
}

bool Sphere::IsOverlappingWith(RcSphere that) const
{
	const float border = GetRadius() + that.GetRadius();
	return VMath::distSqr(GetCenter(), that.GetCenter()) < border * border;
}

void Sphere::EvenlyDistPoints(int count, Vector<Point3>& vPoints)
{
	vPoints.reserve(count);
	const float inc = VERUS_PI * (3 - sqrt(5.f));
	const float off = 2.f / count;
	VERUS_FOR(i, count)
	{
		const float y = i * off - 1 + (off * 0.5f);
		const float r = sqrt(1 - y * y);
		const float phi = i * inc;
		vPoints.push_back(Point3(cos(phi) * r, y, sin(phi) * r));
	}
}
