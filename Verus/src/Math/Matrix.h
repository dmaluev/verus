// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Matrix3 : public VMath::Matrix3
	{
	public:
		Matrix3() {}
		Matrix3(
			const VMath::Vector3& col0,
			const VMath::Vector3& col1,
			const VMath::Vector3& col2) : VMath::Matrix3(col0, col1, col2) {}
		Matrix3(const VMath::Quat& q) : VMath::Matrix3(q) {}
		Matrix3(const VMath::Matrix3& that) : VMath::Matrix3(that) {}
		Matrix3& operator=(const VMath::Matrix3& that);

		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		bool IsOrthogonal(float e = VERUS_FLOAT_THRESHOLD) const;

		static Matrix3 MakeAimZ(RcVector3 zAxis, PcVector3 pUp = nullptr);
		Matrix3            AimZ(RcVector3 zAxis, PcVector3 pUp = nullptr);
		static Matrix3 MakeRotateTo(RcVector3 v0, RcVector3 v1);
		Matrix3            RotateTo(RcVector3 v0, RcVector3 v1);

		static Matrix3 Lerp(const Matrix3& a, const Matrix3& b, float t);
	};
	VERUS_TYPEDEFS(Matrix3);

	class Matrix4 : public VMath::Matrix4
	{
	public:
		Matrix4() {}
		Matrix4(
			const VMath::Vector4& col0,
			const VMath::Vector4& col1,
			const VMath::Vector4& col2,
			const VMath::Vector4& col3) : VMath::Matrix4(col0, col1, col2, col3) {}
		Matrix4(const VMath::Transform3& that) : VMath::Matrix4(that) {}
		Matrix4(const VMath::Matrix4& that) : VMath::Matrix4(that) {}
		Matrix4& operator=(const VMath::Matrix4& that);

		Matrix4(const glm::mat4& that);
		glm::mat4 GLM() const;

		matrix UniformBufferFormat() const;
		static matrix UniformBufferFormatIdentity();
		void InstFormat(VMath::Vector4* p) const;

		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		static Matrix4 MakePerspective(float fovY, float aspectRatio, float zNear, float zFar, bool rightHanded = true);
		static Matrix4 MakeOrtho(float w, float h, float zNear, float zFar);
	};
	VERUS_TYPEDEFS(Matrix4);

	class Transform3 : public VMath::Transform3
	{
	public:
		Transform3() {}
		Transform3(
			const VMath::Vector3& col0,
			const VMath::Vector3& col1,
			const VMath::Vector3& col2,
			const VMath::Vector3& col3) : VMath::Transform3(col0, col1, col2, col3) {}
		Transform3(const VMath::Matrix3& that, const VMath::Vector3& v) : VMath::Transform3(that, v) {}
		Transform3(const VMath::Quat& q, const VMath::Vector3& v) : VMath::Transform3(q, v) {}
		Transform3(const VMath::Transform3& that) : VMath::Transform3(that) {}
		Transform3(const VMath::Matrix4& that) : VMath::Transform3(that.getUpper3x3(), that.getTranslation()) {}
		Transform3& operator=(const VMath::Transform3& that);

		Transform3(const btTransform& tr);
		btTransform Bullet() const;

		Transform3(const glm::mat4& that);
		Transform3(const glm::mat4x3& that);
		glm::mat4 GLM() const;
		glm::mat4x3 GLM4x3() const;

		mataff UniformBufferFormat() const;
		static mataff UniformBufferFormatIdentity();
		void InstFormat(VMath::Vector4* p) const;

		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		bool IsIdentity();

		Transform3 Normalize(bool scalePos = false);
		Vector3 GetScale() const;

		String ToString() const;
		Transform3& FromString(CSZ sz);
	};
	VERUS_TYPEDEFS(Transform3);
}
