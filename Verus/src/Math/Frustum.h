// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Math
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

		Matrix4 _matInvVP = Matrix4::identity();
		Plane   _planes[static_cast<int>(Name::count)];
		Point3  _corners[10];

	public:
		Frustum();
		~Frustum();

		static Frustum MakeFromMatrix(RcMatrix4 matVP);
		Frustum& FromMatrix(RcMatrix4 matVP);
		Relation ContainsSphere(RcSphere sphere) const;
		Relation ContainsAabb(RcBounds bounds) const;
		void Draw();

		RcPoint3 GetCorner(int index) const { return _corners[index]; }
		RcPoint3 GetZNearPosition() const { return _corners[8]; }
		RcPoint3 GetZFarPosition() const { return _corners[9]; }

		Bounds GetBounds(RcTransform3 tr, PPoint3 pFocusedCenterPos = nullptr) const;

		Frustum& SetNearPlane(RcPoint3 eye, RcVector3 front, float zNear);
		Frustum& SetFarPlane(RcPoint3 eye, RcVector3 front, float zFar);
	};
	VERUS_TYPEDEFS(Frustum);
}
