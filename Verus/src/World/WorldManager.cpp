// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

WorldManager::WorldManager()
{
	VERUS_ZERO_MEM(_visibleCountPerType);
}

WorldManager::~WorldManager()
{
	Done();
}

void WorldManager::Init(RcDesc desc)
{
	VERUS_INIT();

	_vNodes.reserve(1024);
	_vVisibleNodes.reserve(1024);

	SetAllCameras(desc._pCamera);

	_pPickingShape = new(_pPickingShape.GetData()) btBoxShape(btVector3(
		_pickingShapeHalfExtent,
		_pickingShapeHalfExtent,
		_pickingShapeHalfExtent));

	_worldSide = desc._worldSide;
	const float sideHalf = desc._worldSide * 0.5f;
	Math::Bounds bounds;
	bounds.Set(
		Vector3(-sideHalf, -sideHalf, -sideHalf),
		Vector3(+sideHalf, +sideHalf, +sideHalf));
	const Vector3 limit(sideHalf / 14, sideHalf / 14, sideHalf / 14);
	_octree.Done();
	_octree.Init(bounds, limit);
	_octree.SetDelegate(this);
}

void WorldManager::Done()
{
	DeleteAllNodes();

	_pPickingShape.Delete();

	VERUS_DONE(WorldManager);
}

void WorldManager::ResetInstanceCount()
{
	for (auto& x : TStoreModelNodes::_map)
		x.second.GetMesh().ResetInstanceCount();
	for (auto& x : TStoreTerrainNodes::_list)
		x.GetTerrain().ResetInstanceCount();
}

void WorldManager::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	for (auto pNode : _vNodes)
		pNode->Update();
}

void WorldManager::UpdateParts()
{
	const RcPoint3 headPos = _pHeadCamera->GetEyePosition();
	for (auto& blockNode : TStoreBlockNodes::_list)
	{
		const float distSq = VMath::distSqr(blockNode.GetPosition(), headPos);
		const float part = MaterialManager::ComputePart(distSq, blockNode.GetBounds().GetAverageSize() * 0.5f);
		blockNode.GetMaterial()->IncludePart(part);
	}
}

void WorldManager::Layout()
{
	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;

	// Allocate enough space:
	if (_vVisibleNodes.size() != _vNodes.size())
		_vVisibleNodes.resize(_vNodes.size());

	_visibleCount = 0;
	VERUS_ZERO_MEM(_visibleCountPerType);

	// <Traverse>
	PCamera pPrevPassCamera = nullptr;
	// For CSM we need to create geometry beyond the view frustum (1st slice):
	if (settings._sceneShadowQuality >= App::Settings::Quality::high && atmo.GetShadowMapBaker().IsBaking())
	{
		PCamera pPassCameraCSM = atmo.GetShadowMapBaker().GetPassCameraCSM();
		if (pPassCameraCSM)
			pPrevPassCamera = SetPassCamera(pPassCameraCSM);
	}
	_octree.TraverseVisible(_pPassCamera->GetFrustum());
	// Back to original camera:
	if (pPrevPassCamera)
		SetPassCamera(pPrevPassCamera);
	// </Traverse>

	VERUS_RT_ASSERT(!_visibleCountPerType[+NodeType::unknown]);
	std::sort(_vVisibleNodes.begin(), _vVisibleNodes.begin() + _visibleCount, [](PBaseNode pA, PBaseNode pB)
		{
			// Sort by node type?
			const NodeType typeA = pA->GetType();
			const NodeType typeB = pB->GetType();
			if (typeA != typeB)
				return typeA < typeB;

			// Both are blocks?
			if (NodeType::block == typeA)
			{
				PBlockNode pBlockNodeA = static_cast<PBlockNode>(pA);
				PBlockNode pBlockNodeB = static_cast<PBlockNode>(pB);
				MaterialPtr materialA = pBlockNodeA->GetMaterial();
				MaterialPtr materialB = pBlockNodeB->GetMaterial();

				if (materialA && materialB) // A and B have materials, compare them.
				{
					const bool ab = *materialA < *materialB;
					const bool ba = *materialB < *materialA;
					if (ab || ba)
						return ab;
				}
				else if (materialA) // A is with material, B is without, so A goes first.
				{
					return true;
				}
				else if (materialB) // A is without material, B is with, so B goes first.
				{
					return false;
				}

				// Equal materials or none have any material, compare models used:
				ModelNodePtr modelNodeA = pBlockNodeA->GetModelNode();
				ModelNodePtr modelNodeB = pBlockNodeB->GetModelNode();
				if (modelNodeA != modelNodeB)
					return modelNodeA < modelNodeB;
			}

			// Both are lights?
			if (NodeType::light == typeA)
			{
				PLightNode pLightNodeA = static_cast<PLightNode>(pA);
				PLightNode pLightNodeB = static_cast<PLightNode>(pB);
				const CGI::LightType lightTypeA = pLightNodeA->GetLightType();
				const CGI::LightType lightTypeB = pLightNodeB->GetLightType();
				if (lightTypeA != lightTypeB)
					return lightTypeA < lightTypeB;
			}

			// Draw same node types front-to-back:
			return pA->GetDistToHeadSq() < pB->GetDistToHeadSq();
		});

	for (auto& x : TStoreTerrainNodes::_list)
		x.Layout();
}

void WorldManager::Draw()
{
	if (!_visibleCountPerType[+NodeType::block])
		return;

	VERUS_QREF_RENDERER;

	ModelNodePtr modelNode;
	MaterialPtr material;
	bool bindPipeline = true;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Mesh::GetShader();

	const int begin = FindOffsetFor(NodeType::block);
	const int end = begin + _visibleCountPerType[+NodeType::block];
	shader->BeginBindDescriptors();
	for (int i = begin; i <= end; ++i)
	{
		if (i == end)
		{
			if (modelNode)
				modelNode->Draw(cb); // Finish with this one.
			break;
		}

		PBaseNode pNode = _vVisibleNodes[i];
		PBlockNode pBlockNode = static_cast<PBlockNode>(pNode);
		ModelNodePtr nextModelNode = pBlockNode->GetModelNode();
		MaterialPtr nextMaterial = pBlockNode->GetMaterial();

		if (!nextModelNode->IsLoaded() || !nextMaterial->IsLoaded())
			continue; // Not ready.

		const bool changeModelNode = nextModelNode != modelNode;
		const bool changeMaterial = nextMaterial != material;
		if (changeModelNode || changeMaterial)
		{
			if (modelNode)
				modelNode->Draw(cb); // Finish with this one.
		}
		if (changeModelNode)
		{
			modelNode = nextModelNode;
			modelNode->MarkFirstInstance();
			if (bindPipeline)
			{
				bindPipeline = false;
				modelNode->BindPipeline(cb);
				cb->BindDescriptors(shader, 0);
			}
			modelNode->BindGeo(cb);
			cb->BindDescriptors(shader, 2);
		}
		if (changeMaterial)
		{
			modelNode->MarkFirstInstance();
			material = nextMaterial;
			material->UpdateMeshUniformBuffer();
			cb->BindDescriptors(shader, 1, material->GetComplexSetHandle());
		}

		if (modelNode)
			modelNode->PushInstance(pBlockNode->GetTransform(), pBlockNode->GetColor());
	}
	shader->EndBindDescriptors();
}

void WorldManager::DrawSimple(DrawSimpleMode mode)
{
	if (!_visibleCountPerType[+NodeType::block])
		return;

	VERUS_QREF_RENDERER;

	ModelNodePtr modelNode;
	MaterialPtr material;
	bool bindPipeline = true;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Mesh::GetSimpleShader();

	const int begin = FindOffsetFor(NodeType::block);
	const int end = begin + _visibleCountPerType[+NodeType::block];
	shader->BeginBindDescriptors();
	for (int i = begin; i <= end; ++i)
	{
		if (i == end)
		{
			if (modelNode)
				modelNode->Draw(cb); // Finish with this one.
			break;
		}

		PBaseNode pNode = _vVisibleNodes[i];
		PBlockNode pBlockNode = static_cast<PBlockNode>(pNode);
		ModelNodePtr nextModelNode = pBlockNode->GetModelNode();
		MaterialPtr nextMaterial = pBlockNode->GetMaterial();

		if (!nextModelNode->IsLoaded() || !nextMaterial->IsLoaded())
			continue; // Not ready.

		const bool changeModelNode = nextModelNode != modelNode;
		const bool changeMaterial = nextMaterial != material;
		if (changeModelNode || changeMaterial)
		{
			if (modelNode)
				modelNode->Draw(cb); // Finish with this one.
		}
		if (changeModelNode)
		{
			modelNode = nextModelNode;
			modelNode->MarkFirstInstance();
			if (bindPipeline)
			{
				bindPipeline = false;
				modelNode->BindPipelineSimple(mode, cb);
				cb->BindDescriptors(shader, 0);
			}
			modelNode->BindGeo(cb);
			cb->BindDescriptors(shader, 2);
		}
		if (changeMaterial)
		{
			modelNode->MarkFirstInstance();
			material = nextMaterial;
			material->UpdateMeshUniformBufferSimple();
			cb->BindDescriptors(shader, 1, material->GetComplexSetHandleSimple());
		}

		if (modelNode)
			modelNode->PushInstance(pBlockNode->GetTransform(), pBlockNode->GetColor());
	}
	shader->EndBindDescriptors();
}

void WorldManager::DrawTerrainNodes(Terrain::RcDrawDesc dd)
{
	for (auto& x : TStoreTerrainNodes::_list)
	{
		if (!x.IsDisabled())
			x.GetTerrain().Draw(dd);
	}
}

void WorldManager::DrawTerrainNodesSimple(DrawSimpleMode mode)
{
	for (auto& x : TStoreTerrainNodes::_list)
	{
		if (!x.IsDisabled())
			x.GetTerrain().DrawSimple(mode);
	}
}

void WorldManager::DrawLights()
{
	if (!_visibleCountPerType[+NodeType::light])
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_WU;

	CGI::LightType type = CGI::LightType::none;
	PMesh pMesh = nullptr;
	bool bindPipeline = true;

	auto& ds = renderer.GetDS();
	auto cb = renderer.GetCommandBuffer();

	auto DrawMesh = [cb](PMesh pMesh)
	{
		if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
		{
			pMesh->UpdateInstanceBuffer();
			cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
		}
	};

	const int begin = FindOffsetFor(NodeType::light);
	const int end = begin + _visibleCountPerType[+NodeType::light];
	for (int i = begin; i <= end; ++i)
	{
		if (i == end)
		{
			DrawMesh(pMesh);
			break;
		}

		PBaseNode pNode = _vVisibleNodes[i];
		PLightNode pLightNode = static_cast<PLightNode>(pNode);
		const CGI::LightType nextType = pLightNode->GetLightType();

		if (nextType != type)
		{
			DrawMesh(pMesh);

			type = nextType;
			ds.OnNewLightType(cb, type);

			pMesh = (type != CGI::LightType::none) ? &wu.GetDeferredLights().Get(type) : nullptr;

			if (pMesh)
			{
				pMesh->MarkFirstInstance();
				pMesh->BindGeo(cb, (1 << 0) | (1 << 4));
				pMesh->CopyPosDeqScale(&ds.GetUbPerMeshVS()._posDeqScale.x);
				pMesh->CopyPosDeqBias(&ds.GetUbPerMeshVS()._posDeqBias.x);
				ds.BindDescriptorsPerMeshVS(cb);
			}
		}

#ifdef VERUS_RELEASE_DEBUG
		if (CGI::LightType::dir == type) // Must not be scaled!
		{
			RcTransform3 matW = pLightNode->GetTransform();
			const Vector3 s = matW.GetScale();
			VERUS_RT_ASSERT(glm::all(glm::epsilonEqual(s.GLM(), glm::vec3(1), glm::vec3(VERUS_FLOAT_THRESHOLD))));
		}
#endif

		if (pMesh)
			pMesh->PushInstance(pLightNode->GetTransform(), pLightNode->GetInstData());
	}
}

void WorldManager::DrawTransparent()
{
	for (auto& x : TStoreParticlesNodes::_map)
		x.second.Draw();
}

void WorldManager::DrawSelectedBounds()
{
	VERUS_FOR(i, _visibleCount)
	{
		PBaseNode pNode = _vVisibleNodes[i];
		if (pNode->IsSelected())
			pNode->DrawBounds();
	}
}

void WorldManager::DrawSelectedRelationshipLines()
{
	for (auto pNode : _vNodes)
	{
		if (pNode->IsSelected())
			pNode->DrawRelationshipLines();
	}
}

void WorldManager::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	VERUS_FOR(i, _visibleCount)
	{
		PBaseNode pNode = _vVisibleNodes[i];
		pNode->DrawEditorOverlays(flags);
	}
	if (flags & DrawEditorOverlaysFlags::extras)
	{
		for (auto pNode : _vNodes)
			pNode->DrawEditorOverlays(DrawEditorOverlaysFlags::unbounded);
	}
}

bool WorldManager::IsDrawingDepth(DrawDepth dd)
{
	if (DrawDepth::automatic == dd)
	{
		VERUS_QREF_ATMO;
		return atmo.GetShadowMapBaker().IsBaking();
	}
	else
		return DrawDepth::yes == dd;
}

int WorldManager::GetEditorOverlaysAlpha(int originalAlpha, float distSq, float fadeDistSq)
{
	const float alpha = Math::Clamp<float>(1 - (distSq - fadeDistSq) * (1 / fadeDistSq), 0, 1);
	return static_cast<int>(originalAlpha * alpha);
}

Continue WorldManager::Octree_ProcessNode(void* pToken, void* pUser)
{
	PBaseNode pNode = static_cast<PBaseNode>(pToken);
	if (pNode->IsDisabled())
		return Continue::yes;
	_vVisibleNodes[_visibleCount++] = pNode;
	_visibleCountPerType[+pNode->GetType()]++;
	return Continue::yes;
}

bool WorldManager::RayTest(RcPoint3 pointA, RcPoint3 pointB, Physics::Group mask)
{
	return RayTestEx(pointA, pointB, nullptr, nullptr, nullptr, nullptr, mask);
}

bool WorldManager::RayTestEx(RcPoint3 pointA, RcPoint3 pointB, PBlockNodePtr pBlock,
	PPoint3 pPoint, PVector3 pNormal, const float* pRadius, Physics::Group mask)
{
	VERUS_RT_ASSERT(!pBlock || *pBlock);
	VERUS_QREF_BULLET;
	if (!memcmp(pointA.ToPointer(), pointB.ToPointer(), sizeof(float) * 3))
		return false;
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
			ccrc.m_collisionFilterGroup = +Physics::Group::ray;
			ccrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->convexSweepTest(&sphere, trA, trB, ccrc);
			if (ccrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(ccrc.m_hitCollisionObject->getUserPointer());
				if (pBlock && p->UserPtr_GetType() == +NodeType::block)
					pBlock->Attach(static_cast<PBlockNode>(p));
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
			crrc.m_collisionFilterGroup = +Physics::Group::ray;
			crrc.m_collisionFilterMask = +mask;
			bullet.GetWorld()->rayTest(from, to, crrc);
			if (crrc.hasHit())
			{
				Physics::PUserPtr p = static_cast<Physics::PUserPtr>(crrc.m_collisionObject->getUserPointer());
				if (pBlock && p->UserPtr_GetType() == +NodeType::block)
					pBlock->Attach(static_cast<PBlockNode>(p));
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
		arrc.m_collisionFilterGroup = +Physics::Group::ray;
		arrc.m_collisionFilterMask = +mask;
		bullet.GetWorld()->rayTest(from, to, arrc);
		if (arrc.hasHit())
		{
			Physics::PUserPtr p = static_cast<Physics::PUserPtr>(arrc.m_collisionObject->getUserPointer());
			if (pBlock && p->UserPtr_GetType() == +NodeType::block)
				pBlock->Attach(static_cast<PBlockNode>(p));
			return true;
		}
		else
			return false;
	}
	return false;
}

Matrix3 WorldManager::GetBasisAt(RcPoint3 point, Physics::Group mask) const
{
	VERUS_QREF_BULLET;
	const btVector3 from = point.Bullet();
	const btVector3 to = from - btVector3(0, 100, 0);
	btCollisionWorld::ClosestRayResultCallback crrc(from, to);
	crrc.m_collisionFilterGroup = +Physics::Group::ray;
	crrc.m_collisionFilterMask = +mask;
	bullet.GetWorld()->rayTest(from, to, crrc);
	if (crrc.hasHit())
	{
		glm::vec3 nreal = Vector3(crrc.m_hitNormalWorld).GLM(), nrm(0, 1, 0), tan(1, 0, 0), bin(0, 0, 1);
		const float angle = glm::angle(nrm, nreal);
		VERUS_RT_ASSERT(angle <= VERUS_PI * 0.5f);
		if (angle >= Math::ToRadians(1))
		{
			const glm::vec3 axis = glm::normalize(glm::cross(nrm, nreal));
			const glm::quat q = glm::angleAxis(angle, axis);
			const glm::mat3 mat = glm::mat3_cast(q);
			tan = mat * tan;
			bin = mat * bin;
		}
		return Matrix3(Vector3(tan), Vector3(nreal), Vector3(bin));
	}
	else
		return Matrix3::identity();
}

btBoxShape* WorldManager::GetPickingShape()
{
	return _pPickingShape.Get();
}

String WorldManager::EnsureUniqueName(CSZ name, PcBaseNode pSkipNode)
{
	// Compact name:
	const char* s = strrchr(name, ':');
	s = s ? s + 1 : name;
	const char* e = strrchr(name, '.');
	if (!e)
		e = name + strlen(name);

	// Existing index:
	const char* u = nullptr;
	for (const char* p = s; p < e; ++p)
	{
		if ('_' == *p)
			u = p;
	}
	int id = 0;
	if (u)
		id = atoi(u + 1);
	if (id)
		e = u;

	// Unique name:
	String test;
	do
	{
		test = !id ? String(s, e) : String(s, e) + "_" + std::to_string(id);
		for (auto pNode : _vNodes)
		{
			if (pNode->GetName() == _C(test) && pNode != pSkipNode)
				test.clear();
		}
		id++;
	} while (test.empty());

	return test;
}

void WorldManager::SortNodes()
{
	if (_recursionDepth > 0)
		return;
	std::sort(_vNodes.begin(), _vNodes.end(), [](PcBaseNode pNodeA, PcBaseNode pNodeB)
		{
			if (!pNodeA)
				return true;
			if (!pNodeB)
				return false;

			// Find a pair of nodes which have the same depth, but are different:
			int depth = Math::Max(pNodeA->GetDepth(), pNodeB->GetDepth());
			PcBaseNode pSavedNodeA = nullptr;
			PcBaseNode pSavedNodeB = nullptr;
			while (depth >= 0)
			{
				PcBaseNode pCurrentNodeA = (depth == pNodeA->GetDepth()) ? pNodeA : nullptr;
				PcBaseNode pCurrentNodeB = (depth == pNodeB->GetDepth()) ? pNodeB : nullptr;
				if (pCurrentNodeA != pCurrentNodeB)
				{
					pSavedNodeA = pCurrentNodeA;
					pSavedNodeB = pCurrentNodeB;
				}
				if (pCurrentNodeA)
					pNodeA = pNodeA->GetParent();
				if (pCurrentNodeB)
					pNodeB = pNodeB->GetParent();
				depth--;
			}
			pNodeA = pSavedNodeA;
			pNodeB = pSavedNodeB;

			if (!pNodeA)
				return true;
			if (!pNodeB)
				return false;

			const int priorityA = GetNodeTypePriority(pNodeA->GetType());
			const int priorityB = GetNodeTypePriority(pNodeB->GetType());
			if (priorityA != priorityB)
				return priorityA < priorityB;

			return pNodeA->GetName() < pNodeB->GetName();
		});
}

int WorldManager::FindOffsetFor(NodeType type) const
{
	int ret = 0;
	VERUS_FOR(i, +type)
		ret += _visibleCountPerType[i];
	return ret;
}

int WorldManager::GetNodeCount(int* pPerType, bool excludeGenerated) const
{
	if (pPerType)
	{
		VERUS_FOR(i, +NodeType::count)
			pPerType[i] = 0;
		for (auto pNode : _vNodes)
		{
			if (excludeGenerated && pNode->IsGenerated())
				continue;
			pPerType[+pNode->GetType()]++;
		}
	}
	if (excludeGenerated)
	{
		int ret = 0;
		for (auto pNode : _vNodes)
		{
			if (!pNode->IsGenerated())
				ret++;
		}
		return ret;
	}
	else
	{
		return static_cast<int>(_vNodes.size());
	}
}

int WorldManager::GetIndexOf(PcBaseNode pTargetNode, bool excludeGenerated) const
{
	int offset = 0;
	const int nodeCount = GetNodeCount();
	VERUS_FOR(i, nodeCount)
	{
		PBaseNode pNode = _vNodes[i];
		if (excludeGenerated && pNode->IsGenerated())
		{
			offset++;
			continue;
		}
		if (pNode == pTargetNode)
			return i - offset;
	}
	return -1;
}

PBaseNode WorldManager::GetNodeByIndex(int index) const
{
	if (index >= 0 && index < _vNodes.size())
		return _vNodes[index];
	return nullptr;
}

bool WorldManager::IsAncestorOf(PcBaseNode pNodeA, PcBaseNode pNodeB)
{
	if (!pNodeA || !pNodeB)
		return false;
	PcBaseNode pParent = pNodeB->GetParent();
	while (pParent)
	{
		if (pParent == pNodeA)
			return true;
		pParent = pParent->GetParent();
	}
	return false;
}

bool WorldManager::HasAncestorOfType(NodeType type, PcBaseNode pNode)
{
	if (!pNode)
		return false;
	PcBaseNode pParent = pNode->GetParent();
	while (pParent)
	{
		if (pParent->GetType() == type)
			return true;
		pParent = pParent->GetParent();
	}
	return false;
}

bool WorldManager::IsValidNode(PcBaseNode pTargetNode)
{
	for (auto pNode : _vNodes)
	{
		if (pNode == pTargetNode)
			return true;
	}
	return false;
}

void WorldManager::DeleteNode(PBaseNode pTargetNode, bool hierarchy)
{
	if (IsValidNode(pTargetNode))
		DeleteNode(pTargetNode->GetType(), _C(pTargetNode->GetName()), hierarchy);
}

void WorldManager::DeleteNode(NodeType type, CSZ name, bool hierarchy)
{
	_recursionDepth++;

	Query query;
	query._name = name;
	query._type = type;
	ForEachNode(query, [this, hierarchy](RBaseNode node)
		{
			if (!hierarchy)
			{
				for (auto pNode : _vNodes)
				{
					if (pNode->GetParent() == &node)
						pNode->SetParent(node.GetParent());
				}
			}

			BroadcastOnNodeDeleted(&node, false, hierarchy);
			bool deleted = true;
			switch (node.GetType())
			{
			case NodeType::base:         TStoreBaseNodes::Delete(static_cast<PBlockNode>(&node)); break;
			case NodeType::model:        deleted = TStoreModelNodes::Delete(_C(static_cast<RModelNode>(node).GetURL())); break;
			case NodeType::particles:    deleted = TStoreParticlesNodes::Delete(_C(static_cast<RParticlesNode>(node).GetURL())); break;
			case NodeType::block:        TStoreBlockNodes::Delete(static_cast<PBlockNode>(&node)); break;
			case NodeType::controlPoint: TStoreControlPointNodes::Delete(static_cast<PControlPointNode>(&node)); break;
			case NodeType::emitter:      TStoreEmitterNodes::Delete(static_cast<PEmitterNode>(&node)); break;
			case NodeType::instance:     TStoreInstanceNodes::Delete(static_cast<PInstanceNode>(&node)); break;
			case NodeType::light:        TStoreLightNodes::Delete(static_cast<PLightNode>(&node)); break;
			case NodeType::path:         TStorePathNodes::Delete(static_cast<PPathNode>(&node)); break;
			case NodeType::physics:      TStorePhysicsNodes::Delete(static_cast<PPhysicsNode>(&node)); break;
			case NodeType::prefab:       TStorePrefabNodes::Delete(static_cast<PPrefabNode>(&node)); break;
			case NodeType::shaker:       TStoreShakerNodes::Delete(static_cast<PShakerNode>(&node)); break;
			case NodeType::sound:        TStoreSoundNodes::Delete(static_cast<PSoundNode>(&node)); break;
			case NodeType::terrain:      TStoreTerrainNodes::Delete(static_cast<PTerrainNode>(&node)); break;
			}
			if (deleted)
				std::replace(_vNodes.begin(), _vNodes.end(), &node, static_cast<PBaseNode>(nullptr));
			BroadcastOnNodeDeleted(&node, true, hierarchy);
			return Continue::no;
		});

	_recursionDepth--;

	if (!_recursionDepth)
	{
		_vNodes.erase(std::remove(_vNodes.begin(), _vNodes.end(), nullptr), _vNodes.end());
		_vVisibleNodes.clear();
		_visibleCount = 0;
		VERUS_ZERO_MEM(_visibleCountPerType);
	}
}

void WorldManager::DeleteAllNodes()
{
	while (!_vNodes.empty())
		DeleteNode(_vNodes.back());
}

PBaseNode WorldManager::DuplicateNode(PBaseNode pTargetNode, HierarchyDuplication hierarchyDuplication)
{
	if (hierarchyDuplication & HierarchyDuplication::withProbability)
	{
		const int chance = pTargetNode->GetDictionary().FindInt("PrefabDupP", 100);
		if (static_cast<int>(_random.Next() % 100) >= chance)
			return nullptr;
	}

	_recursionDepth++;

	PBaseNode pNewNode = nullptr;
	BroadcastOnNodeDuplicated(pTargetNode, false, pNewNode, hierarchyDuplication);
	switch (pTargetNode->GetType())
	{
	case NodeType::base: {BaseNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::block: {BlockNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::controlPoint: {ControlPointNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::emitter: {EmitterNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::instance: {InstanceNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::light: {LightNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::path: {PathNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::physics: {PhysicsNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::prefab: {PrefabNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::shaker: {ShakerNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	case NodeType::sound: {SoundNodePtr node; node.Duplicate(*pTargetNode); pNewNode = node.Get(); break; }
	}
	BroadcastOnNodeDuplicated(pTargetNode, true, pNewNode, hierarchyDuplication);

	_recursionDepth--;
	SortNodes();

	if (pNewNode)
		pNewNode->SetGeneratedFlag(hierarchyDuplication & HierarchyDuplication::generated);

	return pNewNode;
}

void WorldManager::EnableAll()
{
	for (auto pNode : _vNodes)
		pNode->Disable(false);
}

void WorldManager::EnableSelected()
{
	for (auto pNode : _vNodes)
	{
		if (pNode->IsSelected())
			pNode->Disable(false);
	}
}

void WorldManager::DisableAll()
{
	for (auto pNode : _vNodes)
		pNode->Disable();
}

void WorldManager::DisableSelected()
{
	for (auto pNode : _vNodes)
	{
		if (pNode->IsSelected())
			pNode->Disable();
	}
}

void WorldManager::SelectAll()
{
	for (auto pNode : _vNodes)
		pNode->Select();
}

void WorldManager::SelectAllInsideSphere(Math::RcSphere sphere)
{
	const float r2 = sphere.GetRadiusSq();
	for (auto pNode : _vNodes)
	{
		if (VMath::distSqr(pNode->GetPosition(), sphere.GetCenter()) < r2)
			pNode->Select();
	}
}

void WorldManager::ClearSelection()
{
	for (auto pNode : _vNodes)
		pNode->Select(false);
}

void WorldManager::InvertSelection()
{
	for (auto pNode : _vNodes)
		pNode->Select(!pNode->IsSelected());
}

int WorldManager::GetSelectedCount()
{
	int ret = 0;
	for (auto pNode : _vNodes)
	{
		if (pNode->IsSelected())
			ret++;
	}
	return ret;
}

void WorldManager::SelectAllChildNodes(PBaseNode pTargetNode)
{
	const int nodeIndex = GetIndexOf(pTargetNode);
	const int nodeDepth = pTargetNode->GetDepth();

	const int nodeCount = GetNodeCount();
	for (int i = nodeIndex + 1; i < nodeCount; ++i)
	{
		PBaseNode pNode = _vNodes[i];
		if (pNode->GetDepth() <= nodeDepth)
			break;
		pNode->Select();
	}
}

void WorldManager::ResetRigidBodyTransforms()
{
	Query query;
	query._type = NodeType::physics;
	ForEachNode(query, [](RBaseNode node)
		{
			static_cast<RPhysicsNode>(node).ResetRigidBodyTransform();
			return Continue::yes;
		});
}

bool WorldManager::Connect2ControlPoints(PControlPointNode pNodeA, PControlPointNode pNodeB)
{
	if (!pNodeA || !pNodeB)
		return false;
	if (pNodeA->GetPathNode() != pNodeB->GetPathNode())
		return false;

	// A to B?
	if (!pNodeA->GetNextControlPoint() && !pNodeB->GetPreviousControlPoint())
	{
		pNodeA->SetNextControlPoint(pNodeB);
		pNodeB->SetPreviousControlPoint(pNodeA);
		return true;
	}
	// B to A?
	if (!pNodeB->GetNextControlPoint() && !pNodeA->GetPreviousControlPoint())
	{
		pNodeB->SetNextControlPoint(pNodeA);
		pNodeA->SetPreviousControlPoint(pNodeB);
		return true;
	}

	return false;
}

void WorldManager::UpdatePrefabInstances(PPrefabNode pPrefabNode, bool sortNodes)
{
	// Find all instances of this prefab:
	Vector<PInstanceNode> vInstanceNodes;
	vInstanceNodes.reserve(GetNodeCount() / 10);
	Query query;
	query._type = NodeType::instance;
	ForEachNode(query, [this, pPrefabNode, &vInstanceNodes](RBaseNode node)
		{
			RInstanceNode instanceNode = static_cast<RInstanceNode>(node);
			if (instanceNode.GetPrefabNode() == pPrefabNode)
				vInstanceNodes.push_back(&instanceNode);
			return Continue::yes;
		});

	// Delete all nodes which are children of these instances:
	bool nodeDeleted = false;
	do
	{
		nodeDeleted = false;
		for (auto pNode : _vNodes)
		{
			for (auto pInstanceNode : vInstanceNodes)
			{
				if (pNode->GetParent() == pInstanceNode)
				{
					DeleteNode(pNode);
					nodeDeleted = true;
					break;
				}
			}
			if (nodeDeleted)
				break;
		}
	} while (nodeDeleted);

	_recursionDepth = 1; // Disable sorting.

	// Find all children of this prefab:
	Vector<PBaseNode> vChildNodes;
	vChildNodes.reserve(GetNodeCount() / 10);
	query.Reset();
	query._pParent = pPrefabNode;
	ForEachNode(query, [this, &vChildNodes](RBaseNode node)
		{
			vChildNodes.push_back(&node);
			return Continue::yes;
		});

	// Duplicate nodes:
	int index = 0;
	for (auto pInstanceNode : vInstanceNodes)
	{
		_random.Seed(pPrefabNode->GetSeed() + index);
		for (auto pChildNode : vChildNodes)
		{
			const HierarchyDuplication hierarchyDuplication =
				HierarchyDuplication::yes |
				HierarchyDuplication::withProbability |
				HierarchyDuplication::generated;
			if (PBaseNode pDuplicatedNode = DuplicateNode(pChildNode, hierarchyDuplication))
			{
				pDuplicatedNode->SetParent(pInstanceNode, true);
				pDuplicatedNode->UpdateRigidBodyTransform();
			}
		}
		index++;
	}

	_recursionDepth = 0; // Enable sorting.
	if (sortNodes)
		SortNodes();
}

void WorldManager::ReplaceSimilarWithInstances(PPrefabNode pPrefabNode)
{
	int prefabNodeIndex = GetIndexOf(pPrefabNode);
	int prefabNodeDepth = pPrefabNode->GetDepth();

	// Select mode.
	// If prefab has one child, replace similar branch with instance.
	// If prefab has multiple children, then search for BaseNode with similar children, and replace it.
	int nodeCount = GetNodeCount();
	int childCount = 0;
	for (int i = prefabNodeIndex + 1; i < nodeCount; ++i)
	{
		PBaseNode pNode = _vNodes[i];
		if (pNode->GetDepth() <= prefabNodeDepth)
			break;
		if (pNode->GetParent() == pPrefabNode)
			childCount++;
		if (childCount >= 2)
			break;
	}
	const bool multiMode = (childCount >= 2);

	bool nodeDeleted = false;
	do
	{
		nodeDeleted = false;
		prefabNodeIndex = GetIndexOf(pPrefabNode);
		prefabNodeDepth = pPrefabNode->GetDepth();
		nodeCount = GetNodeCount();
		VERUS_FOR(i, nodeCount)
		{
			PBaseNode pNode = _vNodes[i];

			if (i == prefabNodeIndex)
				continue; // It's me.
			if (multiMode && NodeType::base != pNode->GetType())
				continue;
			if (HasAncestorOfType(NodeType::prefab, pNode))
				continue;
			if (HasAncestorOfType(NodeType::instance, pNode))
				continue;

			int offset = 0;
			int depthDelta = 0;
			bool similar = false;

			while (true)
			{
				const int prefabChildIndex = offset + prefabNodeIndex + 1;
				const int targetChildIndex = offset + i + (multiMode ? 1 : 0);

				bool prefabEnd = (prefabChildIndex >= nodeCount);
				bool targetEnd = (targetChildIndex >= nodeCount);

				if (!prefabEnd && (prefabNodeDepth >= _vNodes[prefabChildIndex]->GetDepth()))
					prefabEnd = true;
				if (!targetEnd && (multiMode || offset) && (pNode->GetDepth() >= _vNodes[targetChildIndex]->GetDepth()))
					targetEnd = true;

				if (prefabEnd || targetEnd)
				{
					similar = (prefabEnd == targetEnd); // Both branches have equal length.
					break; // No more prefab children.
				}

				PBaseNode pPrefabChildNode = _vNodes[prefabChildIndex];
				PBaseNode pTargetChildNode = _vNodes[targetChildIndex];

				if (0 == offset)
					depthDelta = 1 + prefabNodeDepth - pTargetChildNode->GetDepth();

				if (pPrefabChildNode->GetDepth() - pTargetChildNode->GetDepth() != depthDelta)
					break; // Depth mismatch.
				if (pPrefabChildNode->GetType() != pTargetChildNode->GetType())
					break; // Type mismatch.
				if (NodeType::block == pPrefabChildNode->GetType())
				{
					PBlockNode pPrefabBlockNode = static_cast<PBlockNode>(pPrefabChildNode);
					PBlockNode pTargetBlockNode = static_cast<PBlockNode>(pTargetChildNode);
					if (pPrefabBlockNode->GetURL() != pTargetBlockNode->GetURL())
						break; // Block URL mismatch.
				}

				offset++;
			}

			if (similar)
			{
				_recursionDepth = 1; // Disable sorting.

				InstanceNodePtr instanceNode;
				instanceNode.Init(pPrefabNode);
				instanceNode->SetTransform(pNode->GetTransform());

				_recursionDepth = 0; // Enable sorting.

				DeleteNode(pNode);
				nodeDeleted = true;
				break;
			}
		}
	} while (nodeDeleted);

	SortNodes();
}

PModelNode WorldManager::InsertModelNode(CSZ url)
{
	auto p = TStoreModelNodes::Insert(url);
	if (!p->GetRefCount())
		_vNodes.push_back(p);
	return p;
}

PModelNode WorldManager::FindModelNode(CSZ url)
{
	return TStoreModelNodes::Find(url);
}

PParticlesNode WorldManager::InsertParticlesNode(CSZ url)
{
	auto p = TStoreParticlesNodes::Insert(url);
	if (!p->GetRefCount())
		_vNodes.push_back(p);
	return p;
}

PParticlesNode WorldManager::FindParticlesNode(CSZ url)
{
	return TStoreParticlesNodes::Find(url);
}

PBaseNode WorldManager::InsertBaseNode()
{
	auto p = TStoreBaseNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PBlockNode WorldManager::InsertBlockNode()
{
	auto p = TStoreBlockNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PControlPointNode WorldManager::InsertControlPointNode()
{
	auto p = TStoreControlPointNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PEmitterNode WorldManager::InsertEmitterNode()
{
	auto p = TStoreEmitterNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PInstanceNode WorldManager::InsertInstanceNode()
{
	auto p = TStoreInstanceNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PLightNode WorldManager::InsertLightNode()
{
	auto p = TStoreLightNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PPathNode WorldManager::InsertPathNode()
{
	auto p = TStorePathNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PPhysicsNode WorldManager::InsertPhysicsNode()
{
	auto p = TStorePhysicsNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PPrefabNode WorldManager::InsertPrefabNode()
{
	auto p = TStorePrefabNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PShakerNode WorldManager::InsertShakerNode()
{
	auto p = TStoreShakerNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PSoundNode WorldManager::InsertSoundNode()
{
	auto p = TStoreSoundNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

PTerrainNode WorldManager::InsertTerrainNode()
{
	auto p = TStoreTerrainNodes::Insert();
	_vNodes.push_back(p);
	return p;
}

void WorldManager::BroadcastOnNodeDeleted(PBaseNode pTargetNode, bool afterEvent, bool hierarchy)
{
	for (auto pNode : _vNodes)
	{
		if (pNode)
			pNode->OnNodeDeleted(pTargetNode, afterEvent, hierarchy);
	}
}

void WorldManager::BroadcastOnNodeDuplicated(PBaseNode pTargetNode, bool afterEvent, PBaseNode pDuplicatedNode, HierarchyDuplication hierarchyDuplication)
{
	const int nodeCount = GetNodeCount();
	VERUS_FOR(i, nodeCount)
	{
		PBaseNode pNode = _vNodes[i];
		pNode->OnNodeDuplicated(pTargetNode, afterEvent, pDuplicatedNode, hierarchyDuplication);
	}
}

void WorldManager::BroadcastOnNodeParentChanged(PBaseNode pTargetNode, bool afterEvent)
{
	for (auto pNode : _vNodes)
		pNode->OnNodeParentChanged(pTargetNode, afterEvent);
}

void WorldManager::BroadcastOnNodeRigidBodyTransformUpdated(PBaseNode pTargetNode, bool afterEvent)
{
	for (auto pNode : _vNodes)
		pNode->OnNodeRigidBodyTransformUpdated(pTargetNode, afterEvent);
}

void WorldManager::BroadcastOnNodeTransformed(PBaseNode pTargetNode, bool afterEvent)
{
	for (auto pNode : _vNodes)
		pNode->OnNodeTransformed(pTargetNode, afterEvent);
}

void WorldManager::Serialize(IO::RSeekableStream stream)
{
	stream.WriteText(VERUS_CRNL VERUS_CRNL "<WM>");
	stream.BeginBlock();

	stream.WriteString(_C(std::to_string(_worldSide)));

	stream.WriteString(_C(std::to_string(GetNodeCount(nullptr, true))));

	for (auto& shakerNode : TStoreShakerNodes::_list)
		shakerNode.ApplyInitialValue();

	for (auto pNode : _vNodes)
	{
		if (pNode->IsGenerated())
			continue;
		stream << NodeTypeToHash(pNode->GetType());
		switch (pNode->GetType())
		{
		case NodeType::model:     stream.WriteString(_C(static_cast<PModelNode>(pNode)->GetURL())); break;
		case NodeType::particles: stream.WriteString(_C(static_cast<PParticlesNode>(pNode)->GetURL())); break;
		}
		pNode->Serialize(stream);
	}

	stream.EndBlock();
}

void WorldManager::Deserialize(IO::RStream stream)
{
	char buffer[IO::Stream::s_bufferSize] = {};

	stream.ReadString(buffer);
	const int worldSide = atoi(buffer);

	Desc desc;
	desc._worldSide = worldSide;
	PCamera     pPassCamera = _pPassCamera;
	PMainCamera pHeadCamera = _pHeadCamera;
	PMainCamera pViewCamera = _pViewCamera;
	Done();
	Init(desc);
	_pPassCamera = pPassCamera;
	_pHeadCamera = pHeadCamera;
	_pViewCamera = pViewCamera;

	_recursionDepth = 1; // Disable sorting.

	stream.ReadString(buffer);
	const int nodeCount = atoi(buffer);
	VERUS_FOR(i, nodeCount)
	{
		UINT32 hash;
		stream >> hash;
		const NodeType type = HashToNodeType(hash);

		String url;
		switch (type)
		{
		case NodeType::model:
		case NodeType::particles:
		{
			stream.ReadString(buffer);
			url = buffer;
		}
		break;
		}

		switch (type)
		{
		case NodeType::base:         InsertBaseNode()->Deserialize(stream); break;
		case NodeType::model:        InsertModelNode(_C(url))->Deserialize(stream); break;
		case NodeType::particles:    InsertParticlesNode(_C(url))->Deserialize(stream); break;
		case NodeType::block:        InsertBlockNode()->Deserialize(stream); break;
		case NodeType::controlPoint: InsertControlPointNode()->Deserialize(stream); break;
		case NodeType::emitter:      InsertEmitterNode()->Deserialize(stream); break;
		case NodeType::instance:     InsertInstanceNode()->Deserialize(stream); break;
		case NodeType::light:        InsertLightNode()->Deserialize(stream); break;
		case NodeType::path:         InsertPathNode()->Deserialize(stream); break;
		case NodeType::physics:      InsertPhysicsNode()->Deserialize(stream); break;
		case NodeType::prefab:       InsertPrefabNode()->Deserialize(stream); break;
		case NodeType::shaker:       InsertShakerNode()->Deserialize(stream); break;
		case NodeType::sound:        InsertSoundNode()->Deserialize(stream); break;
		case NodeType::terrain:      InsertTerrainNode()->Deserialize(stream); break;
		}
	}

	_recursionDepth = 0; // Enable sorting.
	SortNodes();

	for (auto pNode : _vNodes)
		pNode->OnAllNodesDeserialized();

	for (auto& prefabNode : TStorePrefabNodes::_list)
		UpdatePrefabInstances(&prefabNode, false);
	SortNodes();
}

void WorldManager::Deserialize_LegacyXXX(IO::RStream stream)
{
	char buffer[IO::Stream::s_bufferSize] = {};
	int count = 0;

	stream.ReadString(buffer);
	const int mapSide = atoi(buffer);

	Desc desc;
	desc._worldSide = mapSide;
	PCamera     pPassCamera = _pPassCamera;
	PMainCamera pHeadCamera = _pHeadCamera;
	PMainCamera pViewCamera = _pViewCamera;
	Done();
	Init(desc);
	_pPassCamera = pPassCamera;
	_pHeadCamera = pHeadCamera;
	_pViewCamera = pViewCamera;

	_recursionDepth = 1; // Disable sorting.

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		stream.ReadString(buffer);
		InsertModelNode(buffer)->Deserialize_LegacyXXX(stream, buffer);
	}

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		InsertBlockNode()->Deserialize_LegacyXXX(stream);
	}

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		InsertEmitterNode()->Deserialize_LegacyXXX(stream);
	}

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		InsertLightNode()->Deserialize_LegacyXXX(stream);
	}

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		InsertBaseNode()->Deserialize_LegacyXXXPrefab(stream);
	}

	stream.ReadString(buffer);
	count = atoi(buffer);
	VERUS_FOR(i, count)
	{
		//PTrigger p = InsertTrigger();
		//p->Deserialize(stream);
	}

	_recursionDepth = 0; // Enable sorting.
	SortNodes();
}

UINT32 WorldManager::NodeTypeToHash(NodeType type)
{
	switch (type)
	{
	case NodeType::base:         return 'ESAB';
	case NodeType::model:        return 'EDOM';
	case NodeType::particles:    return 'TRAP';
	case NodeType::block:        return 'COLB';
	case NodeType::controlPoint: return 'TNOC';
	case NodeType::emitter:      return 'TIME';
	case NodeType::instance:     return 'TSNI';
	case NodeType::light:        return 'HGIL';
	case NodeType::path:         return 'HTAP';
	case NodeType::physics:      return 'SYHP';
	case NodeType::prefab:       return 'FERP';
	case NodeType::shaker:       return 'KAHS';
	case NodeType::sound:        return 'NUOS';
	case NodeType::terrain:      return 'RRET';
	}
	VERUS_RT_FAIL("Invalid type.");
	return 0;
}

NodeType WorldManager::HashToNodeType(UINT32 hash)
{
	switch (hash)
	{
	case 'ESAB': return NodeType::base;
	case 'EDOM': return NodeType::model;
	case 'TRAP': return NodeType::particles;
	case 'COLB': return NodeType::block;
	case 'TNOC': return NodeType::controlPoint;
	case 'TIME': return NodeType::emitter;
	case 'TSNI': return NodeType::instance;
	case 'HGIL': return NodeType::light;
	case 'HTAP': return NodeType::path;
	case 'SYHP': return NodeType::physics;
	case 'FERP': return NodeType::prefab;
	case 'KAHS': return NodeType::shaker;
	case 'NUOS': return NodeType::sound;
	case 'RRET': return NodeType::terrain;
	}
	VERUS_RT_FAIL("Invalid hash.");
	return NodeType::unknown;
}

UINT32 WorldManager::NodeTypeToColor(NodeType type, int alpha)
{
	const UINT32 colors[] =
	{
		0,

		VERUS_COLOR_RGBA(255, 000, 000, alpha), // 1, emitter (red)
		VERUS_COLOR_RGBA(000, 255, 000, alpha), // 2, block (green)
		VERUS_COLOR_RGBA(000, 000, 255, alpha), // 3, physics (blue)

		VERUS_COLOR_RGBA(000, 255, 255, alpha), // 4, controlPoint (cyan)
		VERUS_COLOR_RGBA(255, 000, 255, alpha), // 5, instance (magenta)
		VERUS_COLOR_RGBA(255, 255, 000, alpha), // 6, light (yellow)

		VERUS_COLOR_RGBA(255, 128, 128, alpha), // 7, particles
		VERUS_COLOR_RGBA(128, 255, 128, alpha), // 8, model
		VERUS_COLOR_RGBA(128, 128, 255, alpha), // 9, base

		VERUS_COLOR_RGBA(128, 255, 255, alpha), // 10, path
		VERUS_COLOR_RGBA(255, 128, 255, alpha), // 11, prefab
		VERUS_COLOR_RGBA(255, 255, 128, alpha), // 12, terrain

		VERUS_COLOR_RGBA(255, 128, 000, alpha), // 13, sound
		VERUS_COLOR_RGBA(000, 255, 128, alpha), // 14, shaker
		VERUS_COLOR_RGBA(128, 000, 255, alpha), // 15
	};

	switch (type)
	{
	case NodeType::base:         return colors[9];
	case NodeType::model:        return colors[8];
	case NodeType::particles:    return colors[7];
	case NodeType::block:        return colors[2];
	case NodeType::controlPoint: return colors[4];
	case NodeType::emitter:      return colors[1];
	case NodeType::instance:     return colors[5];
	case NodeType::light:        return colors[6];
	case NodeType::path:         return colors[10];
	case NodeType::physics:      return colors[3];
	case NodeType::prefab:       return colors[11];
	case NodeType::shaker:       return colors[14];
	case NodeType::sound:        return colors[13];
	case NodeType::terrain:      return colors[12];
	}
	return 0;
}

int WorldManager::GetNodeTypePriority(NodeType type)
{
	switch (type)
	{
	case NodeType::model:
	case NodeType::particles:
		return 0;
	case NodeType::controlPoint:
	case NodeType::prefab:
		return 1;
	}
	return 2;
}
