#include "verus.h"

using namespace verus;
using namespace verus::Game;

const float Spirit::s_defaultMaxPitch = VERUS_PI * (44.5f / 90.f);

Spirit::Spirit() :
	_smoothPosition(Point3(0), 10),
	_pitch(0, 10),
	_yaw(0, 10)
{
	ComputeDerivedVars();
}

Spirit::~Spirit()
{
}

void Spirit::ComputeDerivedVars()
{
	_dv._matPitch = Matrix3::rotationX(_pitch);
	_dv._matYaw = Matrix3::rotationY(_yaw);
	_dv._matRot = _dv._matYaw * _dv._matPitch;
	_dv._dirFront = _dv._matRot * Vector3(0, 0, 1);
	_dv._dirFront2D = _dv._matYaw * Vector3(0, 0, 1);
	_dv._dirSide = VMath::normalizeApprox(VMath::cross(_dv._dirFront, Vector3(0, 1, 0)));
	_dv._dirSide2D = VMath::normalizeApprox(VMath::cross(_dv._dirFront2D, Vector3(0, 1, 0)));
}

void Spirit::MoveFront(float x)
{
	_move += _dv._dirFront * x;
}

void Spirit::MoveSide(float x)
{
	_move += _dv._dirSide * x;
}

void Spirit::MoveFront2D(float x)
{
	_move += _dv._dirFront2D * x;
}

void Spirit::MoveSide2D(float x)
{
	_move += _dv._dirSide2D * x;
}

void Spirit::TurnPitch(float rad)
{
	_pitch = _pitch.GetTarget() + rad;
	_pitch = Math::Clamp<float>(_pitch.GetTarget(), -_maxPitch, _maxPitch);
}

void Spirit::TurnYaw(float rad)
{
	_yaw = Math::WrapAngle(_yaw.GetTarget() - rad);
}

void Spirit::SetPitch(float rad)
{
	_pitch = rad;
}

void Spirit::SetYaw(float rad)
{
	_yaw = Math::WrapAngle(rad);
}

void Spirit::HandleInput()
{
	VERUS_QREF_TIMER;

	// Update velocity:
	if (_accel)
	{
	}
	else // No accel:
	{
		_velocity = _move;
	}

	// At this point _move == _velocity.
}

void Spirit::Update()
{
	VERUS_QREF_TIMER;

	_pitch.Update();
	_yaw.Update();

	ComputeDerivedVars();

	// Update position:
	_positionPrev = _position;
	_smoothPositionPrev = _smoothPosition;
	{
		_position += _velocity * dt;
	}
	_smoothPosition = _position;
	_smoothPosition.Update();

	_speed = VMath::dist(_smoothPosition, _smoothPositionPrev) * timer.GetDeltaTimeInv();

	// Friction:
	if (_decel && _moveLen < VERUS_FLOAT_THRESHOLD)
	{
		{
			float speed = VMath::length(_velocity);
			if (speed >= VERUS_FLOAT_THRESHOLD)
			{
				const Vector3 dir = _velocity / speed;
				//speed = Math::Reduce(speed, _decel*dt);
				_velocity = dir * speed;
			}
		}
	}

	_move = Vector3(0);
}

void Spirit::SetAcceleration(float accel, float decel)
{
	_accel = accel;
	_decel = decel;
}

Point3 Spirit::GetPosition(bool smooth)
{
	return smooth ? static_cast<Point3>(_smoothPosition) : _position;
}

void Spirit::MoveTo(RcPoint3 pos, float onTerrain)
{
	_position = pos;
	_positionPrev = _position;
	_smoothPosition.ForceTarget(_position);
	_smoothPositionPrev = _position;
}

void Spirit::SetRemotePosition(RcPoint3 pos)
{
	_positionRemote = pos;
}

bool Spirit::FitRemotePosition()
{
	VERUS_QREF_TIMER;
	const Point3 pos = _position;
	const float d = Math::Max(9.f, VMath::distSqr(_positionRemote, pos) * 4);
	const float ratio = 1 / exp(dt * d);
	const bool warp = d >= 4 * 4;
	_position = warp ? _positionRemote : VMath::lerp(ratio, _positionRemote, pos);
	if (warp)
	{
		_positionPrev = _position;
		_smoothPosition.ForceTarget(_position);
		_smoothPositionPrev = _position;
	}
	_positionRemote += _velocity * dt; // Predict.
	return warp;
}

void Spirit::Rotate(RcVector3 front, float speed)
{
	VERUS_QREF_TIMER;

	const Vector3 dir = front;
	const float yaw = Math::WrapAngle(atan2(
		static_cast<float>(dir.getX()),
		static_cast<float>(dir.getZ())));
	Vector3 dir2D = dir;
	dir2D.setY(0);
	const float pitch = -atan(dir.getY() / VMath::length(dir2D));

	float dPitch = pitch - _pitch;
	float dYaw = yaw - _yaw;
	if (dYaw >= VERUS_PI)
		dYaw -= VERUS_2PI;
	if (dYaw <= -VERUS_PI)
		dYaw += VERUS_2PI;
	_pitch = _pitch + Math::Min(speed * dt, abs(dPitch)) * glm::sign(dPitch);
	_yaw = Math::WrapAngle(_yaw + Math::Min(speed * dt, abs(dYaw)) * glm::sign(dYaw));
}

void Spirit::LookAt(RcPoint3 point)
{
	const Vector3 dir = point - GetPosition();
	_yaw = Math::WrapAngle(atan2(
		static_cast<float>(dir.getX()),
		static_cast<float>(dir.getZ())));
	Vector3 dir2D = dir;
	dir2D.setY(0);
	_pitch = -atan(dir.getY() / VMath::length(dir2D));
}

RcMatrix3 Spirit::GetPitchMatrix() const
{
	return _dv._matPitch;
}

RcMatrix3 Spirit::GetYawMatrix() const
{
	return _dv._matYaw;
}

RcMatrix3 Spirit::GetRotationMatrix() const
{
	return _dv._matRot;
}

Transform3 Spirit::GetMatrix() const
{
	return Transform3(_dv._matRot, Vector3(_smoothPosition));
}

Transform3 Spirit::GetUprightMatrix() const
{
	return Transform3(_dv._matYaw, Vector3(_smoothPosition));
}
