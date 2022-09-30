// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Physics;

Vehicle::Vehicle()
{
}

Vehicle::~Vehicle()
{
	Done();
}

void Vehicle::Init(RcDesc desc)
{
	VERUS_INIT();

	VERUS_QREF_BULLET;

	const int leftWheelCount = Utils::Cast32(desc._vLeftWheels.size());
	const int rightWheelCount = Utils::Cast32(desc._vRightWheels.size());
	_wheelCount = leftWheelCount + rightWheelCount;
	_invWheelCount = 1.f / _wheelCount;

	// <Clearance>
	float maxWheelY = -FLT_MAX;
	Vector3 averageWheelPos(0);
	VERUS_FOR(i, leftWheelCount)
	{
		averageWheelPos += Vector3(desc._vLeftWheels[i].GetCenter());
		maxWheelY = Math::Max<float>(maxWheelY, desc._vLeftWheels[i].GetCenter().getY());
	}
	VERUS_FOR(i, rightWheelCount)
	{
		averageWheelPos += Vector3(desc._vRightWheels[i].GetCenter());
		maxWheelY = Math::Max<float>(maxWheelY, desc._vRightWheels[i].GetCenter().getY());
	}
	// </Clearance>
	averageWheelPos *= _invWheelCount;

	Math::Bounds chassis = desc._chassis;
	chassis.Set(maxWheelY - 0.05f, chassis.GetMax().getY(), 1); // Adjust clearance.
	chassis.Set(
		chassis.GetMin().getZ() + chassis.GetExtents().getY(),
		chassis.GetMax().getZ() - chassis.GetExtents().getY(),
		2); // Make space for bumpers.

	// Lower the center of gravity:
	const Vector3 centerOfMassOffset = chassis.GetCenter() - Vector3(0, chassis.GetExtents().getY() * 0.8f, 0);
	const Transform3 transformCoM = Transform3::translation(-centerOfMassOffset);
	const btTransform trCoM = transformCoM.Bullet();
	const float g = bullet.GetWorld()->getGravity().length();
	const float forcePerWheel = (desc._mass * g) * _invWheelCount; // Newton's law (F = m * a).
	const float k = forcePerWheel / desc._suspensionRestLength; // Hooke's law (F = k * x).

	_vehicleTuning.m_suspensionStiffness = k / desc._mass; // Per 1 kg.
	_vehicleTuning.m_suspensionCompression = _vehicleTuning.m_suspensionStiffness * 0.15f;
	_vehicleTuning.m_suspensionDamping = _vehicleTuning.m_suspensionStiffness * 0.2f;
	_vehicleTuning.m_maxSuspensionTravelCm = 100 * 2 * desc._suspensionRestLength;
	_vehicleTuning.m_frictionSlip = 1.5f;
	_vehicleTuning.m_maxSuspensionForce = forcePerWheel * 3;

	averageWheelPos -= centerOfMassOffset;
	averageWheelPos.setY(0);
	if (!desc._vRightWheels.empty())
		_frontRightWheelIndex = leftWheelCount;

	const float bumperR = chassis.GetExtents().getY();
	const float bumperH = Math::Max(VERUS_FLOAT_THRESHOLD, chassis.GetDimensions().getX() - bumperR * 2);

	_pChassisShape = new(_pChassisShape.GetData()) btBoxShape(chassis.GetExtents().Bullet());
	_pBumperShape = new(_pBumperShape.GetData()) btCapsuleShape(bumperR, bumperH);
	_pCompoundShape = new(_pCompoundShape.GetData()) btCompoundShape();
	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(chassis.GetCenter().Bullet() - centerOfMassOffset.Bullet());
	_pCompoundShape->addChildShape(tr, _pChassisShape.Get());
	tr.setIdentity();
	tr.getBasis().setEulerZYX(0, 0, VERUS_PI * 0.5f);
	tr.setOrigin(chassis.GetCenter().Bullet() - centerOfMassOffset.Bullet() + btVector3(0, 0, chassis.GetExtents().getZ()));
	_pCompoundShape->addChildShape(tr, _pBumperShape.Get());
	tr.setIdentity();
	tr.getBasis().setEulerZYX(0, 0, VERUS_PI * 0.5f);
	tr.setOrigin(chassis.GetCenter().Bullet() - centerOfMassOffset.Bullet() - btVector3(0, 0, chassis.GetExtents().getZ()));
	_pCompoundShape->addChildShape(tr, _pBumperShape.Get());

	_pChassis = bullet.AddNewRigidBody(_pChassis, desc._mass, desc._tr.Bullet(), _pCompoundShape.Get(),
		+Group::transport, +Group::all, &trCoM);
	_pChassis->setFriction(Bullet::GetFriction(Material::metal));
	_pChassis->setRestitution(Bullet::GetRestitution(Material::metal) * 0.25f); // Energy-absorbing deformation.
	_pChassis->setUserPointer(this);
	_pChassis->setActivationState(DISABLE_DEACTIVATION);
	_pVehicleRaycaster = new(_pVehicleRaycaster.GetData()) btDefaultVehicleRaycaster(bullet.GetWorld());
	_pRaycastVehicle = new(_pRaycastVehicle.GetData()) btRaycastVehicle(_vehicleTuning, _pChassis.Get(), _pVehicleRaycaster.Get());
	_pRaycastVehicle->setCoordinateSystem(0, 1, 2);
	bullet.GetWorld()->addAction(_pRaycastVehicle.Get());

	auto AddWheels = [this, &centerOfMassOffset, &averageWheelPos](const Vector<Math::Sphere>& vWheels, float suspensionRestLength)
	{
		const btVector3 wheelDir(0, -1, 0);
		const btVector3 wheelAxle(-1, 0, 0);
		const int wheelCount = Utils::Cast32(vWheels.size());
		VERUS_FOR(i, wheelCount)
		{
			Math::RcSphere wheel = vWheels[i];
			const bool front = !i;
			btRaycastVehicle::btVehicleTuning vehicleTuning(_vehicleTuning);
			const Point3 pos = wheel.GetCenter() - centerOfMassOffset;
			const float ratio = VMath::dist(pos, Point3(averageWheelPos)) / VMath::length(Vector3(pos));
			const float scale = ratio * ratio; // Try to level out the car.
			vehicleTuning.m_suspensionStiffness *= scale;
			vehicleTuning.m_suspensionCompression *= scale;
			vehicleTuning.m_suspensionDamping *= scale;

			_pRaycastVehicle->addWheel(
				pos.Bullet(),
				wheelDir,
				wheelAxle,
				suspensionRestLength,
				wheel.GetRadius(),
				vehicleTuning,
				front);
		}
	};
	AddWheels(desc._vLeftWheels, desc._suspensionRestLength);
	AddWheels(desc._vRightWheels, desc._suspensionRestLength);

	if (_frontRightWheelIndex > 0)
		_invHandBrakeWheelCount = 1.f / (_wheelCount - 2);
	else
		_invHandBrakeWheelCount = 1.f / (_wheelCount - 1);
}

void Vehicle::Done()
{
	VERUS_QREF_BULLET;

	if (_pRaycastVehicle.Get())
	{
		bullet.GetWorld()->removeAction(_pRaycastVehicle.Get());
		_pRaycastVehicle.Delete();
	}
	_pVehicleRaycaster.Delete();

	if (_pChassis.Get())
	{
		bullet.GetWorld()->removeRigidBody(_pChassis.Get());
		_pChassis.Delete();
	}
	_pCompoundShape.Delete();
	_pBumperShape.Delete();
	_pChassisShape.Delete();

	VERUS_DONE(Vehicle);
}

void Vehicle::Update()
{
}

Transform3 Vehicle::GetTransform() const
{
	return static_cast<btDefaultMotionState*>(_pChassis->getMotionState())->m_graphicsWorldTrans;
}

void Vehicle::ApplyAirForce(float scale)
{
	btVector3 v = _pChassis->getLinearVelocity();
	const float speedSq = v.length2();
	if (speedSq >= VERUS_FLOAT_THRESHOLD)
	{
		const float speed = sqrt(speedSq);
		v /= speed;
		const float force = speed * speed * scale * 2;
		_pChassis->applyCentralForce(-v * force); // Drag.
		const Transform3 tr = GetTransform();
		const Vector3 down = -tr.getCol1();
		_pChassis->applyCentralForce(down.Bullet() * force); // Downforce.
	}
}

void Vehicle::SetBrake(float brake, float handBrake, int index)
{
	bool setHandBrake = true;
	if (index >= 0)
	{
		_pRaycastVehicle->setBrake(brake, index);
		setHandBrake = false;
	}
	else if (-2 == index) // Front wheel drive?
	{
		const float perWheel = brake * (_frontRightWheelIndex > 0 ? 0.5f : 1);
		_pRaycastVehicle->setBrake(perWheel, 0);
		if (_frontRightWheelIndex > 0)
			_pRaycastVehicle->setBrake(perWheel, _frontRightWheelIndex);
	}
	else // All wheel drive?
	{
		const float perWheel = brake * _invWheelCount;
		VERUS_FOR(i, _wheelCount)
			_pRaycastVehicle->setBrake(perWheel, i);
	}

	if (setHandBrake)
	{
		const float perWheel = handBrake * _invHandBrakeWheelCount;
		VERUS_FOR(i, _wheelCount)
		{
			if (i != 0 && i != _frontRightWheelIndex)
				_pRaycastVehicle->setBrake(perWheel, i);
		}
	}
}

void Vehicle::SetEngineForce(float force, int index)
{
	if (index >= 0)
	{
		_pRaycastVehicle->applyEngineForce(force, index);
	}
	else if (-2 == index) // Front wheel drive?
	{
		const float perWheel = force * (_frontRightWheelIndex > 0 ? 0.5f : 1);
		_pRaycastVehicle->applyEngineForce(perWheel, 0);
		if (_frontRightWheelIndex > 0)
			_pRaycastVehicle->applyEngineForce(perWheel, _frontRightWheelIndex);
	}
	else // All wheel drive?
	{
		const float perWheel = force * _invWheelCount;
		VERUS_FOR(i, _wheelCount)
			_pRaycastVehicle->applyEngineForce(perWheel, i);
	}
}

void Vehicle::SetSteeringAngle(float angle)
{
	_pRaycastVehicle->setSteeringValue(angle, 0);
	if (_frontRightWheelIndex > 0)
		_pRaycastVehicle->setSteeringValue(angle, _frontRightWheelIndex);
}

int Vehicle::UserPtr_GetType()
{
	return +World::NodeType::vehicle;
}

float Vehicle::ComputeEnginePitch() const
{
	bool contact = false;
	VERUS_FOR(i, _wheelCount)
	{
		if (_pRaycastVehicle->getWheelInfo(i).m_raycastInfo.m_isInContact)
		{
			contact = true;
			break;
		}
	}
	if (!contact)
		return 1.5f;

	const float speedKmHourSigned = _pRaycastVehicle->getCurrentSpeedKmHour();
	const float speedKmHour = abs(speedKmHourSigned);
	const float power = 1 / 2.5f;
	const float first = 15.588f; // 3^2.5
	const float gear = (speedKmHourSigned < first) ?
		Math::Clamp<float>(speedKmHour / first, 0, 0.99f) :
		Math::Clamp<float>(pow(speedKmHour, power) - 1, 2, 5.99f);
	return 0.8f + Math::Min(0.2f, speedKmHour * 0.004f) + 0.6f * fmod(gear, 1.f);
}
