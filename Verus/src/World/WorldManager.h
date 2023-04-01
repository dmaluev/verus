// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		// Although references are considered to be safer, prefer using PBaseNode over RBaseNode, because:
		// * You can pass nullptr to GetIndexOf and get the correct index value for nullptr (-1).
		// * No need to dereference the deleted node when calling BroadcastOnNodeDeleted(), which is UB in C++.
		// * In many algorithms nullptr can appear naturally and the code will look cleaner without extra checks.
		// * this is prettier than *this.
		typedef StoreUnique<String, ModelNode> TStoreModelNodes;
		typedef StoreUnique<String, ParticlesNode> TStoreParticlesNodes;
		typedef Store<BaseNode> TStoreBaseNodes;
		typedef Store<BlockNode> TStoreBlockNodes;
		typedef Store<BlockChainNode> TStoreBlockChainNodes;
		typedef Store<ControlPointNode> TStoreControlPointNodes;
		typedef Store<EmitterNode> TStoreEmitterNodes;
		typedef Store<InstanceNode> TStoreInstanceNodes;
		typedef Store<LightNode> TStoreLightNodes;
		typedef Store<PathNode> TStorePathNodes;
		typedef Store<PhysicsNode> TStorePhysicsNodes;
		typedef Store<PrefabNode> TStorePrefabNodes;
		typedef Store<ShakerNode> TStoreShakerNodes;
		typedef Store<SoundNode> TStoreSoundNodes;
		typedef Store<TerrainNode> TStoreTerrainNodes;
		class WorldManager : public Singleton<WorldManager>, public Object, public Math::OctreeDelegate,
			private TStoreModelNodes, private TStoreParticlesNodes,
			private TStoreBaseNodes, private TStoreBlockNodes, private TStoreBlockChainNodes,
			private TStoreControlPointNodes, private TStoreEmitterNodes, private TStoreInstanceNodes,
			private TStoreLightNodes, private TStorePathNodes, private TStorePhysicsNodes,
			private TStorePrefabNodes, private TStoreShakerNodes, private TStoreSoundNodes,
			private TStoreTerrainNodes
		{
			Math::Octree         _octree;
			LocalPtr<btBoxShape> _pPickingShape;
			PCamera              _pPassCamera = nullptr; // Render pass camera for getting view and projection matrices.
			PMainCamera          _pHeadCamera = nullptr; // Head camera which is located between the eyes.
			PMainCamera          _pViewCamera = nullptr; // Current view camera which is valid only inside DrawView method (eye camera).
			Vector<PBaseNode>    _vNodes;
			Vector<PBaseNode>    _vVisibleNodes;
			Random               _random;
			int                  _visibleCount = 0;
			int                  _visibleCountPerType[+NodeType::count];
			int                  _worldSide = 0;
			int                  _recursionDepth = 0;
			float                _pickingShapeHalfExtent = 0.05f;

		public:
			struct Desc
			{
				PMainCamera _pCamera = nullptr;
				int         _worldSide = 256;

				Desc() {}
			};
			VERUS_TYPEDEFS(Desc);

			struct Query
			{
				PBaseNode _pParent = nullptr;
				CSZ       _name = nullptr;
				CSZ       _blockMesh = nullptr;
				CSZ       _blockMaterial = nullptr;
				CSZ       _particlesURL = nullptr;
				NodeType  _type = NodeType::unknown;
				int       _selected = -1;

				void Reset() { *this = Query(); }
			};
			VERUS_TYPEDEFS(Query);

			WorldManager();
			virtual ~WorldManager();

			void Init(RcDesc desc = Desc());
			void Done();

			void ResetInstanceCount();
			void Update();
			void UpdateParts();
			void Layout();
			void SortVisible();
			void Draw();
			void DrawSimple(DrawSimpleMode mode);
			void DrawTerrainNodes(Terrain::RcDrawDesc dd);
			void DrawTerrainNodesSimple(DrawSimpleMode mode);
			void DrawLights();
			void DrawTransparent();
			void DrawSelectedBounds();
			void DrawSelectedRelationshipLines();
			void DrawEditorOverlays(DrawEditorOverlaysFlags flags);

			static bool IsDrawingDepth(DrawDepth dd);

			static int GetEditorOverlaysAlpha(int originalAlpha, float distSq, float fadeDistSq);

			// <Cameras>
			PCamera GetPassCamera() const { return _pPassCamera; }
			PMainCamera GetHeadCamera() const { return _pHeadCamera; }
			PMainCamera GetViewCamera() const { return _pViewCamera; }
			PCamera SetPassCamera(PCamera p) { return Utils::Swap(_pPassCamera, p); }
			PMainCamera SetHeadCamera(PMainCamera p) { return Utils::Swap(_pHeadCamera, p); }
			PMainCamera SetViewCamera(PMainCamera p) { return Utils::Swap(_pViewCamera, p); }
			void SetAllCameras(PMainCamera p) { _pPassCamera = _pHeadCamera = _pViewCamera = p; }
			// </Cameras>

			// Octree (Acceleration Structure):
			Math::ROctree GetOctree() { return _octree; }
			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) override;

			bool RayTest(RcPoint3 pointA, RcPoint3 pointB, Physics::Group mask = Physics::Group::all);
			bool RayTestEx(RcPoint3 pointA, RcPoint3 pointB, PBlockNodePtr pBlock = nullptr,
				PPoint3 pPoint = nullptr, PVector3 pNormal = nullptr, const float* pRadius = nullptr,
				Physics::Group mask = Physics::Group::all);
			Matrix3 GetBasisAt(RcPoint3 point, Physics::Group mask = Physics::Group::immovable) const;

			btBoxShape* GetPickingShape();
			float GetPickingShapeHalfExtent() const { return _pickingShapeHalfExtent; }

			template<typename T>
			void ForEachNode(const T& fn)
			{
				const int nodeCount = GetNodeCount();
				VERUS_FOR(i, nodeCount)
				{
					PBaseNode pNode = _vNodes[i];
					const bool hasChildren = (i + 1 < nodeCount) && _vNodes[i + 1]->GetDepth() > pNode->GetDepth();
					if (Continue::no == fn(*pNode, hasChildren))
						return;
				}
			}

			template<typename T>
			void ForEachNode(RcQuery query, const T& fn)
			{
				auto MatchName = [&query](RBaseNode node)
				{
					return !query._name || node.GetName() == query._name;
				};
				auto MatchSelected = [&query](RBaseNode node)
				{
					return -1 == query._selected || !!query._selected == node.IsSelected();
				};
				auto MatchParent = [&query](RBaseNode node)
				{
					return !query._pParent || node.GetParent() == query._pParent;
				};

				if (NodeType::unknown == query._type || NodeType::block == query._type)
				{
					VERUS_FOREACH_X(TStoreBlockNodes::TList, TStoreBlockNodes::_list, it)
					{
						auto& block = *it++;
						if (
							MatchName(block) &&
							MatchSelected(block) &&
							MatchParent(block) &&
							(!query._blockMesh || block.GetURL() == query._blockMesh) &&
							(!query._blockMaterial || block.GetMaterial()->_name == query._blockMaterial))
							if (Continue::no == fn(block))
								return;
					}
				}

				if (query._blockMesh || query._blockMaterial)
					return;

				if (NodeType::unknown == query._type || NodeType::base == query._type)
				{
					VERUS_FOREACH_X(TStoreBaseNodes::TList, TStoreBaseNodes::_list, it)
					{
						auto& node = *it++;
						if (
							MatchName(node) &&
							MatchSelected(node) &&
							MatchParent(node))
							if (Continue::no == fn(node))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::model == query._type)
				{
					VERUS_FOREACH_X(TStoreModelNodes::TMap, TStoreModelNodes::_map, it)
					{
						auto& model = it->second;
						it++;
						if (
							MatchName(model) &&
							MatchSelected(model) &&
							MatchParent(model))
							if (Continue::no == fn(model))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::particles == query._type)
				{
					VERUS_FOREACH_X(TStoreParticlesNodes::TMap, TStoreParticlesNodes::_map, it)
					{
						auto& particles = it->second;
						it++;
						if (
							MatchName(particles) &&
							MatchSelected(particles) &&
							MatchParent(particles) &&
							(!query._particlesURL || particles.GetURL() == query._particlesURL))
							if (Continue::no == fn(particles))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::blockChain == query._type)
				{
					VERUS_FOREACH_X(TStoreBlockChainNodes::TList, TStoreBlockChainNodes::_list, it)
					{
						auto& blockChain = *it++;
						if (
							MatchName(blockChain) &&
							MatchSelected(blockChain) &&
							MatchParent(blockChain))
							if (Continue::no == fn(blockChain))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::controlPoint == query._type)
				{
					VERUS_FOREACH_X(TStoreControlPointNodes::TList, TStoreControlPointNodes::_list, it)
					{
						auto& controlPoint = *it++;
						if (
							MatchName(controlPoint) &&
							MatchSelected(controlPoint) &&
							MatchParent(controlPoint))
							if (Continue::no == fn(controlPoint))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::emitter == query._type)
				{
					VERUS_FOREACH_X(TStoreEmitterNodes::TList, TStoreEmitterNodes::_list, it)
					{
						auto& emitter = *it++;
						if (
							MatchName(emitter) &&
							MatchSelected(emitter) &&
							MatchParent(emitter) &&
							(!query._particlesURL || emitter.GetURL() == query._particlesURL))
							if (Continue::no == fn(emitter))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::instance == query._type)
				{
					VERUS_FOREACH_X(TStoreInstanceNodes::TList, TStoreInstanceNodes::_list, it)
					{
						auto& instance = *it++;
						if (
							MatchName(instance) &&
							MatchSelected(instance) &&
							MatchParent(instance))
							if (Continue::no == fn(instance))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::light == query._type)
				{
					VERUS_FOREACH_X(TStoreLightNodes::TList, TStoreLightNodes::_list, it)
					{
						auto& light = *it++;
						if (
							MatchName(light) &&
							MatchSelected(light) &&
							MatchParent(light))
							if (Continue::no == fn(light))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::path == query._type)
				{
					VERUS_FOREACH_X(TStorePathNodes::TList, TStorePathNodes::_list, it)
					{
						auto& path = *it++;
						if (
							MatchName(path) &&
							MatchSelected(path) &&
							MatchParent(path))
							if (Continue::no == fn(path))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::physics == query._type)
				{
					VERUS_FOREACH_X(TStorePhysicsNodes::TList, TStorePhysicsNodes::_list, it)
					{
						auto& physics = *it++;
						if (
							MatchName(physics) &&
							MatchSelected(physics) &&
							MatchParent(physics))
							if (Continue::no == fn(physics))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::prefab == query._type)
				{
					VERUS_FOREACH_X(TStorePrefabNodes::TList, TStorePrefabNodes::_list, it)
					{
						auto& prefab = *it++;
						if (
							MatchName(prefab) &&
							MatchSelected(prefab) &&
							MatchParent(prefab))
							if (Continue::no == fn(prefab))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::shaker == query._type)
				{
					VERUS_FOREACH_X(TStoreShakerNodes::TList, TStoreShakerNodes::_list, it)
					{
						auto& shaker = *it++;
						if (
							MatchName(shaker) &&
							MatchSelected(shaker) &&
							MatchParent(shaker))
							if (Continue::no == fn(shaker))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::sound == query._type)
				{
					VERUS_FOREACH_X(TStoreSoundNodes::TList, TStoreSoundNodes::_list, it)
					{
						auto& sound = *it++;
						if (
							MatchName(sound) &&
							MatchSelected(sound) &&
							MatchParent(sound))
							if (Continue::no == fn(sound))
								return;
					}
				}

				if (NodeType::unknown == query._type || NodeType::terrain == query._type)
				{
					VERUS_FOREACH_X(TStoreTerrainNodes::TList, TStoreTerrainNodes::_list, it)
					{
						auto& terrain = *it++;
						if (
							MatchName(terrain) &&
							MatchSelected(terrain) &&
							MatchParent(terrain))
							if (Continue::no == fn(terrain))
								return;
					}
				}
			}

			template<typename T>
			void ForEachControlPointOf(PPathNode pPathNode, bool onlyHeads, const T& fn)
			{
				const int nodeCount = GetNodeCount();
				for (int i = GetIndexOf(pPathNode) + 1; i < nodeCount; ++i)
				{
					PBaseNode pNode = _vNodes[i];
					if (pNode->GetDepth() <= pPathNode->GetDepth())
						break;
					if (NodeType::controlPoint == pNode->GetType())
					{
						PControlPointNode pControlPointNode = static_cast<PControlPointNode>(pNode);
						if (!onlyHeads || pControlPointNode->IsHeadControlPoint())
						{
							if (Continue::no == fn(pControlPointNode))
								return;
						}
					}
				}
			}

			// <NodeOperations>
			String EnsureUniqueName(CSZ name, PcBaseNode pSkipNode = nullptr);
			void SortNodes();
			int FindOffsetFor(NodeType type) const;

			int GetNodeCount(int* pPerType = nullptr, bool excludeGenerated = false) const;
			int GetIndexOf(PcBaseNode pTargetNode, bool excludeGenerated = false) const;
			PBaseNode GetNodeByIndex(int index) const;

			static bool IsAncestorOf(PcBaseNode pNodeA, PcBaseNode pNodeB);
			static bool HasAncestorOfType(NodeType type, PcBaseNode pNode);

			bool IsValidNode(PcBaseNode pTargetNode);
			void DeleteNode(PBaseNode pTargetNode, bool hierarchy = true);
			void DeleteNode(NodeType type, CSZ name, bool hierarchy = true);
			void DeleteAllNodes();

			PBaseNode DuplicateNode(PBaseNode pTargetNode, HierarchyDuplication hierarchyDuplication);

			void EnableAll();
			void EnableSelected();
			void DisableAll();
			void DisableSelected();

			void SelectAll();
			void SelectAllInsideSphere(Math::RcSphere sphere);
			void ClearSelection();
			void InvertSelection();
			int GetSelectedCount();
			void SelectAllChildNodes(PBaseNode pTargetNode);

			void ResetRigidBodyTransforms();

			void GenerateBlockChainNodes(PBlockChainNode pBlockChainNode);
			static bool Connect2ControlPoints(PControlPointNode pNodeA, PControlPointNode pNodeB);
			void UpdatePrefabInstances(PPrefabNode pPrefabNode, bool sortNodes = true);
			void ReplaceSimilarWithInstances(PPrefabNode pPrefabNode);
			// </NodeOperations>

			PModelNode InsertModelNode(CSZ url);
			PModelNode FindModelNode(CSZ url);

			PParticlesNode InsertParticlesNode(CSZ url);
			PParticlesNode FindParticlesNode(CSZ url);

			PBaseNode InsertBaseNode();

			PBlockNode InsertBlockNode();

			PBlockChainNode InsertBlockChainNode();

			PControlPointNode InsertControlPointNode();

			PEmitterNode InsertEmitterNode();

			PInstanceNode InsertInstanceNode();

			PLightNode InsertLightNode();

			PPathNode InsertPathNode();

			PPhysicsNode InsertPhysicsNode();

			PPrefabNode InsertPrefabNode();

			PShakerNode InsertShakerNode();

			PSoundNode InsertSoundNode();

			PTerrainNode InsertTerrainNode();

			// <Events>
			void BroadcastOnNodeDeleted(PBaseNode pTargetNode, bool afterEvent, bool hierarchy);
			void BroadcastOnNodeDuplicated(PBaseNode pTargetNode, bool afterEvent, PBaseNode pDuplicatedNode, HierarchyDuplication hierarchyDuplication);
			void BroadcastOnNodeParentChanged(PBaseNode pTargetNode, bool afterEvent);
			void BroadcastOnNodeRigidBodyTransformUpdated(PBaseNode pTargetNode, bool afterEvent);
			void BroadcastOnNodeTransformed(PBaseNode pTargetNode, bool afterEvent);
			// </Events>

			// <Serialization>
			void Serialize(IO::RSeekableStream stream);
			void Deserialize(IO::RStream stream);
			void Deserialize_LegacyXXX(IO::RStream stream);
			static UINT32 NodeTypeToHash(NodeType type);
			static NodeType HashToNodeType(UINT32 hash);
			// </Serialization>

			static UINT32 NodeTypeToColor(NodeType type, int alpha = 255);
			static int GetNodeTypePriority(NodeType type);
		};
		VERUS_TYPEDEFS(WorldManager);
	}
}
