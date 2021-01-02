// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		typedef StoreUnique<String, Model> TStoreModels;
		typedef StoreUnique<String, SceneParticles> TStoreSceneParticles;
		typedef StoreUnique<String, Site> TStoreSites;
		typedef Store<Block> TStoreBlocks;
		typedef Store<Light> TStoreLights;
		typedef Store<Emitter> TStoreEmitters;
		typedef Store<Prefab> TStorePrefabs;
		class SceneManager : public Singleton<SceneManager>, public Object, public Math::OctreeDelegate,
			private TStoreModels, private TStoreSceneParticles, private TStoreSites,
			private TStoreBlocks, private TStoreLights, private TStoreEmitters, private TStorePrefabs
		{
			Math::Octree       _octree;
			PCamera            _pCamera = nullptr;
			PMainCamera        _pMainCamera = nullptr;
			Vector<PSceneNode> _vVisibleNodes;
			int                _visibleCount = 0;
			int                _visibleCountPerType[+NodeType::count];
			int                _mapSide = 0;

		public:
			struct Desc
			{
				PMainCamera _pMainCamera = nullptr;
				int         _mapSide = 256;

				Desc() {}
			};
			VERUS_TYPEDEFS(Desc);

			struct Query
			{
				CSZ      _name = nullptr;
				CSZ      _blockMesh = nullptr;
				CSZ      _blockMaterial = nullptr;
				CSZ      _particlesUrl = nullptr;
				NodeType _type = NodeType::unknown;
				int      _selected = -1;

				void Reset() { *this = Query(); }
			};
			VERUS_TYPEDEFS(Query);

			SceneManager();
			virtual ~SceneManager();

			void Init(RcDesc desc = Desc());
			void Done();

			void ResetInstanceCount();
			void Update();
			void UpdateParts();
			void Layout();
			void Draw();
			void DrawSimple(DrawSimpleMode mode);
			void DrawTransparent();
			void DrawLights();
			void DrawBounds();

			// Camera:
			PCamera GetCamera() const { return _pCamera; }
			PCamera SetCamera(PCamera p) { return Utils::Swap(_pCamera, p); }
			PMainCamera GetMainCamera() const { return _pMainCamera; }
			PMainCamera SetCamera(PMainCamera p) { _pCamera = p; return Utils::Swap(_pMainCamera, p); }

			static bool IsDrawingDepth(DrawDepth dd);

			String EnsureUniqueName(CSZ name);

			// Models:
			PModel InsertModel(CSZ url);
			PModel FindModel(CSZ url);
			void DeleteModel(CSZ url);
			void DeleteAllModels();
			template<typename T>
			void ForEachModel(const T& fn)
			{
				VERUS_FOREACH_X(TStoreModels::TMap, TStoreModels::_map, it)
				{
					auto& model = *it++;
					if (Continue::yes != fn(model.second))
						return;
				}
			}

			// SceneParticles:
			PSceneParticles InsertSceneParticles(CSZ url);
			PSceneParticles FindSceneParticles(CSZ url);
			void DeleteSceneParticles(CSZ url);
			void DeleteAllSceneParticles();
			template<typename T>
			void ForEachParticles(const T& fn)
			{
				VERUS_FOREACH_X(TStoreSceneParticles::TMap, TStoreSceneParticles::_map, it)
				{
					auto& particles = *it++;
					if (Continue::yes != fn(particles.second))
						return;
				}
			}

			// Sites:
			PSite InsertSite(CSZ name);
			PSite FindSite(CSZ name);
			void DeleteSite(CSZ name);
			void DeleteAllSites();
			template<typename T>
			void ForEachSite(const T& fn)
			{
				VERUS_FOREACH_X(TStoreSites::TMap, TStoreSites::_map, it)
				{
					auto& site = *it++;
					if (Continue::yes != fn(site.second))
						return;
				}
			}

			// Blocks:
			PBlock InsertBlock();
			void DeleteBlock(PBlock p);
			void DeleteAllBlocks();

			// Lights (and Shadows):
			PLight InsertLight();
			void DeleteLight(PLight p);
			void DeleteAllLights();

			// Emitters:
			PEmitter InsertEmitter();
			void DeleteEmitter(PEmitter p);
			void DeleteAllEmitters();

			// Prefabs:
			PPrefab InsertPrefab();
			void DeletePrefab(PPrefab p);
			void DeleteAllPrefabs();

			template<typename T>
			void ForEachNode(RcQuery query, const T& fn)
			{
				auto MatchName = [&query](RSceneNode node)
				{
					return !query._name || node.GetName() == query._name;
				};
				auto MatchSelected = [&query](RSceneNode node)
				{
					return -1 == query._selected || !!query._selected == node.IsSelected();
				};
				auto MatchNotBlockQuery = [&query]()
				{
					return !query._blockMesh && !query._blockMaterial;
				};

				// Visit PBLE:
				if (NodeType::unknown == query._type || NodeType::prefab == query._type)
				{
					VERUS_FOREACH_X(TStorePrefabs::TList, TStorePrefabs::_list, it)
					{
						auto& prefab = *it++;
						if (
							MatchName(prefab) &&
							MatchSelected(prefab) &&
							MatchNotBlockQuery())
							if (Continue::yes != fn(prefab))
								return;
					}
				}
				if (NodeType::unknown == query._type || NodeType::block == query._type)
				{
					VERUS_FOREACH_X(TStoreBlocks::TList, TStoreBlocks::_list, it)
					{
						auto& block = *it++;
						if (
							MatchName(block) &&
							MatchSelected(block) &&
							(!query._blockMesh || block.GetUrl() == query._blockMesh) &&
							(!query._blockMaterial || block.GetMaterial()->_name == query._blockMaterial))
							if (Continue::yes != fn(block))
								return;
					}
				}
				if (NodeType::unknown == query._type || NodeType::light == query._type)
				{
					VERUS_FOREACH_X(TStoreLights::TList, TStoreLights::_list, it)
					{
						auto& light = *it++;
						if (
							MatchName(light) &&
							MatchSelected(light) &&
							MatchNotBlockQuery())
							if (Continue::yes != fn(light))
								return;
					}
				}
				if (NodeType::unknown == query._type || NodeType::emitter == query._type)
				{
					VERUS_FOREACH_X(TStoreEmitters::TList, TStoreEmitters::_list, it)
					{
						auto& emitter = *it++;
						if (
							MatchName(emitter) &&
							MatchSelected(emitter) &&
							MatchNotBlockQuery() &&
							(!query._particlesUrl || emitter.GetUrl() == query._particlesUrl))
							if (Continue::yes != fn(emitter))
								return;
					}
				}
			}

			void DeleteNode(NodeType type, CSZ name);

			void ClearSelection();
			int GetSelectedCount();

			// Octree (Acceleration Structure):
			Math::ROctree GetOctree() { return _octree; }
			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) override;

			bool RayCastingTest(RcPoint3 pointA, RcPoint3 pointB, PBlockPtr pBlock = nullptr,
				PPoint3 pPoint = nullptr, PVector3 pNormal = nullptr, const float* pRadius = nullptr,
				Physics::Group mask = Physics::Group::immovable | Physics::Group::terrain);

			void Serialize(IO::RSeekableStream stream);
			void Deserialize(IO::RStream stream);
		};
		VERUS_TYPEDEFS(SceneManager);
	}
}
