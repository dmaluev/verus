// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
		_actualYaw -= Math::Min(_yawSpeed * dt, -delta);
	else
		_actualYaw += Math::Min(_yawSpeed * dt, delta);
	_actualYaw = Math::WrapAngle(_actualYaw);
}

bool Turret::IsTargetPitchReached(float threshold) const
{
	const float delta = _targetPitch - _actualPitch;
	return abs(Math::WrapAngle(delta)) < threshold;
}

bool Turret::IsTargetYawReached(float threshold) const
{
	const float delta = _targetYaw - _actualYaw;
	return abs(Math::WrapAngle(delta)) < threshold;
}

void Turret::LookAt(RcVector3 rayFromTurret, bool instantly)
{
	_targetYaw = Math::WrapAngle(atan2(
		static_cast<float>(rayFromTurret.getX()),
		static_cast<float>(rayFromTurret.getZ())));
	if (instantly)
		_actualYaw = _targetYaw;
}
