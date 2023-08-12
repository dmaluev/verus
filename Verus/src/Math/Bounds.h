// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Math
{
	class Bounds
	{
		Point3 _min;
		Point3 _max;

	public:
		Bounds();
		Bounds(RcPoint3 mn, RcPoint3 mx);
		~Bounds();

		RcPoint3 GetMin() const { return _min; }
		RcPoint3 GetMax() const { return _max; }
		bool IsNull() const;

		Point3 GetCenter()      const { return VMath::lerp(0.5f, _min, _max); }
		Vector3 GetDimensions() const { return (_max - _min); }
		Vector3 GetExtents()    const { return (_max - _min) * 0.5f; }

		Bounds& Set(RcPoint3 mn, RcPoint3 mx);
		Bounds& Set(float mn, float mx, int axis);
		Bounds& Set(const Bounds& that, int axis);

		Bounds& Reset();
		Bounds& Include(RcPoint3 point);
		Bounds& CombineWith(const Bounds& that);
		Bounds& MoveBy(RcVector3 offset);

		static Bounds MakeFromCenterExtents(RcPoint3 center, RcVector3 extents);
		static Bounds MakeFromOrientedBox(const Bounds& that, RcTransform3 tr);

		Bounds& ScaleBy(float scale);
		Bounds& FattenBy(float d);

		bool IsInside(RcPoint3 point) const;
		bool IsInside2D(RcPoint3 point) const;
		bool IsOverlappingWith(const Bounds& that) const;
		bool IsOverlappingWith2D(const Bounds& that, int axis) const;

		void GetCorners(PPoint3 points) const;
		void GetCorners(PMatrix4 m) const;
		Sphere GetSphere() const;
		Matrix4 GetMatrix() const;
		Transform3 GetDrawTransform() const;
		Transform3 GetBoxTransform() const;

		Bounds& MirrorY();
		Bounds& Wrap();

		void GetQuadrant2D(int id, Bounds& that) const;
		void GetQuadrant3D(int id, Bounds& that) const;

		float GetDiagonal() const;
		float GetAverageSize() const;
		float GetMaxSide() const;
		Vector3 GetExtentsFromOrigin(RcVector3 scale = Vector3::Replicate(1)) const;
		float GetMaxExtentFromOrigin(RcVector3 scale = Vector3::Replicate(1)) const;

		Bounds& ToUnitBounds();
	};
	VERUS_TYPEDEFS(Bounds);
}
