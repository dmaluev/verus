// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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

	float maxHeight = -FLT_MAX;
	Vector3 averageWheelPos(0);
	VERUS_FOR(i, leftWheelCount)
	{
		averageWheelPos += Vector3(desc._vLeftWheels[i].GetCenter());
		maxHeight = Math::Max<float>(maxHeight, desc._vLeftWheels[i].GetCenter().getY());
	}
	VERUS_FOR(i, rightWheelCount)
	{
		averageWheelPos += Vector3(desc._vRightWheels[i].GetCenter());
		maxHeight = Math::Max<float>(maxHeight, desc._vRightWheels[i].GetCenter().getY());
	}
	averageWheelPos *= _invWheelCount;

	Math::Bounds chassis = desc._chassis;
	chassis.Set(maxHeight - 0.05f, chassis.GetMax().getY(), 1);

	const Vector3 centerOfMassOffset = chassis.GetCenter() - Vector3(0, chassis.GetExtents().getY() * 0.8f, 0);
	const Transform3 transformCoM = Transform3::translation(-centerOfMassOffset);
	const float g = bullet.GetWorld()->getGravity().length();
	const float forcePerWheel = (desc._mass * g) * _invWheelCount;
	const float k = forcePerWheel / desc._suspensionRestLength; // Hooke's law.

	_vehicleTuning.m_suspensionStiffness = k / desc._mass;
	_vehicleTuning.m_suspensionCompression = _vehicleTuning.m_suspensionStiffness * 0.2f;
	_vehicleTuning.m_suspensionDamping = _vehicleTuning.m_suspensionStiffness * 0.35f;
	_vehicleTuning.m_maxSuspensionTravelCm = 100 * 2 * desc._suspensionRestLength;
	_vehicleTuning.m_frictionSlip = 2.5f;
	_vehicleTuning.m_maxSuspensionForce = forcePerWheel * 3;

	averageWheelPos -= centerOfMassOffset;
	averageWheelPos.setY(0);
	if (!desc._vRightWheels.empty())
		_frontRightWheelIndex = leftWheelCount;

	_pChassisShape = new(_pChassisShape.GetData()) btBoxShape(chassis.GetExtents().Bullet());
	_pCompoundShape = new(_pCompoundShape.GetData()) btCompoundShape();
	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(chassis.GetCenter().Bullet() - centerOfMassOffset.Bullet());
	const btTransform trCoM = transformCoM.Bullet();
	_pCompoundShape->addChildShape(tr, _pChassisShape.Get());
	_pChassis = bullet.AddNewRigidBody(_pChassis, desc._mass, desc._tr.Bullet(), _pCompoundShape.Get(),
		Group::vehicle, Group::all, &trCoM);
	_pChassis->setFriction(0);
	_pChassis->setRestitution(0);
	_pChassis->setUserPointer(this);
	_pChassis->setActivationState(DISABLE_DEACTIVATION);
	_pChassis->setAngularFactor(btVector3(0.5f, 1, 0.5f)); // For stability.
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
			const float scale = ratio * ratio;
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
	_pChassisShape.Delete();

	VERUS_DONE(Vehicle);
}

void Vehicle::Update()
{
	const Transform3 tr = GetTransform();
	if (tr.getCol1().getY() > 0.25f)
	{
		_pChassis->setFriction(0);
		_pChassis->setRestitution(0);
	}
	else // Rollover?
	{
		_pChassis->setFriction(Bullet::GetFriction(Material::metal) * 0.5f);
		_pChassis->setRestitution(Bullet::GetRestitution(Material::metal) * 0.25f);
	}
}

Transform3 Vehicle::GetTransform() const
{
	return static_cast<btDefaultMotionState*>(_pChassis->getMotionState())->m_graphicsWorldTrans;
}

void Vehicle::ApplyAirForce(float scale)
{
	btVector3 v = _pChassis->getLinearVelocity();
	const float speedSq = v.length2();
	if (speedSq >= 1e-4f)
	{
		const float speed = sqrt(speedSq);
		v /= speed;
		const float force = speed * speed * scale;
		_pChassis->applyCentralForce(-v * force); // Drag.
		const Transform3 tr = GetTransform();
		const Vector3 down = -tr.getCol1();
		_pChassis->applyCentralForce(down.Bullet() * force); // Downforce.
	}
}

void Vehicle::SetBrake(float amount)
{
	const float perWheel = amount * _invWheelCount;
	VERUS_FOR(i, _wheelCount)
		_pRaycastVehicle->setBrake(perWheel, i);
}

void Vehicle::SetEngineForce(float force)
{
	const float perWheel = force * _invWheelCount;
	VERUS_FOR(i, _wheelCount)
		_pRaycastVehicle->applyEngineForce(perWheel, i);
}

void Vehicle::SetSteeringAngle(float angle)
{
	_pRaycastVehicle->setSteeringValue(angle, 0);
	if (_frontRightWheelIndex > 0)
		_pRaycastVehicle->setSteeringValue(angle, _frontRightWheelIndex);
}

void Vehicle::UseHandBrake(float amount)
{
	const float perWheel = amount * _invWheelCount;
	VERUS_FOR(i, _wheelCount)
	{
		if (i != 0 && i != _frontRightWheelIndex)
		{
			_pRaycastVehicle->setBrake(perWheel, i);
			_pRaycastVehicle->applyEngineForce(0, i);
		}
	}
}

int Vehicle::UserPtr_GetType()
{
	return +Scene::NodeType::vehicle;
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
		return 1.8f;

	const float speedKmHourSigned = _pRaycastVehicle->getCurrentSpeedKmHour();
	const float speedKmHour = abs(speedKmHourSigned);
	const float base = 1.6f;
	const float first = base * 10;
	const float gear = (speedKmHourSigned < first) ? Math::Clamp<float>(speedKmHour / first, 0, 0.99f) : log(speedKmHour * 0.1f) / log(base);
	return 0.8f + Math::Min(0.2f, speedKmHour * 0.004f) + 0.6f * fmod(gear, 1.f);
}
