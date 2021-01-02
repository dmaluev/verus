// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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

	enum class Easing : int
	{
		none,

		sineIn,
		sineOut,
		sineInOut,

		quadIn,
		quadOut,
		quadInOut,

		cubicIn,
		cubicOut,
		cubicInOut,

		quartIn,
		quartOut,
		quartInOut,

		quintIn,
		quintOut,
		quintInOut,

		expoIn,
		expoOut,
		expoInOut,

		circIn,
		circOut,
		circInOut,

		backIn,
		backOut,
		backInOut,

		elasticIn,
		elasticOut,
		elasticInOut,

		bounceIn,
		bounceOut,
		bounceInOut,

		count
	};

	typedef glm::ivec2 int2;
	typedef glm::ivec3 int3;
	typedef glm::ivec4 int4;
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
		UINT32 NextPowerOfTwo(UINT32 x);
		int HighestBit(int x);
		int LowestBit(int x);
		bool IsNaN(float x);

		// Angles:
		float ToRadians(float deg);
		float ToDegrees(float rad);
		float WrapAngle(float rad);

		// Interpolation, splines:
		float Lerp(float a, float b, float t);
		float SmoothStep(float a, float b, float t);
		float ApplyEasing(Easing easing, float x);
		Easing EasingFromString(CSZ s);
		CSZ EasingToString(Easing easing);

		// Shapes:
		Vector3 TriangleNormal(RcPoint3 a, RcPoint3 b, RcPoint3 c);
		float TriangleArea(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2);

		// Geometry:
		int StripGridIndexCount(int polyCountWidth, int polyCountHeight);
		void CreateStripGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices);
		void CreateListGrid(int polyCountWidth, int polyCountHeight, Vector<UINT16>& vIndices);
		bool CheckIndexBuffer(Vector<UINT16>& vIndices, int maxIndex);

		// Scene:
		Transform3 BoundsDrawMatrix(RcPoint3 mn, RcPoint3 mx);
		float ComputeOnePixelDistance(float objectSize, float viewportHeightInPixels = 135, float fovY = VERUS_PI / 4);
		float ComputeDistToMipScale(float texHeight, float viewportHeightInPixels, float objectSize, float fovY);
		void Quadrant(const int** ppSrcMinMax, int** ppDestMinMax, int half, int id);

		int ComputeMipLevels(int w, int h, int d = 1);

		// Matrices:
		Transform3 QuadMatrix(float x = 0, float y = 0, float w = 1, float h = 1);
		Transform3 ToUVMatrix(float zOffset = 0, RcVector4 texSize = Vector4(0), PcVector4 pTileSize = nullptr, float uOffset = 0, float vOffset = 0);

		float Reduce(float val, float reduction);

		void Test();
	};
}
