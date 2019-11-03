#include "verus.h"

using namespace verus;
using namespace verus::AI;

void Turret::Update()
{
	VERUS_QREF_TIMER;

	float delta = _targetYaw - _actualYaw;
	if (delta <= -VERUS_PI) delta += VERUS_2PI;
	if (delta >= +VERUS_PI) delta -= VERUS_2PI;
	if (delta < 0)
		_actualYaw -= Math::Max(_yawSpeed * dt, -delta);
	else
		_actualYaw += Math::Min(_yawSpeed * dt, delta);
	_actualYaw = Math::WrapAngle(_actualYaw);
}

void Turret::LookAt(RcVector3 rayFromTurret, bool instantly)
{
	_targetYaw = Math::WrapAngle(atan2(
		static_cast<float>(rayFromTurret.getX()),
		static_cast<float>(rayFromTurret.getZ())));
	if (instantly)
		_actualYaw = _targetYaw;
}
