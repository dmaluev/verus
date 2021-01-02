// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

// Matrix3:

Matrix3& Matrix3::operator=(const VMath::Matrix3& that)
{
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	const float* p = reinterpret_cast<const float*>(&that);
	VERUS_FOR(i, 12)
	{
		VERUS_RT_ASSERT(!Math::IsNaN(p[i]));
	}
#endif
	VMath::Matrix3::operator=(that);
	return *this;
}

bool Matrix3::IsOrthogonal(float e) const
{
	const Vector3 x = getCol0();
	const Vector3 y = getCol1();
	const Vector3 z = getCol2();
	const float lx = VMath::length(x);
	const float ly = VMath::length(y);
	const float lz = VMath::length(z);
	const float dotXY = VMath::dot(x, y);
	const float dotYZ = VMath::dot(y, z);
	const float dotZX = VMath::dot(z, x);
	return
		glm::epsilonEqual<float>(lx, 1, e) &&
		glm::epsilonEqual<float>(ly, 1, e) &&
		glm::epsilonEqual<float>(lz, 1, e) &&
		glm::epsilonEqual<float>(dotXY, 0, e) &&
		glm::epsilonEqual<float>(dotYZ, 0, e) &&
		glm::epsilonEqual<float>(dotZX, 0, e);
}

Matrix3 Matrix3::MakeAimZ(RcVector3 zAxis, PcVector3 pUp)
{
	if (zAxis.IsZero())
		return Matrix3::identity();
	const Vector3 zNorm = VMath::normalizeApprox(zAxis);
	if (zNorm.getY() > 0.999f)
		return Matrix3::rotationX(-VERUS_PI * 0.5f);
	if (zNorm.getY() < -0.999f)
		return Matrix3::rotationX(VERUS_PI * 0.5f);
	return VMath::transpose(
		Matrix4::lookAt(Point3(0), Point3(-zAxis), pUp ? *pUp : Vector3(0, 1, 0)).getUpper3x3());
}

Matrix3 Matrix3::AimZ(RcVector3 zAxis, PcVector3 pUp)
{
	*this = MakeAimZ(zAxis, pUp);
	return *this;
}

Matrix3 Matrix3::MakeRotateTo(RcVector3 v0, RcVector3 v1)
{
	const float e = 1e-6f;
	Quat q;
	const float d = VMath::dot(v0, v1);
	if (d >= 1 - e)
	{
		return Matrix3::identity();
	}
	if (d < e - 1)
	{
		Vector3 axis = VMath::cross(Vector3(1, 0, 0), v0);
		if (VMath::lengthSqr(axis) < e * e)
			axis = VMath::cross(Vector3(0, 1, 0), v0);
		axis = VMath::normalize(axis);
		q = Quat::rotation(VERUS_PI, axis);
	}
	else
	{
		const float s = sqrt((1 + d) * 2);
		const float invs = 1 / s;
		const Vector3 c = VMath::cross(v0, v1);
		q = Quat(c * invs, s * 0.5f);
		q = VMath::normalize(q);
	}
	return Matrix3(q);
}

Matrix3 Matrix3::RotateTo(RcVector3 v0, RcVector3 v1)
{
	*this = MakeRotateTo(v0, v1);
	return *this;
}

Matrix3 Matrix3::Lerp(RcMatrix3 a, RcMatrix3 b, float t)
{
	Matrix3 m;
	m.setCol0(VMath::lerp(t, a.getCol0(), b.getCol0()));
	m.setCol1(VMath::lerp(t, a.getCol1(), b.getCol1()));
	m.setCol2(VMath::lerp(t, a.getCol2(), b.getCol2()));
	return m;
}

// Matrix4:

RMatrix4 Matrix4::operator=(const VMath::Matrix4& that)
{
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	const float* p = reinterpret_cast<const float*>(&that);
	VERUS_FOR(i, 16)
	{
		VERUS_RT_ASSERT(!Math::IsNaN(p[i]));
	}
#endif
	VMath::Matrix4::operator=(that);
	return *this;
}

Matrix4::Matrix4(const glm::mat4& that)
{
	memcpy(this, &that, sizeof(*this));
}

glm::mat4 Matrix4::GLM() const
{
	glm::mat4 m;
	memcpy(&m, this, sizeof(m));
	return m;
}

matrix Matrix4::UniformBufferFormat() const
{
	Matrix4 m2(*this);
	m2 = VMath::transpose(m2);
	glm::mat4 m;
	memcpy(&m, &m2, sizeof(m));
	return matrix(m);
}

matrix Matrix4::UniformBufferFormatIdentity()
{
	Matrix4 m2(Matrix4::identity());
	glm::mat4 m;
	memcpy(&m, &m2, sizeof(m));
	return matrix(m);
}

void Matrix4::InstFormat(VMath::Vector4* p) const
{
	const Matrix4 m = VMath::transpose(*this);
	memcpy(p, &m, 3 * sizeof(Vector4));
}

Matrix4 Matrix4::MakePerspective(float fovY, float aspectRatio, float zNear, float zFar, bool rightHanded)
{
	VERUS_RT_ASSERT(fovY);
	VERUS_RT_ASSERT(aspectRatio);
	VERUS_RT_ASSERT(zNear < zFar);
	Matrix4 m;
	memset(&m, 0, sizeof(m));
	const float yScale = 1 / tan(fovY * 0.5f);
	const float xScale = yScale / aspectRatio;
	if (rightHanded)
	{
		const float zScale = zFar / (zNear - zFar);
		m.setElem(0, 0, xScale);
		m.setElem(1, 1, yScale);
		m.setElem(2, 2, zScale);
		m.setElem(3, 2, zNear * zScale);
		m.setElem(2, 3, -1);
	}
	else
	{
		const float zScale = zFar / (zFar - zNear);
		m.setElem(0, 0, xScale);
		m.setElem(1, 1, yScale);
		m.setElem(2, 2, zScale);
		m.setElem(3, 2, -zNear * zScale);
		m.setElem(2, 3, 1);
	}
	return m;
}

Matrix4 Matrix4::MakeOrtho(float w, float h, float zNear, float zFar)
{
	VERUS_RT_ASSERT(w);
	VERUS_RT_ASSERT(h);
	VERUS_RT_ASSERT(zNear < zFar);
	Matrix4 m;
	const float zScale = 1 / (zNear - zFar);
	memset(&m, 0, sizeof(m));
	m.setElem(0, 0, 2 / w);
	m.setElem(1, 1, 2 / h);
	m.setElem(2, 2, zScale);
	m.setElem(3, 3, 1);
	m.setElem(3, 2, zNear * zScale);
	return m;
}

// Transform3:

RTransform3 Transform3::operator=(const VMath::Transform3& that)
{
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	const float* p = reinterpret_cast<const float*>(&that);
	VERUS_FOR(i, 16)
	{
		VERUS_RT_ASSERT(!Math::IsNaN(p[i]));
	}
#endif
	VMath::Transform3::operator=(that);
	return *this;
}

Transform3::Transform3(const btTransform& tr)
{
	tr.getOpenGLMatrix(reinterpret_cast<float*>(this));
}

btTransform Transform3::Bullet() const
{
	btTransform tr;
	tr.setFromOpenGLMatrix(ToPointer());
	return tr;
}

Transform3::Transform3(const glm::mat4& that)
{
	memcpy(this, &that, sizeof(*this));
}

Transform3::Transform3(const glm::mat4x3& that)
{
	*this = Transform3::identity();
	VERUS_FOR(i, 4)
		memcpy(&(*this)[i], &that[i], sizeof(float) * 3);
}

glm::mat4 Transform3::GLM() const
{
	glm::mat4 m;
	const Matrix4 m2(*this);
	memcpy(&m, &m2, sizeof(m));
	return m;
}

glm::mat4x3 Transform3::GLM4x3() const
{
	glm::mat4x3 m;
	VERUS_FOR(i, 4)
	{
		const auto v = (*this)[i];
		memcpy(&m[i], &v, sizeof(float) * 3);
	}
	return m;
}

mataff Transform3::UniformBufferFormat() const
{
	Matrix4 m2(*this);
	m2 = VMath::transpose(m2);
	glm::mat4 m;
	memcpy(&m, &m2, sizeof(m));
	return mataff(m);
}

mataff Transform3::UniformBufferFormatIdentity()
{
	Matrix4 m2(Matrix4::identity());
	glm::mat4 m;
	memcpy(&m, &m2, sizeof(m));
	return mataff(m);
}

void Transform3::InstFormat(VMath::Vector4* p) const
{
	Matrix4 m(*this);
	m = VMath::transpose(m);
	memcpy(p, &m, 3 * sizeof(Vector4));
}

bool Transform3::IsIdentity()
{
	const Matrix4 x(*this);
	const Matrix4 i = Matrix4::identity();
	return !memcmp(&x, &i, sizeof(x));
}

Transform3 Transform3::Normalize(bool scalePos)
{
	const float xl = VMath::length(getCol0());
	const float yl = VMath::length(getCol1());
	const float zl = VMath::length(getCol2());
	setCol0(getCol0() / xl);
	setCol1(getCol1() / yl);
	setCol2(getCol2() / zl);
	if (scalePos)
		setTranslation(VMath::divPerElem(getTranslation(), Vector3(xl, yl, zl)));
	return *this;
}

Vector3 Transform3::GetScale() const
{
	return Vector3(
		VMath::length(getCol0()),
		VMath::length(getCol1()),
		VMath::length(getCol2()));
}

String Transform3::ToString() const
{
	StringStream ss;
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 3)
		{
			ss << getElem(i, j);
			if (!(i == 3 && j == 2))
				ss << " ";
		}
	}
	return ss.str();
}

RTransform3 Transform3::FromString(CSZ sz)
{
	float* p = const_cast<float*>(ToPointer());
	sscanf(sz,
		"%f %f %f "
		"%f %f %f "
		"%f %f %f "
		"%f %f %f",
		p + 0, p + 1, p + 2,
		p + 4, p + 5, p + 6,
		p + 8, p + 9, p + 10,
		p + 12, p + 13, p + 14);
	return *this;
}
