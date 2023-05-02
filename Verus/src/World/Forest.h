// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		class Forest : public Object, public ScatterDelegate, public Math::OctreeDelegate
		{
#include "../Shaders/DS_Forest.inc.hlsl"
#include "../Shaders/SimpleForest.inc.hlsl"

			enum SHADER
			{
				SHADER_MAIN,
				SHADER_SIMPLE,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_DEPTH,
				PIPE_SIMPLE_ENV_MAP,
				PIPE_SIMPLE_PLANAR_REF,
				PIPE_MESH_BAKE,
				PIPE_COUNT
			};

		public:
			enum SCATTER_TYPE
			{
				SCATTER_TYPE_100,
				SCATTER_TYPE_50,
				SCATTER_TYPE_25,
				SCATTER_TYPE_20,
				SCATTER_TYPE_15,
				SCATTER_TYPE_10,
				SCATTER_TYPE_5,
				SCATTER_TYPE_3,
				SCATTER_TYPE_COUNT
			};

			static const int s_scatterSide = 32;

		private:
			struct Vertex
			{
				glm::vec3 _pos;
				half      _tc[2]; // {psize, angle}
			};
			VERUS_TYPEDEFS(Vertex);

			class BakedChunk
			{
			public:
				Math::Bounds   _bounds;
				Vector<Vertex> _vSprites;
				int            _vbOffset = 0;
				bool           _visible = false;
			};
			VERUS_TYPEDEFS(BakedChunk);

			class CollisionPlant
			{
			public:
				int _id = -1;
				int _poolBlockIndex = -1;

				int GetID() const { return _id; }
			};
			VERUS_TYPEDEFS(CollisionPlant);

			class alignas(VERUS_MEMORY_ALIGNMENT) CollisionPoolBlock
			{
			public:
				BYTE _data[sizeof(btCompoundShape) + 2 * sizeof(btUniformScalingShape) + sizeof(btDefaultMotionState) + sizeof(btRigidBody)];
				bool _reserved = false;

				bool IsReserved() const { return _reserved; }
				void Reserve() { _reserved = true; }
				void Free() { _reserved = false; }

				btCompoundShape* GetCompoundShape()
				{
					return reinterpret_cast<btCompoundShape*>(&_data[0]);
				}
				btUniformScalingShape* GetUniformScalingShapeForTrunk()
				{
					return reinterpret_cast<btUniformScalingShape*>(&_data[sizeof(btCompoundShape)]);
				}
				btUniformScalingShape* GetUniformScalingShapeForBranches()
				{
					return reinterpret_cast<btUniformScalingShape*>(&_data[sizeof(btCompoundShape) + sizeof(btUniformScalingShape)]);
				}
				btDefaultMotionState* GetDefaultMotionState()
				{
					return reinterpret_cast<btDefaultMotionState*>(&_data[sizeof(btCompoundShape) + 2 * sizeof(btUniformScalingShape)]);
				}
				btRigidBody* GetRigidBody()
				{
					return reinterpret_cast<btRigidBody*>(&_data[sizeof(btCompoundShape) + 2 * sizeof(btUniformScalingShape) + sizeof(btDefaultMotionState)]);
				}
			};
			VERUS_TYPEDEFS(CollisionPoolBlock);

			class Plant
			{
			public:
				enum TEX
				{
					TEX_GBUFFER_0,
					TEX_GBUFFER_1,
					TEX_GBUFFER_2,
					TEX_GBUFFER_3,
					TEX_COUNT
				};

				String                      _url;
				Mesh                        _mesh;
				MaterialPwn                 _material;
				CGI::TexturePwns<TEX_COUNT> _tex;
				CGI::CSHandle               _csh;
				CGI::CSHandle               _cshSimple;
				LocalPtr<btConvexHullShape> _pConvexHullShapeForTrunk;
				LocalPtr<btConvexHullShape> _pConvexHullShapeForBranches;
				Vector<BakedChunk>          _vBakedChunks;
				Vector<float>               _vScales;
				float                       _alignToNormal = 1;
				float                       _maxScale = 0;
				float                       _maxSize = 0;
				float                       _windBending = 1;
				float                       _hullSplitU = -1;
				char                        _allowedNormal = 116;

				float GetSize() const;
			};
			VERUS_TYPEDEFS(Plant);

			class LayerData
			{
			public:
				char _plants[SCATTER_TYPE_COUNT];
				BYTE _occlusion[s_scatterSide * s_scatterSide];

				LayerData()
				{
					std::fill(_plants, _plants + SCATTER_TYPE_COUNT, -1);
					std::fill(_occlusion, _occlusion + sizeof(_occlusion), 0xFF);
				}
			};
			VERUS_TYPEDEFS(LayerData);

			class DrawPlant
			{
			public:
				Matrix3 _basis = Matrix3::identity();
				Point3  _pos = Point3(0);
				Vector3 _pushBack = Vector3(0);
				float   _scale = 1;
				float   _angle = 0;
				float   _distToEyeSq = 0;
				float   _windBending = 0;
				int     _plantIndex = 0;
			};
			VERUS_TYPEDEFS(DrawPlant);

			static CGI::ShaderPwns<SHADER_COUNT> s_shader;
			static UB_ForestVS                   s_ubForestVS;
			static UB_ForestFS                   s_ubForestFS;
			static UB_SimpleForestVS             s_ubSimpleForestVS;
			static UB_SimpleForestFS             s_ubSimpleForestFS;

			PTerrain                         _pTerrain = nullptr;
			CGI::GeometryPwn                 _geo;
			CGI::PipelinePwns<PIPE_COUNT>    _pipe;
			Math::Octree                     _octree;
			Scatter                          _scatter;
			Vector<Plant>                    _vPlants;
			Vector<LayerData>                _vLayerData;
			Vector<DrawPlant>                _vDrawPlants;
			DifferenceVector<CollisionPlant> _vCollisionPlants;
			Pool<CollisionPoolBlock>         _vCollisionPool;
			const float                      _margin = 1.1f;
			float                            _maxDistForModels = 100;
			float                            _maxDistForChunks = 1000;
			float                            _tessDist = 50;
			float                            _maxSizeAll = 0;
			float                            _phaseY = 0;
			float                            _phaseXZ = 0;
			int                              _capacity = 4000;
			int                              _visibleCount = 0;
			int                              _totalPlantCount = 0;
			bool                             _async_initPlants = false;

		public:
			class PlantDesc
			{
			public:
				CSZ   _url = nullptr;
				float _alignToNormal = 1;
				float _minScale = 0.8f;
				float _maxScale = 1.25f;
				float _windBending = 1;
				float _hullSplitU = -1;
				char  _allowedNormal = 116;

				void Set(
					CSZ url,
					float alignToNormal = 1,
					float minScale = 0.8f,
					float maxScale = 1.25f,
					float windBending = 1,
					float hullSplitU = -1,
					char allowedNormal = 116)
				{
					_url = url;
					_alignToNormal = alignToNormal;
					_minScale = minScale;
					_maxScale = maxScale;
					_windBending = windBending;
					_hullSplitU = hullSplitU;
					_allowedNormal = allowedNormal;
				}
			};
			VERUS_TYPEDEFS(PlantDesc);

			class LayerDesc
			{
			public:
				PlantDesc _forType[SCATTER_TYPE_COUNT];

				void Reset() { *this = LayerDesc(); }
			};
			VERUS_TYPEDEFS(LayerDesc);

			Forest();
			~Forest();

			static void InitStatic();
			static void DoneStatic();

			void Init(PTerrain pTerrain, CSZ url = nullptr);
			void Done();

			void ResetInstanceCount();
			void Update();
			void Layout(bool reflection = false);
			void SortVisible();
			void Draw(bool allowTess = true);
			VERUS_P(void DrawModels(bool allowTess));
			VERUS_P(void DrawSprites());
			void DrawSimple(DrawSimpleMode mode, CGI::CubeMapFace cubeMapFace = CGI::CubeMapFace::none);

			void UpdateCollision(const Vector<Vector4>& vZones);

			PTerrain SetTerrain(PTerrain p) { return Utils::Swap(_pTerrain, p); }
			void OnTerrainModified();
			float GetMinHeight(const int ij[2], float h) const;

			void SetLayer(int layer, RcLayerDesc desc);
			VERUS_P(int FindPlant(CSZ url) const);
			VERUS_P(void BakeChunksFor(RPlant plant));
			VERUS_P(void UpdateOcclusionFor(RPlant plant));
			VERUS_P(void AddOcclusionAt(const int ij[2], int layer, int radius));

			bool LoadSprite(RPlant plant);
			void BakeSprite(RPlant plant, CSZ url);
			bool BakeSprites(CSZ url);

			virtual void Scatter_AddInstance(const int ij[2], int type, float x, float z,
				float scale, float angle, UINT32 r) override;

			virtual Continue Octree_OnElementDetected(void* pToken, void* pUser) override;

			BYTE GetOcclusionAt(const int ij[2], int layer) const;
		};
		VERUS_TYPEDEFS(Forest);
	}
}
