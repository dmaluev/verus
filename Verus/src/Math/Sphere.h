#pragma once

namespace verus
{
	namespace Math
	{
		class Sphere
		{
			Vector4 _center_radius;

		public:
			Sphere() {}
			Sphere(RcPoint3 center, float r) : _center_radius(Vector3(center), r) {}

			Point3 GetCenter() const { return _center_radius.getXYZ(); }
			float GetRadius() const { return _center_radius.getW(); }
			float GetRadiusSq() const { return _center_radius.getW() * _center_radius.getW(); }

			Sphere& SetCenter(RcPoint3 center) { _center_radius.setXYZ(Vector3(center)); return *this; }
			Sphere& SetRadius(float r) { _center_radius.setW(r); return *this; }

			bool IsInside(RcPoint3 point) const;
			bool IsOverlappingWith(const Sphere& that) const;

			static void EvenlyDistPoints(int count, Vector<Point3>& vPoints);
		};
		VERUS_TYPEDEFS(Sphere);
	}
}
