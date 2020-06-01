#include "verus.h"

using namespace verus;
using namespace verus::Scene;

SceneManager::SceneManager()
{
}

SceneManager::~SceneManager()
{
	Done();
}

void SceneManager::Init()
{
	VERUS_INIT();
}

void SceneManager::Done()
{
	VERUS_DONE(SceneManager);
}

void SceneManager::UpdateParts()
{
}

bool SceneManager::IsDrawingDepth(DrawDepth dd)
{
	if (DrawDepth::automatic == dd)
	{
		VERUS_QREF_ATMO;
		return atmo.GetShadowMap().IsRendering();
	}
	else
		return DrawDepth::yes == dd;
}

bool SceneManager::RayCastingTest(RcPoint3 pointA, RcPoint3 pointB, void* pBlock,
	PPoint3 pPoint, PVector3 pNormal, const float* pRadius, Physics::Group mask)
{
	//VERUS_RT_ASSERT(!pBlock || *pBlock);
	VERUS_QREF_BULLET;
	btVector3 from(pointA.Bullet()), to(pointB.Bullet());
	if (pPoint || pNormal)
	{
		if (pRadius)
		{
			btSphereShape sphere(*pRadius);
			btTransform trA, trB;
			trA.setIdentity();
			trB.setIdentity();
			trA.setOrigin(pointA.Bullet());
			trB.setOrigin(pointB.Bullet());
			btCollisionWorld::ClosestConvexResultCallback ccrc(from, to);
			ccrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->convexSweepTest(&sphere, trA, trB, ccrc);
			if (ccrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(ccrc.m_hitCollisionObject->getUserPointer());
				//if (pBlock && p->UserPtr_GetType() == +NodeType::block)
				//	pBlock->Attach(static_cast<PBlock>(p));
				if (pPoint)
					*pPoint = ccrc.m_hitPointWorld;
				if (pNormal)
					*pNormal = ccrc.m_hitNormalWorld;
				return true;
			}
			else
				return false;
		}
		else
		{
			btCollisionWorld::ClosestRayResultCallback crrc(from, to);
			crrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->rayTest(from, to, crrc);
			if (crrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(crrc.m_collisionObject->getUserPointer());
				//if (pBlock && p->UserPtr_GetType() == +NodeType::block)
				//	pBlock->Attach(static_cast<PBlock>(p));
				if (pPoint)
					*pPoint = crrc.m_hitPointWorld;
				if (pNormal)
					*pNormal = crrc.m_hitNormalWorld;
				return true;
			}
			else
				return false;
		}
	}
	else // Point/normal not required?
	{
		struct AnyRayResultCallback : public btCollisionWorld::RayResultCallback
		{
			btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
			{
				m_closestHitFraction = 0;
				m_collisionObject = rayResult.m_collisionObject;
				return 0;
			}
		} arrc;
		arrc.m_collisionFilterMask = +mask;
		bullet.GetWorld()->rayTest(from, to, arrc);
		if (arrc.hasHit())
		{
			Physics::PUserPtr p = static_cast<Physics::PUserPtr>(arrc.m_collisionObject->getUserPointer());
			//if (pBlock && p->UserPtr_GetType() == +NodeType::block)
			//	pBlock->Attach(static_cast<PBlock>(p));
			return true;
		}
		else
			return false;
	}
	return false;
}
