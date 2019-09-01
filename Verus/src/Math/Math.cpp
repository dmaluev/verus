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

int Math::GetNumStripGridIndices(int widthPoly, int heightPoly)
{
	const int widthVert = widthPoly + 1;
	return (widthVert * 2)*heightPoly + (heightPoly - 1) * 2; // Strip and primitive restart values.
}

void Math::BuildStripGrid(int widthPoly, int heightPoly, Vector<UINT16>& vIndices)
{
	vIndices.resize(GetNumStripGridIndices(widthPoly, heightPoly));
	int offset = 0;
	const int widthVert = widthPoly + 1;
	VERUS_FOR(h, heightPoly)
	{
		if (h > 0) // Primitive restart value:
		{
			vIndices[offset++] = 0xFFFF;
			vIndices[offset++] = 0xFFFF;
		}
		if (h & 0x1)
		{
			for (int w = widthPoly; w >= 0; --w)
			{
				vIndices[offset++] = widthVert * (h + 1) + w;
				vIndices[offset++] = widthVert * (h + 0) + w;
			}
		}
		else
		{
			VERUS_FOR(w, widthVert)
			{
				vIndices[offset++] = widthVert * (h + 0) + w;
				vIndices[offset++] = widthVert * (h + 1) + w;
			}
		}
	}
}

void Math::BuildListGrid(int widthPoly, int heightPoly, Vector<UINT16>& vIndices)
{
	vIndices.resize(widthPoly*heightPoly * 6);
	int offset = 0;
	const int widthVert = widthPoly + 1;
	VERUS_FOR(h, heightPoly)
	{
		if (h & 0x1)
		{
			for (int w = widthPoly - 1; w >= 0; --w)
			{
				vIndices[offset++] = widthVert * (h + 1) + w + 1;
				vIndices[offset++] = widthVert * (h + 0) + w + 1;
				vIndices[offset++] = widthVert * (h + 1) + w;
				vIndices[offset++] = widthVert * (h + 1) + w;
				vIndices[offset++] = widthVert * (h + 0) + w + 1;
				vIndices[offset++] = widthVert * (h + 0) + w;
			}
		}
		else
		{
			VERUS_FOR(w, widthPoly)
			{
				vIndices[offset++] = widthVert * (h + 0) + w;
				vIndices[offset++] = widthVert * (h + 1) + w;
				vIndices[offset++] = widthVert * (h + 0) + w + 1;
				vIndices[offset++] = widthVert * (h + 0) + w + 1;
				vIndices[offset++] = widthVert * (h + 1) + w;
				vIndices[offset++] = widthVert * (h + 1) + w + 1;
			}
		}
	}
}

Transform3 Math::BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx)
{
	const Vector3 d = mx - mn;
	const Point3 c = VMath::lerp(0.5f, mn, mx);
	return VMath::appendScale(Transform3::translation(c - Point3(0, d.getY()*0.5f, 0)), d);
}

float Math::ComputeOnePixelDistance(float size, float screenHeight, float fovY)
{
	const float viewSliceSize = size * screenHeight;
	return viewSliceSize / (tan(fovY*0.5f) * 2);
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
