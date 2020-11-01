// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class Frustum
		{
			enum class Name : int
			{
				left,
				right,
				bottom,
				top,
				nearest,
				furthest,
				count
			};

			Matrix4 _matI = Matrix4::identity();
			Plane   _planes[static_cast<int>(Name::count)];
			Point3  _corners[10];

		public:
			Frustum();
			~Frustum();

			static Frustum MakeFromMatrix(RcMatrix4 m);
			Frustum& FromMatrix(RcMatrix4 m);
			Relation ContainsSphere(RcSphere sphere) const;
			Relation ContainsAabb(RcBounds aabb) const;
			void Draw();

			RcPoint3 GetEyePosition() const { return _corners[8]; }

			Vector4 GetBounds(RcMatrix4 m, float& zNear, float& zFar) const;

			Frustum& SetNearPlane(RcPoint3 eye, RcVector3 front, float zNear);
			Frustum& SetFarPlane(RcPoint3 eye, RcVector3 front, float zFar);
		};
		VERUS_TYPEDEFS(Frustum);
	}
}
