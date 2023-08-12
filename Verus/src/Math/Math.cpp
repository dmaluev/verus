// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

VERUS_CT_ASSERT(sizeof(glm::vec2) == 8);
VERUS_CT_ASSERT(sizeof(glm::vec3) == 12);
VERUS_CT_ASSERT(sizeof(glm::vec4) == 16);
VERUS_CT_ASSERT(sizeof(glm::mat4) == 64);
VERUS_CT_ASSERT(sizeof(glm::mat4x3) == 48);

using namespace verus;

static CSZ g_easings[] =
{
	"",

	"sineIn",
	"sineOut",
	"sineInOut",

	"quadIn",
	"quadOut",
	"quadInOut",

	"cubicIn",
	"cubicOut",
	"cubicInOut",

	"quartIn",
	"quartOut",
	"quartInOut",

	"quintIn",
	"quintOut",
	"quintInOut",

	"expoIn",
	"expoOut",
	"expoInOut",

	"circIn",
	"circOut",
	"circInOut",

	"backIn",
	"backOut",
	"backInOut",

	"elasticIn",
	"elasticOut",
	"elasticInOut",

	"bounceIn",
	"bounceOut",
	"bounceInOut"
};

bool Math::IsPowerOfTwo(int x)
{
	if (x <= 0)
		return false;
	return !(x & (x - 1));
}

UINT32 Math::NextPowerOfTwo(UINT32 x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

int Math::HighestBit(int x)
{
	int bit = -1;
	for (int tmp = x; tmp; tmp >>= 1, ++bit) {}
	return bit;
}

int Math::LowestBit(int x)
{
	if (!x)
		return -1;
	int bit = 0;
	while (!(x & 1))
	{
		x >>= 1;
		++bit;
	}
	return bit;
}

bool Math::IsNaN(float x)
{
	return std::isnan(x);
}

float Math::ToRadians(float deg)
{
	return deg * (VERUS_PI * (1 / 180.f));
}

float Math::ToDegrees(float rad)
{
	return rad * (180 / VERUS_PI);
}

float Math::WrapAngle(float rad)
{
	return rad - floor(rad * (1 / VERUS_2PI) + 0.5f) * VERUS_2PI;
}

float Math::Lerp(float a, float b, float t)
{
	return a + t * (b - a);
}

float Math::LerpAngles(float a, float b, float t)
{
	a = WrapAngle(a);
	b = WrapAngle(b);
	if (abs(a - b) >= VERUS_PI)
	{
		if (a < b)
			a += VERUS_2PI;
		else
			b += VERUS_2PI;
	}
	return WrapAngle(Lerp(a, b, t));
}

float Math::SmoothStep(float a, float b, float t)
{
	const float x = Clamp((t - a) / (b - a), 0.f, 1.f);
	return x * x * (3 - (x + x));
}

float Math::ApplyEasing(Easing easing, float x)
{
	switch (easing)
	{
		using enum Easing;

	case none: return x;

	case sineIn: return glm::sineEaseIn(x);
	case sineOut: return glm::sineEaseOut(x);
	case sineInOut: return glm::sineEaseInOut(x);

	case quadIn: return glm::quadraticEaseIn(x);
	case quadOut: return glm::quadraticEaseOut(x);
	case quadInOut: return glm::quadraticEaseInOut(x);

	case cubicIn: return glm::cubicEaseIn(x);
	case cubicOut: return glm::cubicEaseOut(x);
	case cubicInOut: return glm::cubicEaseInOut(x);

	case quartIn: return glm::quarticEaseIn(x);
	case quartOut: return glm::quarticEaseOut(x);
	case quartInOut: return glm::quarticEaseInOut(x);

	case quintIn: return glm::quinticEaseIn(x);
	case quintOut: return glm::quinticEaseOut(x);
	case quintInOut: return glm::quinticEaseInOut(x);

	case expoIn: return glm::exponentialEaseIn(x);
	case expoOut: return glm::exponentialEaseOut(x);
	case expoInOut: return glm::exponentialEaseInOut(x);

	case circIn: return glm::circularEaseIn(x);
	case circOut: return glm::circularEaseOut(x);
	case circInOut: return glm::circularEaseInOut(x);

	case backIn: return glm::backEaseIn(x);
	case backOut: return glm::backEaseOut(x);
	case backInOut: return glm::backEaseInOut(x);

	case elasticIn: return glm::elasticEaseIn(x);
	case elasticOut: return glm::elasticEaseOut(x);
	case elasticInOut: return glm::elasticEaseInOut(x);

	case bounceIn: return glm::bounceEaseIn(x);
	case bounceOut: return glm::bounceEaseOut(x);
	case bounceInOut: return glm::bounceEaseInOut(x);
	}
	VERUS_RT_FAIL("Unknown easing.");
	return x;
}

Easing Math::EasingFromString(CSZ s)
{
	const int count = VERUS_COUNT_OF(g_easings);
	VERUS_FOR(i, count)
	{
		if (!strcmp(s, g_easings[i]))
			return static_cast<Easing>(i);
	}
	throw VERUS_RECOVERABLE << "Unknown easing string: " << s;
}

CSZ Math::EasingToString(Easing easing)
{
	VERUS_RT_ASSERT(easing >= Easing::none && easing < Easing::count);
	return g_easings[+easing];
}

Quat Math::NLerp(float t, RcQuat qA, RcQuat qB)
{
	Quat ret;
	const auto dp = VMath::dot(qA, qB);
	if (dp < 0)
	{
		const Quat qNeg = -qB;
		ret = VMath::lerp(t, qA, qNeg);
	}
	else
	{
		ret = VMath::lerp(t, qA, qB);
	}
	ret = VMath::normalize(reinterpret_cast<RcVector4>(ret)); // Use more accurate normalize!
	return ret;
}

Vector3 Math::Barycentric(RcPoint3 a, RcPoint3 b, RcPoint3 c, RcPoint3 p)
{
	const Vector3 ab = b - a;
	const Vector3 ac = c - a;
	const Vector3 pa = a - p;
	const Vector3 pb = b - p;
	const Vector3 pc = c - p;
	const Vector3 crossABAC = VMath::cross(ab, ac);
	const Vector3 n = VMath::normalize(crossABAC); // Compute the normal of the triangle.
	const float areaABC = VMath::dot(n, crossABAC); // Compute twice area of triangle ABC.
	const float areaPCA = VMath::dot(n, VMath::cross(pc, pa)); // Compute b.
	const float areaPAB = VMath::dot(n, VMath::cross(pa, pb)); // Compute c.
	return Vector3(areaPCA / areaABC, areaPAB / areaABC);
}

bool Math::IsPointInsideBarycentric(RcVector3 bc)
{
	const float sum = bc.getX() + bc.getY();
	return
		(bc.getX() >= 0 && bc.getX() < 1) &&
		(bc.getY() >= 0 && bc.getY() < 1) &&
		(sum >= 0 && sum < 1);
}

Vector3 Math::TriangleNormal(RcPoint3 a, RcPoint3 b, RcPoint3 c)
{
	return VMath::normalize(VMath::cross(a - b, a - c));
}

float Math::TriangleArea(
	const glm::vec3& a,
	const glm::vec3& b,
	const glm::vec3& c)
{
	// https://en.wikipedia.org/wiki/Heron%27s_formula
	const float bc = glm::length(c - b);
	const float ca = glm::length(a - c);
	const float ab = glm::length(b - a);
	const float s = (bc + ca + ab) * 0.5f;
	return sqrt(s * (s - bc) * (s - ca) * (s - ab));
}

bool Math::IsPointInsideTriangle(
	const glm::vec2& a,
	const glm::vec2& b,
	const glm::vec2& c,
	const glm::vec2& p)
{
	const float dx = p.x - c.x;
	const float dy = p.y - c.y;
	const float dx21 = c.x - b.x;
	const float dy12 = b.y - c.y;
	const float d = dy12 * (a.x - c.x) + dx21 * (a.y - c.y);
	const float s = dy12 * dx + dx21 * dy;
	const float t = (c.y - a.y) * dx + (a.x - c.x) * dy;
	if (d < 0)
		return s <= 0 && t <= 0 && s + t >= d;
	return s >= 0 && t >= 0 && s + t <= d;
}

int Math::StripGridIndexCount(int polyCountWidth, int polyCountHeight)
{
	const int vertCountWidth = polyCountWidth + 1;
	return (vertCountWidth * 2) * polyCountHeight + (polyCountHeight - 1) * 2; // Strip and primitive restart values.
}

void Math::CreateStripGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices)
{
	vIndices.resize(StripGridIndexCount(polyCountWidth, polyCountHeight));
	int offset = 0;
	const int vertCountWidth = polyCountWidth + 1;
	VERUS_FOR(h, polyCountHeight)
	{
		if (h > 0) // Primitive restart value:
		{
			vIndices[offset++] = 0xFFFF;
			vIndices[offset++] = 0xFFFF;
		}
		if (h & 0x1)
		{
			for (int w = polyCountWidth; w >= 0; --w)
			{
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
				vIndices[offset++] = vertCountWidth * (h + 0) + w;
			}
		}
		else
		{
			VERUS_FOR(w, vertCountWidth)
			{
				vIndices[offset++] = vertCountWidth * (h + 0) + w;
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
			}
		}
	}
}

void Math::CreateListGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices)
{
	vIndices.resize(polyCountWidth * polyCountHeight * 6);
	int offset = 0;
	const int vertCountWidth = polyCountWidth + 1;
	VERUS_FOR(h, polyCountHeight)
	{
		if (h & 0x1)
		{
			for (int w = polyCountWidth - 1; w >= 0; --w)
			{
				vIndices[offset++] = vertCountWidth * (h + 1) + w + 1;
				vIndices[offset++] = vertCountWidth * (h + 0) + w + 1;
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
				vIndices[offset++] = vertCountWidth * (h + 0) + w + 1;
				vIndices[offset++] = vertCountWidth * (h + 0) + w;
			}
		}
		else
		{
			VERUS_FOR(w, polyCountWidth)
			{
				vIndices[offset++] = vertCountWidth * (h + 0) + w;
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
				vIndices[offset++] = vertCountWidth * (h + 0) + w + 1;
				vIndices[offset++] = vertCountWidth * (h + 0) + w + 1;
				vIndices[offset++] = vertCountWidth * (h + 1) + w;
				vIndices[offset++] = vertCountWidth * (h + 1) + w + 1;
			}
		}
	}
}

bool Math::CheckIndexBuffer(Vector<UINT16>& vIndices, int maxIndex)
{
	Vector<UINT16> vTemp(vIndices.begin(), vIndices.end());
	std::sort(vTemp.begin(), vTemp.end());
	auto itRestart = std::find(vTemp.begin(), vTemp.end(), 0xFFFF);
	if (itRestart != vTemp.end())
		vTemp.erase(itRestart, vTemp.end());
	if (vTemp.empty())
		return false;
	if (vTemp.front() != 0)
		return false;
	if (vTemp.back() != maxIndex)
		return false;
	auto it = std::adjacent_find(vTemp.begin(), vTemp.end(), [](UINT16 a, UINT16 b) {return (b - a) > 1; });
	return vTemp.end() == it;
}

Transform3 Math::BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx)
{
	const Vector3 d = mx - mn;
	const Point3 c = VMath::lerp(0.5f, mn, mx);
	return VMath::appendScale(Transform3::translation(c - Point3(0, d.getY() * 0.5f, 0)), d);
}

Transform3 Math::BoundsBoxMatrix(RcPoint3 mn, RcPoint3 mx)
{
	const Vector3 d = mx - mn;
	const Vector3 c = VMath::lerp(0.5f, mn, mx);
	return VMath::appendScale(Transform3::translation(c), d);
}

float Math::ComputeOnePixelDistance(float objectSize, float viewportHeightInPixels, float fovY)
{
	// Assume that object occupies one pixel, then the whole viewport height will be:
	const float viewportHeightInMeters = objectSize * viewportHeightInPixels;
	return (0.5f * viewportHeightInMeters) / tan(0.5f * fovY);
}

float Math::ComputeDistToMipScale(float texHeight, float viewportHeightInPixels, float objectSize, float fovY)
{
	const float fillScreenScale = viewportHeightInPixels / texHeight;
	const float viewportHeightInMeters = objectSize * fillScreenScale;
	return tan(0.5f * fovY) / (0.5f * viewportHeightInMeters); // Scale distance by this value.
}

void Math::Quadrant(const int** ppSrcMinMax, int** ppDestMinMax, int half, int id)
{
	VERUS_RT_ASSERT(id >= 0 && id < 4);
	switch (id)
	{
	case 0: // -x -z:
		ppDestMinMax[0][0] = ppSrcMinMax[0][0];
		ppDestMinMax[0][1] = ppSrcMinMax[0][1];
		ppDestMinMax[1][0] = ppSrcMinMax[1][0] - half;
		ppDestMinMax[1][1] = ppSrcMinMax[1][1] - half;
		break;
	case 1: // +x -z:
		ppDestMinMax[0][0] = ppSrcMinMax[0][0] + half;
		ppDestMinMax[0][1] = ppSrcMinMax[0][1];
		ppDestMinMax[1][0] = ppSrcMinMax[1][0];
		ppDestMinMax[1][1] = ppSrcMinMax[1][1] - half;
		break;
	case 2: // -x +z:
		ppDestMinMax[0][0] = ppSrcMinMax[0][0];
		ppDestMinMax[0][1] = ppSrcMinMax[0][1] + half;
		ppDestMinMax[1][0] = ppSrcMinMax[1][0] - half;
		ppDestMinMax[1][1] = ppSrcMinMax[1][1];
		break;
	case 3: // +x +z:
		ppDestMinMax[0][0] = ppSrcMinMax[0][0] + half;
		ppDestMinMax[0][1] = ppSrcMinMax[0][1] + half;
		ppDestMinMax[1][0] = ppSrcMinMax[1][0];
		ppDestMinMax[1][1] = ppSrcMinMax[1][1];
		break;
	}
}

int Math::ComputeMipLevels(int w, int h, int d)
{
	return 1 + static_cast<int>(floor(log2(Max(Max(w, h), d))));
}

BYTE Math::CombineOcclusion(BYTE a, BYTE b)
{
	const BYTE ab = a * b / 0xFF;
	const BYTE mn = Math::Min(a, b);
	return (ab + mn + mn) / 3;
}

Transform3 Math::QuadMatrix(float x, float y, float w, float h)
{
	const Transform3 matT = Transform3::translation(Vector3(x * 2 - 1, y * -2 + 1));
	const Transform3 mat = matT * VMath::prependScale(Vector3(w, h), Transform3::translation(Vector3(1, -1, 0)));
	return mat;
}

Transform3 Math::ToUVMatrix(float zOffset, RcVector4 texSize, PcVector4 pTileSize, float uOffset, float vOffset)
{
	Transform3 m = Transform3::identity();
	m[0][0] = m[3][0] = m[3][1] = 0.5f;
	m[1][1] = -0.5f;
	m[3][2] = zOffset;

	if (pTileSize)
		m = VMath::prependScale(Vector3(
			texSize.getX() * pTileSize->getZ(),
			texSize.getY() * pTileSize->getW(), 1), m);

	if (uOffset || vOffset)
	{
		PcVector4 pTexSize = pTileSize ? pTileSize : &texSize;
		const Vector4 uvOffset(uOffset * pTexSize->getZ(), vOffset * pTexSize->getW());
		m = Transform3::translation(uvOffset.getXYZ()) * m;
	}

	return m;
}

float Math::Reduce(float val, float reduction)
{
	if (abs(val) < abs(reduction))
		return 0;
	else
		return val - glm::sign(val) * reduction;
}

Point3 Math::ClosestPointOnSegment(RcPoint3 segA, RcPoint3 segB, RcPoint3 point)
{
	const float lenSq = VMath::distSqr(segA, segB);
	if (lenSq < VERUS_FLOAT_THRESHOLD)
		return segA;
	const float t = Math::Clamp<float>(VMath::dot(point - segA, segB - segA) / lenSq, 0, 1);
	return VMath::lerp(t, segA, segB);
}

float Math::SegmentToPointDistance(RcPoint3 segA, RcPoint3 segB, RcPoint3 point)
{
	return VMath::dist(point, ClosestPointOnSegment(segA, segB, point));
}

void Math::Test()
{
	const float e = 1e-6f;
	const glm::vec3 eps(0.1f, 0.1f, 0.1f);
	const glm::vec4 eps4(0.1f, 0.1f, 0.1f, 0.1f);

	{
		Vector3 zeroV3(0), nonZeroV3(2, 3, 4);
		Vector4 zeroV4(0), nonZeroV4(3, 4, 5, 6);
		Point3 zeroP3(0), nonZeroP3(4, 3, 2);
		VERUS_RT_ASSERT(zeroV3.IsZero() && !nonZeroV3.IsZero());
		VERUS_RT_ASSERT(zeroV4.IsZero() && !nonZeroV4.IsZero());
		VERUS_RT_ASSERT(zeroP3.IsZero() && !nonZeroP3.IsZero());
		VERUS_RT_ASSERT(nonZeroV3.IsEqual(nonZeroV3, 0.01f));
		VERUS_RT_ASSERT(nonZeroP3.IsEqual(nonZeroP3, 0.01f));
		nonZeroV3.FromString("6.7 7.8 8.9");
		nonZeroV4.FromString("6.7 7.8 8.9 9.0");
		nonZeroP3.FromString("6.7 7.8 8.9");
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(nonZeroV3.GLM(), glm::vec3(6.7f, 7.8f, 8.9f), eps)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(nonZeroV4.GLM(), glm::vec4(6.7f, 7.8f, 8.9f, 9.0f), eps4)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(nonZeroP3.GLM(), glm::vec3(6.7f, 7.8f, 8.9f), eps)));

		Vector3 euler(1, 2, 3), a(0, 0, 1), b(0, 0, 1);
		Quat q;
		euler.EulerToQuaternion(q);
		a = Matrix3(q) * a;
		euler = Vector3(0);
		euler.EulerFromQuaternion(q);
		euler.EulerToQuaternion(q);
		b = Matrix3(q) * b;
		const float d = VMath::dot(a, b);
		VERUS_RT_ASSERT(d > 0.95f);
	}

	{
		const Vector3 a(0, 0, 1), b(0, 0, -1), c(0, 1, 0);
		Matrix3 ab, ac;
		ab.TrackTo(a, b);
		ac.TrackTo(a, c);
		const Vector3 k = ab * a;
		const Vector3 l = ac * a;
		VERUS_RT_ASSERT(
			VMath::dot(b, k) > 0.95f &&
			VMath::dot(c, l) > 0.95f);
	}

	// A test to show that front face is considered CCW (in OpenGL RH).
	{
		const Point3 a(0, 0, 0), b(0, 0, 1), c(1, 0, 0);
		const Vector3 n = TriangleNormal(a, b, c);
		VERUS_RT_ASSERT(n.getY() > 0.9f);
	}

	{
		glm::vec3 a(0, 0, 0), b(0, 3, 0), c(5, 0, 0);
		const float area = TriangleArea(a, b, c);
		VERUS_RT_ASSERT(glm::epsilonEqual(area, 7.5f, e));
	}

	{
		const Vector3 a(1, 0, 0), b(0, 1, 0);
		const float d = VMath::dot(a, b);
		const Vector3 c = VMath::cross(a, b);
		VERUS_RT_ASSERT(glm::epsilonEqual(d, 0.f, e));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(c.GLM(), glm::vec3(0, 0, 1), eps)));
	}

	{
		Math::Plane p(Point3(0, -1, 0), Point3(0, -1, 1), Point3(1, -1, 0)); // All points are 1 meter below ground.
		const float d = p.DistanceTo(Point3(0, -0.75f, 0)); // Test point is 0.75 meters below ground.
		VERUS_RT_ASSERT(glm::epsilonEqual(d, 0.25f, e)); // Should be 0.25 meters.

		Point3 p0;
		float t = 0;
		const bool ret = p.IntersectSegment(Point3(16, 9, 16), Point3(-16, -31, -16), p0, &t);
		VERUS_RT_ASSERT(ret && glm::all(glm::epsilonEqual(p0.GLM(), glm::vec3(8, -1, 8), eps)));
	}

	{
		const Math::Sphere a(Point3(0, 0, 0), 0.5f), b(Point3(1, 0, 0), 0.5f), c(Point3(2, 0, 0), 0.75f);
		const bool ab = a.IsOverlappingWith(b);
		const bool bc = b.IsOverlappingWith(c);
		const bool ca = c.IsOverlappingWith(a);
		VERUS_RT_ASSERT(!ab && bc && !ca && c.IsInside(Point3(2.1f, 0, 0)) && !c.IsInside(Point3(2.8f, 0, 0)));
	}

	{
		const Math::Bounds a(Point3(0, 0, 0), Point3(1, 1, 1)), b(Point3(1, 0, 0), Point3(2, 1, 1));
		VERUS_RT_ASSERT(!a.IsOverlappingWith(b));
		Math::Bounds c(Point3(1, 0, 0), Point3(4, 4, 4));
		VERUS_RT_ASSERT(!c.IsOverlappingWith(a) && c.IsOverlappingWith(b));

		c.FattenBy(0.5f);
		const Vector3 ext = c.GetExtents();
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(ext.GLM(), glm::vec3(2, 2.5f, 2.5f), eps)));

		const Point3 center = c.GetCenter();
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(center.GLM(), glm::vec3(2.5f, 2, 2), eps)));

		const Math::Bounds foo(Point3(1, 2, 3), Point3(7, 8, 9));
		Point3 corners[8];
		foo.GetCorners(corners);
		Matrix4 mc[2];
		foo.GetCorners(mc);
		mc[0] = VMath::transpose(mc[0]);
		mc[1] = VMath::transpose(mc[1]);
		VERUS_FOR(i, 8)
		{
			const int mi = (i >> 2) & 0x1;
			const int ci = i & 0x3;
			VERUS_RT_ASSERT(!memcmp(&mc[mi][ci], &corners[i], 12));
		}
	}

	{
		Transform3 tr = Transform3::translation(Vector3(2, 0, 0));
		tr = VMath::appendScale(tr, Vector3(16, 16, 16)); // Scale and then move.
		Vector3 t = tr.getTranslation();
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(t.GLM(), glm::vec3(2, 0, 0), eps)));
		tr = VMath::prependScale(Vector3(16, 16, 16), tr); // Move and then scale (not recommended).
		t = tr.getTranslation();
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(t.GLM(), glm::vec3(32, 0, 0), eps)));
	}

	{
		Transform3 tr = Transform3(Matrix3::rotationZYX(Vector3(1, 2, 3)), Vector3(7, 8, 9));
		Transform3 trS = VMath::appendScale(tr, Vector3(4, 5, 6));

		const Vector3 s = trS.GetScale();
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(s.GLM(), glm::vec3(4, 5, 6), eps)));

		trS = trS.Normalize(false);
		const Point3 p0 = tr * Point3(5, 6, 7);
		const Point3 p1 = trS * Point3(5, 6, 7);
		VERUS_RT_ASSERT(VMath::distSqr(p0, p1) < 0.0001f);

		const String txt = tr.ToString();
		trS.FromString(_C(txt));
		const Point3 p2 = tr * Point3(5, 6, 7);
		const Point3 p3 = trS * Point3(5, 6, 7);
		VERUS_RT_ASSERT(VMath::distSqr(p2, p3) < 0.0001f);

		const Matrix3 orth = Matrix3::scale(Vector3(1.00001f, 1.00002f, 1.00003f));
		VERUS_RT_ASSERT(orth.IsOrthogonal());
	}

	{
		const Point3 v0(-1, 1), v1(1, -1);
		const Vector4 tile(8, 32, 1 / 8.f, 1 / 32.f);

		Transform3 mT = ToUVMatrix(0, Vector4(512, 256, 1 / 512.f, 1 / 256.f));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mT * v0).GLM(), glm::vec3(0, 0, 0), eps)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mT * v1).GLM(), glm::vec3(1, 1, 0), eps)));
		mT = ToUVMatrix(VERUS_PI, Vector4(512, 256, 1 / 512.f, 1 / 256.f), &tile);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mT * v0).GLM(), glm::vec3(0, 0, VERUS_PI), eps)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mT * v1).GLM(), glm::vec3(64, 8, VERUS_PI), eps)));

		Transform3 mV = QuadMatrix(0, 0, 1, 1);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mV * v0).GLM(), glm::vec3(-1, 1, 0), eps)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mV * v1).GLM(), glm::vec3(1, -1, 0), eps)));
		mV = QuadMatrix(0.25f, 0.75f, 0.5f, 0.1f);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mV * v0).GLM(), glm::vec3(-0.5f, -0.5f, 0), eps)));
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(Point3(mV * v1).GLM(), glm::vec3(0.5f, -0.7f, 0), eps)));
	}

	{
		const Point3 a(0, 0, 0), b(0, 0, 1), c(1, 0, 0);
		Point3 p(0.1f, 0, 0.25f);
		const Vector3 bc = Barycentric(a, b, c, p);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(bc.GLM(), glm::vec3(0.25f, 0.1f, 0), eps)));
		VERUS_RT_ASSERT(IsPointInsideBarycentric(bc));
	}

	{
		Vector<UINT16> v;
		CreateStripGrid(2, 2, v);
		VERUS_RT_ASSERT(v.size() == 14);
		VERUS_RT_ASSERT(0 == v[0] && 3 == v[1] && 1 == v[2]); // First triangle is CCW.
	}

	{
		const Point3 a(1, 0, 0), b(3, 0, 0), p(1.25f, 3, 0), p2(10, 10, 0);
		Point3 res = ClosestPointOnSegment(a, b, p);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(res.GLM(), glm::vec3(1.25f, 0, 0), eps)));
		Point3 res2 = ClosestPointOnSegment(a, b, p2);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(res2.GLM(), glm::vec3(3, 0, 0), eps)));
	}

	{
		// Note that yaw is calculated like this: atan2(frontDir.x, frontDir.z). This way atan2(0, 1) will return 0.
		StringStream ss;
		for (float y = -2.f; y < 2.f; ++y)
			for (float x = -2.f; x < 2.f; ++x)
			{
				ss << "atan2(" << y << ", " << x << ") == " << atan2(y, x) << VERUS_CRNL;
			}
		const String s = ss.str();
	}

	{
		StringStream ss;
		for (float i = -10.f; i < 10.f; ++i)
		{
			ss << "WrapAngle(" << i << ") == " << WrapAngle(i) << VERUS_CRNL;
		}
		const String s = ss.str();
	}

	{
		const float d = 0.75f;
		Vector3 v(1, 0, 0);
		Vector3 x = VMath::normalize(Vector3(-1, 1, 0));
		x.LimitDot(v, d);
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(VMath::length(x), 1, e));
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(VMath::dot(x, v), d, e));
	}

	{
		// Right-handed mode (default):
		World::Camera camera;
		camera.MoveEyeTo(Point3(0, 0, 3));
		camera.MoveAtTo(Point3(0, 0, 4));
		camera.SetYFov(VERUS_PI * 0.25f);
		camera.SetAspectRatio(1);
		camera.SetZNear(0.5f);
		camera.SetZFar(100);
		camera.Update();
		Vector4 point = camera.GetMatrixVP() * Point3(3.5f, 0, 3.5f);
		point /= point.getW();
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(point.getZ(), 0.f, e));
		point = camera.GetMatrixVP() * Point3(103, 0, 103);
		point /= point.getW();
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(point.getZ(), 1.f, e));

		// Left-handed mode:
		camera.SetYFov(VERUS_PI * -0.25f);
		camera.Update();
		point = camera.GetMatrixVP() * Point3(2.5f, 0, 2.5f);
		point /= point.getW();
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(point.getZ(), 0.f, e));
		point = camera.GetMatrixVP() * Point3(-97, 0, -97);
		point /= point.getW();
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(point.getZ(), 1.f, e));
	}

	{
		const Quat q(0);
		const Matrix3 matR(q);
		const Vector3 upDir = matR.getCol1();
		const Vector3 frontDir = matR.getCol2();
		const Vector3 sideDir = VMath::cross(upDir, frontDir);
		VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(sideDir.GLM(), glm::vec3(1, 0, 0), eps)));
	}

	// Z VS W:
	{
		const float zn = 3;
		const float zf = 20;
		const Point3 p1(0, 0, 7);
		const Matrix4 matV = Matrix4::lookAt(Point3(0, 0, 2), p1, Vector3(0, 1, 0)); // 5 meters away from p1.
		const Matrix4 matP = Matrix4::MakePerspective(VERUS_PI * 0.25f, 4 / 3.f, zn, zf);
		const Matrix4 matVP = matP * matV;
		const Vector4 p2 = matV * p1;
		const Vector4 p3 = matVP * p1;
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(p2.getZ(), -5.f, e)); // In V space Z is negative.
		VERUS_RT_ASSERT(glm::epsilonEqual<float>(p3.getW(), 5.f, e)); // In VP space Z turns to positive W.
		const float unormDepth = p3.getZ() / p3.getW();
		const float a = zf * zn / (zn - zf);
		const float b = zf / (zf - zn);
		const float linearDepth = a / (unormDepth - b);
		VERUS_RT_ASSERT(glm::epsilonEqual(linearDepth, 5.f, e)); // Linear depth starts from the eye.
	}
}
