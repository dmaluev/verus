// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CameraOrbit::CameraOrbit() :
	_pitch(0, 18),
	_yaw(0, 18),
	_radius(10, 8)
{
}

CameraOrbit::~CameraOrbit()
{
}

void CameraOrbit::Update()
{
	const Vector3 toEye = Matrix3::rotationZYX(Vector3(_pitch, _yaw, 0)) * Vector3(0, 0, _radius);
	MoveEyeTo(_atPos + toEye);
	MainCamera::Update();
}

void CameraOrbit::UpdateUsingEyeAt()
{
	const Vector3 toEye = _eyePos - _atPos;
	const float distToEye = VMath::length(toEye);
	const Vector3 dirToEye = toEye / distToEye;

	const float yaw = Math::WrapAngle(atan2(dirToEye.getX(), dirToEye.getZ()));
	const float pitch = asin(Math::Clamp<float>(-dirToEye.getY(), -1, 1));

	_pitch = pitch;
	_pitch.ForceTarget();
	_yaw = yaw;
	_yaw.ForceTarget();
	_radius = distToEye;
	_radius.ForceTarget();

	Update();
}

void CameraOrbit::UpdateElastic()
{
	if (!_elastic)
		return;
	_pitch.Update();
	_yaw.Update();
	_radius.Update();
}

void CameraOrbit::DragController_GetParams(float& x, float& y)
{
	x = _yaw;
	y = _pitch;
}

void CameraOrbit::DragController_SetParams(float x, float y)
{
	_yaw = Math::WrapAngle(x);
	_pitch = Math::Clamp(y, -VERUS_PI * 0.49f, VERUS_PI * 0.49f);
	if (!_elastic)
	{
		_pitch.ForceTarget();
		_yaw.ForceTarget();
	}
	Update();
}

void CameraOrbit::DragController_GetRatio(float& x, float& y)
{
	x = -VERUS_2PI / 1440.f;
	y = -VERUS_2PI / 1440.f;
}

void CameraOrbit::MulRadiusBy(float a)
{
	_radius = _radius.GetTarget() * a;
	if (!_elastic)
		_radius.ForceTarget();
	Update();
}
