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
	VERUS_QREF_CONST_SETTINGS;

	_pCollisionConfiguration = new(_pCollisionConfiguration.GetData()) btDefaultCollisionConfiguration();
	_pDispatcher = new(_pDispatcher.GetData()) btCollisionDispatcher(_pCollisionConfiguration.Get());
	_pBroadphaseInterface = new(_pBroadphaseInterface.GetData()) btDbvtBroadphase();
	_pConstraintSolver = new(_pConstraintSolver.GetData()) btSequentialImpulseConstraintSolver();

	_pBroadphaseInterface->getOverlappingPairCache()->setInternalGhostPairCallback(&_ghostPairCallback);

	_pDiscreteDynamicsWorld = new(_pDiscreteDynamicsWorld.GetData()) btDiscreteDynamicsWorld(
		_pDispatcher.Get(),
		_pBroadphaseInterface.Get(),
		_pConstraintSolver.Get(),
		_pCollisionConfiguration.Get());

	_pStaticPlaneShape = new(_pStaticPlaneShape.GetData()) btStaticPlaneShape(btVector3(0, 1, 0), 0);

	if (settings._physicsSupportDebugDraw)
	{
		_pDiscreteDynamicsWorld->setDebugDrawer(&_debugDraw);
		SetDebugDrawMode(DebugDrawMode::basic);
	}

	// See: http://bulletphysics.org/Bullet/phpBB3/viewtopic.php?t=6773:
	_pDiscreteDynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.0001f;
}

void Bullet::Done()
{
	EnableDebugPlane(false);
	DeleteAllCollisionObjects();

	_pStaticPlaneShape.Delete();
	_pDiscreteDynamicsWorld.Delete();
	_pConstraintSolver.Delete();
	_pBroadphaseInterface.Delete();
	_pDispatcher.Delete();
	_pCollisionConfiguration.Delete();

	VERUS_DONE(Bullet);
}

btRigidBody* Bullet::AddNewRigidBody(
	float mass,
	const btTransform& startTransform,
	btCollisionShape* pShape,
	Group group,
	Group mask,
	const btTransform* pCenterOfMassOffset,
	void* pPlacementMotionState,
	void* pPlacementRigidBody)
{
	btAssert(!pShape || pShape->getShapeType() != INVALID_SHAPE_PROXYTYPE);

	const bool dynamic = (mass != 0);

	btVector3 localInertia(0, 0, 0);
	if (dynamic)
		pShape->calculateLocalInertia(mass, localInertia);

	btDefaultMotionState* pMotionState = nullptr;
	if (pPlacementMotionState)
		pMotionState = new(pPlacementMotionState) btDefaultMotionState(startTransform, pCenterOfMassOffset ? *pCenterOfMassOffset : btTransform::getIdentity());
	else
		pMotionState = new btDefaultMotionState(startTransform, pCenterOfMassOffset ? *pCenterOfMassOffset : btTransform::getIdentity());
	btRigidBody::btRigidBodyConstructionInfo rbci(mass, pMotionState, pShape, localInertia);
	rbci.m_linearDamping = 0.01f;
	rbci.m_angularDamping = 0.01f;
	rbci.m_rollingFriction = 0.001f;
	rbci.m_spinningFriction = 0.001f;
	btRigidBody* pRigidBody = nullptr;
	if (pPlacementRigidBody)
		pRigidBody = new(pPlacementRigidBody) btRigidBody(rbci);
	else
		pRigidBody = new btRigidBody(rbci);
	_pDiscreteDynamicsWorld->addRigidBody(pRigidBody, +group, +mask);

	return pRigidBody;
}

btRigidBody* Bullet::AddNewRigidBody(
	RLocalRigidBody localRigidBody,
	float mass,
	const btTransform& startTransform,
	btCollisionShape* pShape,
	Group group,
	Group mask,
	const btTransform* pCenterOfMassOffset)
{
	btAssert(!pShape || pShape->getShapeType() != INVALID_SHAPE_PROXYTYPE);

	const bool dynamic = (mass != 0);

	btVector3 localInertia(0, 0, 0);
	if (dynamic)
		pShape->calculateLocalInertia(mass, localInertia);

	btDefaultMotionState* pMotionState = new(localRigidBody.GetDefaultMotionStateData())
		btDefaultMotionState(startTransform, pCenterOfMassOffset ? *pCenterOfMassOffset : btTransform::getIdentity());
	btRigidBody::btRigidBodyConstructionInfo rbci(mass, pMotionState, pShape, localInertia);
	rbci.m_linearDamping = 0.01f;
	rbci.m_angularDamping = 0.01f;
	rbci.m_rollingFriction = 0.001f;
	rbci.m_spinningFriction = 0.001f;
	btRigidBody* pRigidBody = new(localRigidBody.GetRigidBodyData()) btRigidBody(rbci);
	_pDiscreteDynamicsWorld->addRigidBody(pRigidBody, +group, +mask);

	return pRigidBody;
}

void Bullet::DeleteAllCollisionObjects()
{
	if (!_pDiscreteDynamicsWorld.Get())
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
	if (!_pDiscreteDynamicsWorld.Get())
		return;
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;
	_pDiscreteDynamicsWorld->stepSimulation(_pauseSimulation ? 0 : dt, s_defaultMaxSubSteps);
}

void Bullet::DebugDraw()
{
	if (!_pDiscreteDynamicsWorld.Get() || !_pDiscreteDynamicsWorld->getDebugDrawer())
		return;

	VERUS_QREF_DD;
	dd.Begin(CGI::DebugDraw::Type::lines, nullptr, false);
	_pDiscreteDynamicsWorld->debugDrawWorld();
	dd.End();
}

void Bullet::SetDebugDrawMode(DebugDrawMode mode)
{
	if (!_pDiscreteDynamicsWorld->getDebugDrawer())
		return;

	switch (mode)
	{
	case DebugDrawMode::none:
	{
		_pDiscreteDynamicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_NoDebug);
	}
	break;
	case DebugDrawMode::basic:
	{
		_pDiscreteDynamicsWorld->getDebugDrawer()->setDebugMode(
			btIDebugDraw::DBG_DrawWireframe |
			btIDebugDraw::DBG_DrawConstraints |
			btIDebugDraw::DBG_DrawConstraintLimits |
			btIDebugDraw::DBG_DrawFrames);
	}
	break;
	}
}

void Bullet::EnableDebugPlane(bool b)
{
	if (b == !!_pStaticPlaneRigidBody.Get())
		return;

	if (b)
	{
		btTransform tr;
		tr.setIdentity();
		_pStaticPlaneRigidBody = AddNewRigidBody(_pStaticPlaneRigidBody, 0, tr, _pStaticPlaneShape.Get(), Group::sceneBounds);
		_pStaticPlaneRigidBody->setFriction(GetFriction(Material::wood));
		_pStaticPlaneRigidBody->setRestitution(GetRestitution(Material::wood));
	}
	else
	{
		_pDiscreteDynamicsWorld->removeRigidBody(_pStaticPlaneRigidBody.Get());
		_pStaticPlaneRigidBody.Delete();
	}
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
