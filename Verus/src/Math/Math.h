#pragma once

#define VERUS_PI  3.141592654f
#define VERUS_2PI 6.283185307f
#define VERUS_E   2.718281828f
#define VERUS_GR  1.618034f

#define VERUS_FLOAT_THRESHOLD 1e-4f

namespace verus
{
	enum class Relation : int
	{
		outside,
		inside,
		intersect
	};
	enum class Continue : int
	{
		no,
		yes
	};

	typedef glm::vec2 float2;
	typedef glm::vec3 float3;
	typedef glm::vec4 float4;
	typedef glm::mat4 matrix;
	typedef glm::mat3x4 mataff;

	namespace VMath = Vectormath::SSE;
}

#include "Vector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Sphere.h"
#include "Bounds.h"
#include "Plane.h"
#include "Frustum.h"
#include "NormalComputer.h"
#include "QuadtreeIntegral.h"
#include "Quadtree.h"
#include "Octree.h"

namespace verus
{
	namespace Math
	{
		// Bits:
		bool IsPowerOfTwo(int x);
		int HighestBit(int x);
		bool IsNaN(float x);

		// Angles:
		float ToRadians(float deg);
		float ToDegrees(float rad);
		float WrapAngle(float rad);

		// Interpolation, splines:
		float Lerp(float a, float b, float t);
		float SmoothStep(float a, float b, float t);
		float LinearToSin(float t);
		float LinearToCos(float t);

		// Shapes:
		Vector3 TriangleNormal(RcPoint3 a, RcPoint3 b, RcPoint3 c);
		float TriangleArea(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2);

		// Geometry:
		int GetNumStripGridIndices(int widthPoly, int heightPoly);
		void BuildStripGrid(int widthPoly, int heightPoly, Vector<UINT16>& vIndices);
		void BuildListGrid(int widthPoly, int heightPoly, Vector<UINT16>& vIndices);

		// Scene:
		Transform3 BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx);
		float ComputeOnePixelDistance(float size, float screenHeight = 120, float fovY = VERUS_PI / 4);
		void Quadrant(const int** ppSrcMinMax, int** ppDestMinMax, int half, int id);

		int ComputeMipLevels(int w, int h, int d = 1);

		// Matrices:
		Transform3 QuadMatrix(float x = 0, float y = 0, float w = 1, float h = 1);
		Transform3 ToUVMatrix(float zOffset = 0, RcVector4 texSize = Vector4(0), PcVector4 pTileSize = nullptr);

		// Clamp:
		template<typename T>
		T Clamp(T x, T mn, T mx)
		{
			if (x <= mn)
				return mn;
			else if (x >= mx)
				return mx;
			return x;
		}

		// Minimum & maximum:
		template<typename T>
		T Min(T a, T b)
		{
			return a < b ? a : b;
		}
		template<typename T>
		T Max(T a, T b)
		{
			return a > b ? a : b;
		}

		// Memory alignment:
		template<typename T>
		T AlignUp(T val, T align)
		{
			return (val + align - 1) / align * align;
		}
		template<typename T>
		T AlignDown(T val, T align)
		{
			return val / align * align;
		}
		template<typename T>
		T DivideByMultiple(T value, T alignment)
		{
			return (value + alignment - 1) / alignment;
		}
	};
}
