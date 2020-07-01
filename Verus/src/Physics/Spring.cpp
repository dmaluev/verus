#include "verus.h"

using namespace verus;
using namespace verus::Physics;

Spring::Spring(float stiffness, float damping) :
	_stiffness(stiffness),
	_damping(damping)
{
}

void Spring::Update(RcVector3 appliedForce)
{
	VERUS_QREF_TIMER;

	const Vector3 force = appliedForce - _offset * _stiffness - _velocity * _damping; // Hooke's law.
	_velocity += force * dt;
	_offset += _velocity * dt;

	if (VMath::lengthSqr(_velocity) > 100 * 100.f)
		_velocity = Vector3(0);
	if (VMath::lengthSqr(_offset) > 10 * 10.f)
		_offset = Vector3(0);
}
