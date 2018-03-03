#pragma once

namespace verus
{
	class Quat : public VMath::Quat
	{
	public:
		Quat() {}
		Quat(float x, float y = 0, float z = 0, float w = 1) : VMath::Quat(x, y, z, w) {}
		Quat(
			const VMath::FloatInVec& x,
			const VMath::FloatInVec& y = VMath::FloatInVec(0),
			const VMath::FloatInVec& z = VMath::FloatInVec(0),
			const VMath::FloatInVec& w = VMath::FloatInVec(1)) : VMath::Quat(x, y, z, w) {}
		Quat(const VMath::Vector3& v, float w) : VMath::Quat(v, w) {}
		Quat(const VMath::Vector3& v, const VMath::FloatInVec& w) : VMath::Quat(v, w) {}
		Quat(const VMath::Vector4& v) : VMath::Quat(v) {}
		Quat(const VMath::Matrix3& m) : VMath::Quat(m) {}
		Quat(const VMath::Quat& that) : VMath::Quat(that) {}
		Quat& operator=(const VMath::Quat& that) { VMath::Quat::operator=(that); return *this; }

		Quat(const glm::quat& that) : VMath::Quat(that.x, that.y, that.z, that.w) {}
		glm::quat GLM() const { return glm::quat(getW(), getX(), getY(), getZ()); }

		const float* ToPointer() const { return reinterpret_cast<const float*>(this); }

		bool IsEqual(const Quat& that, float e) const;

		bool IsIdentity() const;
	};
	VERUS_TYPEDEFS(Quat);
}
