// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Anim;

Orbit::Orbit() :
	_pitch(0, 5),
	_yaw(0, 5)
{
	_matrix = Matrix3::identity();
}

Orbit::~Orbit()
{
}

void Orbit::Update(bool smoothRestore)
{
	VERUS_QREF_TIMER;

	if (!_locked && smoothRestore)
	{
		const float strength = 2;
		if (_yaw.GetTarget() >= 0)
		{
			const float deltaBase = _baseYaw;
			const float delta = _yaw.GetTarget();
			const float speedScaleA = Math::Clamp((deltaBase - delta) * strength, 0.5f, 1.f);
			const float speedScaleB = Math::Clamp(delta * strength, 0.1f, 1.f);
			const float speedScale = Math::Min(speedScaleA, speedScaleB);
			_yaw = _yaw.GetTarget() - Math::Clamp(_speed * dt * speedScale, 0.f, delta);
		}
		else
		{
			const float deltaBase = -_baseYaw;
			const float delta = -_yaw.GetTarget();
			const float speedScaleA = Math::Clamp((deltaBase - delta) * strength, 0.5f, 1.f);
			const float speedScaleB = Math::Clamp(delta * strength, 0.1f, 1.f);
			const float speedScale = Math::Min(speedScaleA, speedScaleB);
			_yaw = _yaw.GetTarget() + Math::Clamp(_speed * dt * speedScale, 0.f, delta);
		}

		if (_pitch.GetTarget() >= 0)
		{
			const float deltaBase = _basePitch;
			const float delta = _pitch.GetTarget();
			const float speedScaleA = Math::Clamp((deltaBase - delta) * strength, 0.5f, 1.f);
			const float speedScaleB = Math::Clamp(delta * strength, 0.1f, 1.f);
			const float speedScale = Math::Min(speedScaleA, speedScaleB);
			_pitch = _pitch.GetTarget() - Math::Clamp(_speed * dt * speedScale, 0.f, delta);
		}
		else
		{
			const float deltaBase = -_basePitch;
			const float delta = -_pitch.GetTarget();
			const float speedScaleA = Math::Clamp((deltaBase - delta) * strength, 0.5f, 1.f);
			const float speedScaleB = Math::Clamp(delta * strength, 0.1f, 1.f);
			const float speedScale = Math::Min(speedScaleA, speedScaleB);
			_pitch = _pitch.GetTarget() + Math::Clamp(_speed * dt * speedScale, 0.f, delta);
		}
	}

	if (smoothRestore)
	{
		_pitch.Update();
		_yaw.Update();
	}
	else
	{
		_pitch.ForceTarget();
		_yaw.ForceTarget();
	}

	const Matrix3 rotPitch = Matrix3::rotationX(_pitch);
	const Matrix3 rotYaw = Matrix3::rotationY(_yaw);
	_matrix = rotYaw * rotPitch;

	const float pitchOffsetStrength = abs(_pitch) * (2 / VERUS_PI);
	const float yawOffsetStrength = abs(_yaw) * (2 / VERUS_PI);
	_offsetStrength = Math::Clamp<float>(Math::Max(pitchOffsetStrength, yawOffsetStrength), 0, 1);
}

void Orbit::Lock()
{
	_locked = true;
}

void Orbit::Unlock()
{
	if (_locked)
	{
		_basePitch = _pitch.GetTarget();
		_baseYaw = _yaw.GetTarget();
	}
	_locked = false;
}

void Orbit::AddPitch(float a)
{
	if (_locked)
	{
		_pitch = _pitch.GetTarget() - a;
		_pitch = Math::Clamp(_pitch.GetTarget(), -VERUS_PI * _maxPitch, VERUS_PI * _maxPitch);
	}
}

void Orbit::AddYaw(float a)
{
	if (_locked)
		_yaw = Math::WrapAngle(_yaw.GetTarget() - a);
}

void Orbit::AddPitchFree(float a)
{
	_pitch = _pitch.GetTarget() - a;
	_pitch = Math::Clamp(_pitch.GetTarget(), -VERUS_PI * _maxPitch, VERUS_PI * _maxPitch);
	_basePitch = _pitch.GetTarget();
}

void Orbit::AddYawFree(float a)
{
	_yaw = Math::WrapAngle(_yaw.GetTarget() - a);
	_baseYaw = _yaw.GetTarget();
}
