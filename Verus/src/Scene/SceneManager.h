#pragma once

namespace verus
{
	namespace Scene
	{
		enum class DrawDepth : int
		{
			no,
			yes,
			automatic
		};

		typedef StoreUnique<String, Model> TStoreModels;
		typedef Store<Block> TStoreBlocks;
		typedef Store<Light> TStoreLights;
		class SceneManager : public Singleton<SceneManager>, public Object, public Math::OctreeDelegate,
			private TStoreModels, private TStoreBlocks, private TStoreLights
		{
			Math::Octree       _octree;
			PCamera            _pCamera = nullptr;
			PMainCamera        _pMainCamera = nullptr;
			Vector<PSceneNode> _vVisibleNodes;
			int                _visibleCount = 0;
			int                _visibleCountPerType[+NodeType::count];

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
				NodeType _type = NodeType::unknown;

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
			void DrawLights();

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

			// Blocks:
			PBlock InsertBlock();
			void DeleteBlock(PBlock p);
			void DeleteAllBlocks();

			// Lights (and Shadows):
			PLight InsertLight();
			void DeleteLight(PLight p);
			void DeleteAllLights();

			template<typename T>
			void ForEachNode(RcQuery query, const T& fn)
			{
				// Visit PBLE:
				//if (NodeType::unknown == query._type || NodeType::prefab == query._type)
				//{
				//	VERUS_FOREACH_X(TStorePrefabs::TList, TStorePrefabs::_list, it)
				//	{
				//		auto& prefab = *it++;
				//		if (!query._name || prefab.GetName() == query._name)
				//			if (Continue::yes != fn(prefab))
				//				return;
				//	}
				//}
				if (NodeType::unknown == query._type || NodeType::block == query._type)
				{
					VERUS_FOREACH_X(TStoreBlocks::TList, TStoreBlocks::_list, it)
					{
						auto& block = *it++;
						if (
							(!query._name || block.GetName() == query._name) &&
							(!query._blockMesh || block.GetModel()->GetMesh().GetUrl() == query._blockMesh) &&
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
						if (!query._name || light.GetName() == query._name)
							if (Continue::yes != fn(light))
								return;
					}
				}
				//if (NodeType::unknown == query._type || NodeType::emitter == query._type)
				//{
				//	VERUS_FOREACH_X(TStoreEmitters::TList, TStoreEmitters::_list, it)
				//	{
				//		auto& emitter = *it++;
				//		if (!query._name || emitter.GetName() == query._name)
				//			if (Continue::yes != fn(emitter))
				//				return;
				//	}
				//}
			}

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

			// Octree (Acceleration Structure):
			Math::ROctree GetOctree() { return _octree; }
			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) override;

			bool RayCastingTest(RcPoint3 pointA, RcPoint3 pointB, PBlockPtr pBlock = nullptr,
				PPoint3 pPoint = nullptr, PVector3 pNormal = nullptr, const float* pRadius = nullptr,
				Physics::Group mask = Physics::Group::immovable | Physics::Group::terrain);
		};
		VERUS_TYPEDEFS(SceneManager);
	}
}
