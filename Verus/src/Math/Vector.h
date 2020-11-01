// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Quat;

	class Vector3 : public VMath::Vector3
	{
	public:
		Vector3() {}
		Vector3(float x, float y = 0, float z = 0) : VMath::Vector3(x, y, z) {}
		Vector3(
			const VMath::FloatInVec& x,
			const VMath::FloatInVec& y = VMath::FloatInVec(0),
			const VMath::FloatInVec& z = VMath::FloatInVec(0)) : VMath::Vector3(x, y, z) {}
		Vector3(const VMath::Point3& that) : VMath::Vector3(that) {}
		Vector3(const VMath::Vector3& that) : VMath::Vector3(that) {}
		Vector3& operator=(const VMath::Vector3& that) { VMath::Vector3::operator=(that); return *this; }

		// Bullet:
		Vector3(const btVector3& that) : VMath::Vector3(that.x(), that.y(), that.z()) {}
		btVector3 Bullet() const { return btVector3(getX(), getY(), getZ()); }

		// GLM (vec3 & vec2):
		static Vector3 MakeXZ(const glm::vec2& that) { return VMath::Vector3(that.x, 0, that.y); }
		Vector3(const glm::vec2& that) : VMath::Vector3(that.x, that.y, 0) {}
		Vector3(const glm::vec3& that) : VMath::Vector3(that.x, that.y, that.z) {}
		glm::vec3 GLM() const { return glm::vec3(getX(), getY(), getZ()); }
		glm::vec2 GLM2() const { return glm::vec2(getX(), getY()); }

		static Vector3 Replicate(float x) { return VMath::Vector3(x); }

		// Pointer:
		static Vector3 MakeFromPointer(const float* p);
		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		// Compare:
		bool IsZero() const;
		bool IsEqual(const Vector3& that, float e) const;
		bool IsLessThan(const Vector3& that, float e) const;

		// Math:
		Vector3 Clamp(float mn, float mx) const;
		Vector3 Clamp(const Vector3& mn, const Vector3& mx) const;
		Vector3 Floor() const;
		Vector3 Pow(float p) const;
		Vector3 Mod(float x) const;
		Vector3 Mod(const Vector3& that) const;

		// String:
		String ToString(bool compact = false) const;
		String ToString2(bool compact = false) const;
		Vector3& FromString(CSZ sz);

		void EulerFromQuaternion(const Quat& q);
		void EulerToQuaternion(Quat& q) const;

		void LimitDot(const Vector3& v, float d);
		Vector3 Reflect(const Vector3& normal, float bounce = 1) const;
	};
	VERUS_TYPEDEFS(Vector3);

	class Vector4 : public VMath::Vector4
	{
	public:
		Vector4() {}
		Vector4(float x, float y = 0, float z = 0, float w = 0) : VMath::Vector4(x, y, z, w) {}
		Vector4(
			const VMath::FloatInVec& x,
			const VMath::FloatInVec& y = VMath::FloatInVec(0),
			const VMath::FloatInVec& z = VMath::FloatInVec(0),
			const VMath::FloatInVec& w = VMath::FloatInVec(0)) : VMath::Vector4(x, y, z, w) {}
		Vector4(const VMath::Vector3& that, float w) : VMath::Vector4(that, w) {}
		Vector4(const VMath::Vector3& that, const VMath::FloatInVec& w) : VMath::Vector4(that, w) {}
		Vector4(const VMath::Vector4& that) : VMath::Vector4(that) {}
		Vector4& operator=(const VMath::Vector4& that) { VMath::Vector4::operator=(that); return *this; }

		// Bullet:
		Vector4(const btVector3& that) : VMath::Vector4(that.x(), that.y(), that.z(), that.w()) {}
		btVector3 Bullet() const { return btVector3(getX(), getY(), getZ()); }

		// GLM (vec4):
		static Vector4 MakeXZ(const glm::vec2& that) { return VMath::Vector4(that.x, 0, that.y, 0); }
		Vector4(const glm::vec2& that) : VMath::Vector4(that.x, that.y, 0, 0) {}
		Vector4(const glm::vec3& that) : VMath::Vector4(that.x, that.y, that.z, 0) {}
		Vector4(const glm::vec4& that) : VMath::Vector4(that.x, that.y, that.z, that.w) {}
		glm::vec4 GLM() const { return glm::vec4(getX(), getY(), getZ(), getW()); }

		static Vector4 Replicate(float x) { return VMath::Vector4(x); }

		// Pointer:
		static Vector4 MakeFromPointer(const float* p);
		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		// Compare:
		bool IsZero() const;

		// Math:
		Vector4 Clamp(float mn, float mx) const;
		Vector4 Clamp(const Vector4& mn, const Vector4& mx) const;
		Vector4 Mod(float x) const;
		Vector4 Mod(const Vector4& that) const;

		// String:
		String ToString(bool compact = false) const;
		Vector4& FromString(CSZ sz);

		// Color:
		static Vector4 MakeFromColor(UINT32 color, bool sRGB = true);
		Vector4& FromColor(UINT32 color, bool sRGB = true);
		UINT32 ToColor(bool sRGB = true) const;
		String ToColorString(bool sRGB = true);
		Vector4& FromColorString(CSZ sz, bool sRGB = true);

		// Rectangle:
		static Vector4 MakeRectangle(float l, float t, float r, float b);
		float Left() const;
		float Top() const;
		float Width() const;
		float Height() const;
		float Right() const;
		float Bottom() const;
		bool IsInsideRect(float x, float y) const;

		Vector4 ToGuiCoord() const;
	};
	VERUS_TYPEDEFS(Vector4);

	class Point3 : public VMath::Point3
	{
	public:
		Point3() {}
		Point3(float x, float y = 0, float z = 0) : VMath::Point3(x, y, z) {}
		Point3(
			const VMath::FloatInVec& x,
			const VMath::FloatInVec& y = VMath::FloatInVec(0),
			const VMath::FloatInVec& z = VMath::FloatInVec(0)) : VMath::Point3(x, y, z) {}
		Point3(const VMath::Vector3& that) : VMath::Point3(that) {}
		Point3(const VMath::Point3& that) : VMath::Point3(that) {}
		Point3& operator=(const VMath::Point3& that) { VMath::Point3::operator=(that); return *this; }

		// Bullet:
		Point3(const btVector3& that) : VMath::Point3(that.x(), that.y(), that.z()) {}
		btVector3 Bullet() const { return btVector3(getX(), getY(), getZ()); }

		// GLM (vec3 & vec2):
		static Point3 MakeXZ(const glm::vec2& that) { return VMath::Point3(that.x, 0, that.y); }
		Point3(const glm::vec2& that) : VMath::Point3(that.x, that.y, 0) {}
		Point3(const glm::vec3& that) : VMath::Point3(that.x, that.y, that.z) {}
		glm::vec3 GLM() const { return glm::vec3(getX(), getY(), getZ()); }
		glm::vec2 GLM2() const { return glm::vec2(getX(), getY()); }

		static Point3 Replicate(float x) { return VMath::Point3(x); }

		// Pointer:
		static Point3 MakeFromPointer(const float* p);
		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }
		void ToArray3(float* p) const;

		// Compare:
		bool IsZero() const;
		bool IsEqual(const Point3& that, float e) const;
		bool IsLessThan(const Point3& that, float e) const;

		// String:
		String ToString(bool compact = false) const;
		String ToString2(bool compact = false) const;
		Point3& FromString(CSZ sz);
	};
	VERUS_TYPEDEFS(Point3);
}
