// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

BaseCharacter::BaseCharacter() :
	_fallSpeed(0, 5)
{
	_cameraRadius.Set(_maxCameraRadius, 3);
	_cameraRadius.SetLimits(_minCameraRadius, _maxCameraRadius);
}

BaseCharacter::~BaseCharacter()
{
	EndRagdoll();
	DoneController();
}

void BaseCharacter::HandleActions()
{
	VERUS_QREF_TIMER;

	if (_accel)
	{
		// Normalize move vector:
		_moveLen = VMath::length(_move);
		if (_moveLen >= VERUS_FLOAT_THRESHOLD)
			_move /= _moveLen;

		const Vector3 v0 = _endVelocity;

		if (_airborne) // Move action from user.
			_endVelocity += _move * (dt * _accel * _airborneControl);
		else
			_endVelocity += _move * (dt * _accel);

		// Speed limit & friction (on the ground):
		if (!_airborne)
		{
			Vector3 velocity2D = _endVelocity;
			velocity2D.setY(0);
			const float speed2D = VMath::length(velocity2D);
			if (speed2D > _maxSpeed) // Max?
			{
				velocity2D /= speed2D;
				velocity2D *= _maxSpeed;
			}
			else if (speed2D < VERUS_FLOAT_THRESHOLD) // Min?
			{
				velocity2D = Vector3(0);
			}
			else if (_decel && _moveLen < VERUS_FLOAT_THRESHOLD)
			{
				const Vector3 dir = velocity2D / speed2D;
				velocity2D = dir * Math::Reduce(speed2D, _decel * dt);
			}
			_endVelocity = velocity2D;
		}

		// Extra force:
		_endVelocity += _extraForce * dt;
		_extraForce = Vector3(0);

		_avgVelocity = VMath::lerp(0.5f, v0, _endVelocity);

		_move = _avgVelocity;
	}
	else // No accel:
	{
		_avgVelocity = _endVelocity = _move;
	}

	// At this point _move == _avgVelocity.
	_cc.Move(_move);
}

void BaseCharacter::Update()
{
	VERUS_QREF_TIMER;

	_pitch.Update();
	_yaw.Update();

	// <Position>
	_prevPosition = _position;
	_smoothPrevPosition = _smoothPosition;
	if (_ragdoll)
	{
		// Do nothing.
	}
	else if (_cc.IsInitialized())
	{
		_position = _cc.GetPosition();
		_airborne = !_cc.GetKCC()->onGround();
	}
	else
	{
		_position += _avgVelocity * dt;
		_airborne = true;
	}
	_smoothPosition = _position;
	_smoothPosition.Update();
	// </Position>

	// <Fall>
	if (_airborne)
	{
		const float h0 = _prevPosition.getY();
		const float h1 = _position.getY();
		_fallSpeed = (h0 - h1) * timer.GetDeltaTimeInv();
	}
	else
	{
		BaseCharacter_OnFallImpact(_fallSpeed);
		_fallSpeed.ForceTarget(0);
	}
	_fallSpeed.Update();
	// </Fall>

	_speed = VMath::dist(_smoothPosition, _smoothPrevPosition) * timer.GetDeltaTimeInv();
	_smoothSpeed = _speed;
	_smoothSpeed.Update();
	_smoothSpeedOnGround = _airborne ? 0.f : _speed;
	_smoothSpeedOnGround.Update();

	_move = Vector3(0);
	ComputeDerivedVars(_smoothSpeedOnGround);

	_cameraRadius.UpdateClamped(dt);
}

void BaseCharacter::InitController()
{
	if (!_cc.IsInitialized())
	{
		Physics::CharacterController::Desc desc;
		desc._radius = _idleRadius;
		desc._height = _idleHeight;
		desc._stepHeight = _stepHeight;
		_cc.Init(_position, desc);
		_cc.GetKCC()->setJumpSpeed(_jumpSpeed);
#ifdef _DEBUG
		_cc.Visualize(true);
#endif
	}
}

void BaseCharacter::DoneController()
{
	_position = GetPosition();
	_cc.Done();
}

void BaseCharacter::MoveTo(RcPoint3 pos)
{
	Spirit::MoveTo(pos);
	_fallSpeed.ForceTarget(0);
	if (_cc.IsInitialized())
		_cc.MoveTo(_position);
}

bool BaseCharacter::FitRemotePosition()
{
	const bool warp = Spirit::FitRemotePosition();
	if (warp)
		_fallSpeed.ForceTarget(0);
	if (_cc.IsInitialized())
		_cc.MoveTo(_position);
	return warp;
}

bool BaseCharacter::IsOnGround() const
{
	if (_cc.IsInitialized())
		return _cc.GetKCC()->onGround();
	return false;
}

bool BaseCharacter::TryJump(bool whileAirborne)
{
	if ((!_airborne || whileAirborne) && _jumpCooldown.IsFinished())
	{
		BaseCharacter_OnJump();
		return true;
	}
	return false;
}

void BaseCharacter::Jump()
{
	if (_cc.IsInitialized())
	{
		_jumpCooldown.Start();
		_cc.GetKCC()->jump();
	}
}

void BaseCharacter::SetJumpSpeed(float v)
{
	_jumpSpeed = v;
	if (_cc.IsInitialized())
		_cc.GetKCC()->setJumpSpeed(_jumpSpeed);
}

bool BaseCharacter::IsStuck()
{
	VERUS_QREF_TIMER;
	if (!_unstuckCooldown.IsFinished())
		return false;
	const float desiredSpeedSq = VMath::lengthSqr(_avgVelocity);
	const bool stuck = (desiredSpeedSq >= 1) && (_speed * _speed * 10 < desiredSpeedSq);
	if (stuck)
		_unstuckCooldown.Start();
	return stuck;
}

void BaseCharacter::ToggleRagdoll()
{
	if (_ragdoll)
		EndRagdoll();
	else
		BeginRagdoll();
}

void BaseCharacter::BeginRagdoll()
{
	if (_ragdoll)
		return;

	_ragdoll = true;
	BaseCharacter_OnInitRagdoll(GetMatrix());

	_avgVelocity = _endVelocity = Vector3(0);
	_move = Vector3(0);
	_extraForce = Vector3(0);

	DoneController();
}

void BaseCharacter::EndRagdoll()
{
	if (!_ragdoll)
		return;

	_ragdoll = false;
	BaseCharacter_OnDoneRagdoll();

	InitController();
}

void BaseCharacter::ComputeThirdPersonCameraArgs(RcVector3 offset, RPoint3 eye, RPoint3 at)
{
	VERUS_QREF_SM;

	const float r = 0.1f;
	Point3 point;
	Vector3 norm;

	const RcVector3 offsetW = GetYawMatrix() * offset;
	const Point3 pos = _smoothPosition;
	const float startAt = _cc.GetRadius() + _cc.GetHeight() * 0.5f; // Inside capsule.
	const Point3 origin = pos + Vector3(0, startAt, 0);
	at = pos + offsetW;
	if (sm.RayCastingTest(origin, at, nullptr, &point, &norm, &r))
		at = point + norm * r;
	eye = at - GetFrontDirection() * _cameraRadius.GetValue() + offsetW * 0.05f;
}

float BaseCharacter::ComputeThirdPersonCamera(Scene::RCamera camera, Anim::RcOrbit orbit, RcVector3 offset)
{
	VERUS_QREF_SM;

	const float r = 0.1f;
	Point3 point;
	Vector3 norm;

	Point3 eye, at;
	ComputeThirdPersonCameraArgs(offset, eye, at);
	Vector3 toEye = eye - at;

	// Consider orientation:
	Matrix3 matFromRaySpace;
	matFromRaySpace.TrackToZ(toEye);
	Matrix3 matToRaySpace = VMath::transpose(matFromRaySpace);
	toEye = (matFromRaySpace * orbit.GetMatrix() * matToRaySpace) * toEye;

	eye = at + Vector3(toEye);

	float ret = 0;
	if (sm.RayCastingTest(at, eye, nullptr, &point, &norm, &r)) // Hitting the wall?
	{
		eye = point + norm * r;
		const float maxCameraRadius = VMath::dist(at, eye) + r;
		_cameraRadius.SetLimits(_minCameraRadius, maxCameraRadius);
		ret = 1 - maxCameraRadius / _maxCameraRadius;
	}
	else // No collision?
	{
		_cameraRadius.SetLimits(_minCameraRadius, _maxCameraRadius);
	}
	if (VMath::distSqr(at, eye) < r * r) // Extremely close?
	{
		_cameraRadius.SetLimits(_minCameraRadius, r);
		eye = at - GetFrontDirection() * r;
		ret = 1;
	}

	camera.MoveEyeTo(eye);
	camera.MoveAtTo(at);

	return ret;
}

void BaseCharacter::ComputeThirdPersonAim(RPoint3 aimPos, RVector3 aimDir, RcVector3 offset)
{
	VERUS_QREF_SM;

	const float r = 0.1f;
	Point3 point;
	Vector3 norm;

	Point3 eye, at;
	ComputeThirdPersonCameraArgs(offset, eye, at);

	if (sm.RayCastingTest(at, eye, nullptr, &point, &norm, &r)) // Hitting the wall?
	{
		eye = point + norm * r;
	}
	else // No collision?
	{
	}
	if (VMath::distSqr(at, eye) < r * r) // Extremely close?
	{
		eye = at - GetFrontDirection() * r;
	}

	aimPos = eye;
	aimDir = VMath::normalizeApprox(at - eye);
}

void BaseCharacter::SetMaxCameraRadius(float r)
{
	_maxCameraRadius = r;
	_cameraRadius.SetLimits(_minCameraRadius, r);
	_cameraRadius.UpdateClamped(0);
}
