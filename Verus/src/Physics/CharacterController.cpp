// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Physics;

CharacterController::CharacterController()
{
}

CharacterController::~CharacterController()
{
	Done();
}

void CharacterController::Init(RcPoint3 pos, RcDesc desc)
{
	VERUS_INIT();

	VERUS_QREF_BULLET;

	if (desc._radius >= VERUS_FLOAT_THRESHOLD)
		_radius = desc._radius;
	if (desc._height >= VERUS_FLOAT_THRESHOLD)
		_height = desc._height;
	float stepHeight = _radius;
	if (desc._stepHeight >= 0)
		stepHeight = desc._stepHeight;

	_offset = _radius + _height * 0.5f;

	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(pos.Bullet() + btVector3(0, _offset, 0));
	_pCapsule = new(_pCapsule.GetData()) btCapsuleShape(1, 1);
	_pGhostObject = new(_pGhostObject.GetData()) btPairCachingGhostObject();
	_pGhostObject->setWorldTransform(tr);
	_pGhostObject->setCollisionShape(_pCapsule.Get());
	_pGhostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
	_pGhostObject->setUserPointer(this);
	_pKCC = new(_pKCC.GetData()) KinematicCharacterController(_pGhostObject.Get(), _pCapsule.Get(), stepHeight);
	_pKCC->setGravity(-bullet.GetWorld()->getGravity().getY());
	_pKCC->setMaxSlope(btRadians(48));
	bullet.GetWorld()->addCollisionObject(_pGhostObject.Get(), +Group::character, -1);
	bullet.GetWorld()->addAction(_pKCC.Get());
	UpdateScaling();
}

void CharacterController::Done()
{
	VERUS_QREF_BULLET;

	if (_pKCC.Get())
	{
		bullet.GetWorld()->removeAction(_pKCC.Get());
		_pKCC.Delete();
	}
	if (_pGhostObject.Get())
	{
		bullet.GetWorld()->removeCollisionObject(_pGhostObject.Get());
		_pGhostObject.Delete();
	}
	_pCapsule.Delete();

	VERUS_DONE(CharacterController);
}

int CharacterController::UserPtr_GetType()
{
	return +Scene::NodeType::character;
}

void CharacterController::SetRadius(float r)
{
	const bool update = (r != _radius);
	_radius = r;
	if (update)
		UpdateScaling();
}

void CharacterController::SetHeight(float h)
{
	const Point3 pos = GetPosition();
	const bool update = (h != _height);
	_height = h;
	if (update)
	{
		UpdateScaling();
		MoveTo(pos);
	}
}

void CharacterController::UpdateScaling()
{
	_offset = _radius + _height * 0.5f;
	if (_pCapsule.Get())
		_pCapsule->setLocalScaling(btVector3(_radius, _height, _radius));
}

void CharacterController::Move(RcVector3 velocity)
{
	VERUS_QREF_TIMER;
	if (_pKCC.Get())
		_pKCC->setVelocityForTimeInterval(velocity.Bullet(), dt);
}

Point3 CharacterController::GetPosition()
{
	if (_pGhostObject.Get())
		return _pGhostObject->getWorldTransform().getOrigin() - btVector3(0, _offset, 0);
	return Point3(0);
}

void CharacterController::MoveTo(RcPoint3 pos)
{
	if (_pKCC.Get())
		_pKCC->warp(pos.Bullet() + btVector3(0, _offset, 0));
}

void CharacterController::Visualize(bool b)
{
	const int f = _pGhostObject->getCollisionFlags();
	if (b)
		_pGhostObject->setCollisionFlags(f & ~btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
	else
		_pGhostObject->setCollisionFlags(f | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
}
