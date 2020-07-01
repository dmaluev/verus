#pragma once

namespace verus
{
	namespace Scene
	{
		class Forest : public Object, public ScatterDelegate, public Math::OctreeDelegate
		{
			//#include "../Cg/DS_Forest.inc.cg"

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

		private:
			struct Vertex
			{
				glm::vec3 _pos;
				short     _tc[2]; // psize, angle.
			};
			VERUS_TYPEDEFS(Vertex);

			class BakedChunk
			{
			public:
				Vector<Vertex> _vSprites;
				int            _vbOffset = 0;
				bool           _visible = false;
			};
			VERUS_TYPEDEFS(BakedChunk);

			class Plant
			{
			public:
				enum TEX
				{
					TEX_GBUFFER_0,
					TEX_GBUFFER_2,
					TEX_GBUFFER_3,
					TEX_COUNT
				};

				String                      _url;
				Mesh                        _mesh;
				MaterialPwn                 _material;
				CGI::TexturePwns<TEX_COUNT> _tex;
				Vector<BakedChunk>          _vBakedChunks;
				Vector<float>               _vScales;
				float                       _maxScale = 0;
				float                       _maxSize = 0;
				char                        _normal = 116;

				float GetSize() const;
			};
			VERUS_TYPEDEFS(Plant);

			class LayerPlants
			{
			public:
				char _plants[SCATTER_TYPE_COUNT];

				LayerPlants()
				{
					std::fill(_plants, _plants + SCATTER_TYPE_COUNT, -1);
				}
			};
			VERUS_TYPEDEFS(LayerPlants);

			class DrawPlant
			{
			public:
				Matrix3 _basis = Matrix3::identity();
				Point3  _pos = Point3(0);
				Vector3 _pushBack = Vector3(0);
				float   _scale = 1;
				float   _angle = 0;
				int     _plantIndex = 0;
			};
			VERUS_TYPEDEFS(DrawPlant);

			static CGI::ShaderPwn s_shader;
			//static CGI::CStateBlockPwns<SB_MAX> ms_sb;

			PTerrain            _pTerrain = nullptr;
			CGI::GeometryPwn    _geo;
			Math::Octree        _octree;
			Scatter             _scatter;
			Vector<Plant>       _vPlants;
			Vector<LayerPlants> _vLayerPlants;
			Vector<DrawPlant>   _vDrawPlants;
			const float         _margin = 1.1f;
			float               _maxDist = 100;
			float               _maxSizeAll = 0;
			int                 _capacity = 4000;
			int                 _visibleCount = 0;
			bool                _async_initPlants = false;

		public:
			class PlantDesc
			{
			public:
				CSZ   _url = nullptr;
				float _minScale = 0.5f;
				float _maxScale = 1.5f;
				char  _normal = 116;

				void Set(CSZ url, float minScale = 0.5f, float maxScale = 1.5f, char normal = 116)
				{
					_url = url;
					_minScale = minScale;
					_maxScale = maxScale;
					_normal = normal;
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

			void Init(PTerrain pTerrain);
			void Done();

			void ResetInstanceCount();

			void Update();
			void Layout();
			void Draw();
			VERUS_P(void DrawModels());
			VERUS_P(void DrawSprites());

			PTerrain SetTerrain(PTerrain p) { return Utils::Swap(_pTerrain, p); }

			void SetLayer(int layer, RcLayerDesc desc);
			VERUS_P(int FindPlant(CSZ url) const);
			VERUS_P(void BakeChunks(RPlant plant));
			bool LoadSprite(RPlant plant);
			void BakeSprite(RPlant plant);

			virtual void Scatter_AddInstance(const int ij[2], int type, float x, float z,
				float scale, float angle, UINT32 r) override;

			virtual Continue Octree_ProcessNode(void* pToken, void* pUser) override;
		};
		VERUS_TYPEDEFS(Forest);
	}
}
