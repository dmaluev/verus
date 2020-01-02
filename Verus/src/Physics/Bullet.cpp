#include "verus.h"

using namespace verus;
using namespace verus::Physics;

Bullet::Bullet()
{
}

Bullet::~Bullet()
{
	Done();
}

void Bullet::Init()
{
	VERUS_INIT();

	_pCollisionConfiguration = new btDefaultCollisionConfiguration();
	_pDispatcher = new btCollisionDispatcher(_pCollisionConfiguration);
	_pBroadphaseInterface = new btDbvtBroadphase();
	_pConstraintSolver = new btSequentialImpulseConstraintSolver();

	_pBroadphaseInterface->getOverlappingPairCache()->setInternalGhostPairCallback(&_ghostPairCallback);

	_pDiscreteDynamicsWorld = new btDiscreteDynamicsWorld(
		_pDispatcher,
		_pBroadphaseInterface,
		_pConstraintSolver,
		_pCollisionConfiguration);

#ifdef VERUS_RELEASE_DEBUG
	_pDiscreteDynamicsWorld->setDebugDrawer(&_debugDraw);
	EnableDebugDraw(true);
#endif

	// See: http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=6773:
	_pDiscreteDynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.0001f;
}

void Bullet::Done()
{
	DeleteAllCollisionObjects();

	if (_pDiscreteDynamicsWorld)
	{
		delete _pDiscreteDynamicsWorld;
		_pDiscreteDynamicsWorld = nullptr;
	}

	if (_pConstraintSolver)
	{
		delete _pConstraintSolver;
		_pConstraintSolver = nullptr;
	}

	if (_pBroadphaseInterface)
	{
		delete _pBroadphaseInterface;
		_pBroadphaseInterface = nullptr;
	}

	if (_pDispatcher)
	{
		delete _pDispatcher;
		_pDispatcher = nullptr;
	}

	if (_pCollisionConfiguration)
	{
		delete _pCollisionConfiguration;
		_pCollisionConfiguration = nullptr;
	}

	VERUS_DONE(Bullet);
}

btRigidBody* Bullet::AddNewRigidBody(
	float mass,
	const btTransform& startTransform,
	btCollisionShape* pShape,
	short group,
	short mask,
	const btTransform* pCenterOfMassOffset)
{
	btAssert(!pShape || pShape->getShapeType() != INVALID_SHAPE_PROXYTYPE);

	const bool dynamic = (mass != 0);

	btVector3 localInertia(0, 0, 0);
	if (dynamic)
		pShape->calculateLocalInertia(mass, localInertia);

	btDefaultMotionState* pMotionState = new btDefaultMotionState(startTransform, pCenterOfMassOffset ? *pCenterOfMassOffset : btTransform::getIdentity());
	btRigidBody::btRigidBodyConstructionInfo rbci(mass, pMotionState, pShape, localInertia);
	rbci.m_linearDamping = 0.01f;
	rbci.m_angularDamping = 0.01f;
	rbci.m_rollingFriction = 0.001f;
	rbci.m_spinningFriction = 0.001f;
	btRigidBody* pBody = new btRigidBody(rbci);
	_pDiscreteDynamicsWorld->addRigidBody(pBody, group, mask);

	return pBody;
}

void Bullet::DeleteAllCollisionObjects()
{
	if (!_pDiscreteDynamicsWorld)
		return;

	for (int i = _pDiscreteDynamicsWorld->getNumCollisionObjects() - 1; i >= 0; --i)
	{
		btCollisionObject* pObject = _pDiscreteDynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* pBody = btRigidBody::upcast(pObject);
		if (pBody && pBody->getMotionState())
			delete pBody->getMotionState();
		_pDiscreteDynamicsWorld->removeCollisionObject(pObject);
		delete pObject;
	}
}

void Bullet::Simulate()
{
	if (!_pDiscreteDynamicsWorld)
		return;
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;
	_pDiscreteDynamicsWorld->stepSimulation(dt, s_defaultMaxSubSteps);
}

void Bullet::DebugDraw()
{
#ifdef VERUS_RELEASE_DEBUG
	if (!_pDiscreteDynamicsWorld)
		return;

	VERUS_QREF_DD;
	dd.Begin(CGI::DebugDraw::Type::lines, nullptr, false);
	_pDiscreteDynamicsWorld->debugDrawWorld();
	dd.End();
#endif
}

void Bullet::EnableDebugDraw(bool b)
{
#ifdef VERUS_RELEASE_DEBUG
	if (b)
	{
		_pDiscreteDynamicsWorld->getDebugDrawer()->setDebugMode(
			btIDebugDraw::DBG_DrawWireframe |
			btIDebugDraw::DBG_DrawConstraints |
			btIDebugDraw::DBG_DrawConstraintLimits |
			btIDebugDraw::DBG_DrawFrames);
	}
	else
	{
		_pDiscreteDynamicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_NoDebug);
	}
#endif
}

float Bullet::GetFriction(Material m)
{
	// See: https://www.thoughtspike.com/friction-coefficients-for-bullet-physics/
	switch (m)
	{
	case Material::brick: return 0.9f;
	case Material::glass: return 0.5f;
	case Material::leather: return 0.8f;
	case Material::metal: return 0.84f;
	case Material::plastic: return 0.6f;
	case Material::rubber: return 0.95f;
	case Material::sand: return 0.4f;
	case Material::stone: return 0.75f;
	case Material::wood: return 0.7f;
	}
	return 0;
}

float Bullet::GetRestitution(Material m)
{
	// See: http://hypertextbook.com/facts/2006/restitution.shtml
	switch (m)
	{
	case Material::brick: return 0.77f;
	case Material::glass: return 0.79f;
	case Material::leather: return 0.724f;
	case Material::metal: return 0.72f;
	case Material::plastic: return 0.83f;
	case Material::rubber: return 1;
	case Material::sand: return 0.12f;
	case Material::stone: return 0.8f;
	case Material::wood: return 0.73f;
	}
	return 0;
}
