// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class Plane : public Vector4
		{
		public:
			Plane();
			Plane(RcVector4 that);
			Plane(RcVector3 normal, float d);
			Plane(RcVector3 normal, RcPoint3 point);
			Plane(RcPoint3 pointA, RcPoint3 pointB, RcPoint3 pointC);
			Plane& operator=(RcVector4 that) { Vector4::operator=(that); return *this; }
			~Plane();

			float DistanceTo(RcPoint3 point) const;

			Plane& Normalize();

			bool IntersectSegment(RcPoint3 pointA, RcPoint3 pointB, RPoint3 point, float* t = nullptr);
		};
		VERUS_TYPEDEFS(Plane);
	}
}
