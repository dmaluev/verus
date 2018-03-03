#include "verus.h"

using namespace verus;

bool Math::IsPowerOfTwo(int x)
{
	if (x <= 0)
		return false;
	return !(x&(x - 1));
}

int Math::HighestBit(int x)
{
	int bit = -1;
	for (int tmp = x; tmp; tmp >>= 1, ++bit) {}
	return bit;
}

bool Math::IsNaN(float x)
{
	// 'isnan' : is not a member of 'std' :(
	volatile float y = x;
	return y != y;
}

float Math::ToRadians(float deg)
{
	return deg * (VERUS_PI*(1 / 180.f));
}

float Math::ToDegrees(float rad)
{
	return rad * (180 / VERUS_PI);
}

float Math::WrapAngle(float rad)
{
	return rad - floor(rad*(1 / VERUS_2PI) + 0.5f)*VERUS_2PI;
}

float Math::Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

float Math::SmoothStep(float a, float b, float t)
{
	const float x = Clamp((t - a) / (b - a), 0.f, 1.f);
	return x * x*(3 - (x + x));
}

float Math::LinearToSin(float t)
{
	return sin(t*VERUS_PI);
}

float Math::LinearToCos(float t)
{
	return cos(t*VERUS_PI)*-0.5f + 0.5f; // [1 to -1] -> [0 to 1].
}

Transform3 Math::BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx)
{
	const Vector3 d = mx - mn;
	const Point3 c = VMath::lerp(0.5f, mn, mx);
	return VMath::appendScale(Transform3::translation(c - Point3(0, d.getY()*0.5f, 0)), d);
}

Vector3 Math::TriangleNormal(RcPoint3 a, RcPoint3 b, RcPoint3 c)
{
	return VMath::normalize(VMath::cross(a - b, a - c));
}

float Math::TriangleArea(
	const glm::vec3& p0,
	const glm::vec3& p1,
	const glm::vec3& p2)
{
	// https://en.wikipedia.org/wiki/Heron%27s_formula
	const float a = glm::length(p2 - p1);
	const float b = glm::length(p0 - p2);
	const float c = glm::length(p1 - p0);
	const float s = (a + b + c)*0.5f;
	return sqrt(s*(s - a)*(s - b)*(s - c));
}
