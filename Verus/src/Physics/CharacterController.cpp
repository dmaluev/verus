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

	if (desc._radius >= 1e-4f)
		_radius = desc._radius;
	if (desc._height >= 1e-4f)
		_height = desc._height;
	float stepHeight = _radius;
	if (desc._stepHeight >= 0)
		stepHeight = desc._stepHeight;

	_offset = _radius + _height * 0.5f;

	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(pos.Bullet() + btVector3(0, _offset, 0));
	_pCapsule = new btCapsuleShape(1, 1);
	_pGhostObject = new btPairCachingGhostObject();
	_pGhostObject->setWorldTransform(tr);
	_pGhostObject->setCollisionShape(_pCapsule);
	_pGhostObject->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);
	_pGhostObject->setUserPointer(this);
	_pKCC = new KinematicCharacterController(_pGhostObject, _pCapsule, stepHeight);
	_pKCC->setGravity(-bullet.GetWorld()->getGravity().getY());
	_pKCC->setMaxSlope(btRadians(48));
	bullet.GetWorld()->addCollisionObject(_pGhostObject, +Group::character, -1);
	bullet.GetWorld()->addAction(_pKCC);
	UpdateScaling();
}

void CharacterController::Done()
{
	VERUS_QREF_BULLET;

	if (_pKCC)
	{
		bullet.GetWorld()->removeAction(_pKCC);
		delete _pKCC;
		_pKCC = nullptr;
	}

	if (_pGhostObject)
	{
		bullet.GetWorld()->removeCollisionObject(_pGhostObject);
		delete _pGhostObject;
		_pGhostObject = nullptr;
	}

	if (_pCapsule)
	{
		delete _pCapsule;
		_pCapsule = nullptr;
	}

	VERUS_DONE(CharacterController);
}

int CharacterController::UserPtr_GetType()
{
	return 0;
	//return +Scene::NodeType::character;
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
	if (_pCapsule)
		_pCapsule->setLocalScaling(btVector3(_radius, _height, _radius));
}

void CharacterController::Move(RcVector3 velocity)
{
	VERUS_QREF_TIMER;
	if (_pKCC)
		_pKCC->setVelocityForTimeInterval(velocity.Bullet(), dt);
}

Point3 CharacterController::GetPosition()
{
	if (_pGhostObject)
		return _pGhostObject->getWorldTransform().getOrigin() - btVector3(0, _offset, 0);
	return Point3(0);
}

void CharacterController::MoveTo(RcPoint3 pos)
{
	if (_pKCC)
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
