#pragma once

namespace verus
{
	namespace Scene
	{
		class Terrain;

		enum class TerrainTBN : int
		{
			tangent,
			binormal,
			normal
		};

		class TerrainPhysics
		{
			btHeightfieldTerrainShape* _pTerrainShape = nullptr;
			Vector<short>              _vData;

		public:
			TerrainPhysics();
			~TerrainPhysics();

			void Init(Physics::PUserPtr p, int w, int h, float heightScale = 0.01f);
			void Done();

			Vector<short>& GetData() { return _vData; }
		};
		VERUS_TYPEDEFS(TerrainPhysics);

		class TerrainLOD
		{
		public:
			Vector<UINT16> _vIB;
			int            _side = 0;
			int            _vertCount = 0;
			int            _indexCount = 0;
			int            _firstIndex = 0;

			void Init(int sidePoly, int step);
			void InitGeo(short* pV, UINT16* pI, int vertexOffset);
		};
		VERUS_TYPEDEFS(TerrainLOD);

		class TerrainPatch
		{
		public:
			struct TBN
			{
				char _normals0[16 * 16][3];
				char _normals1[8 * 8][3];
				char _normals2[4 * 4][3];
				char _normals3[2 * 2][3];
				char _normals4[1 * 1][3];
			};
			VERUS_TYPEDEFS(TBN);

			PTBN   _pTBN = nullptr;
			UINT16 _ijCoord[2];
			char   _layerForChannel[4];
			int    _distToCameraSq = 0;
			short  _patchHeight = 0;
			char   _quadtreeLOD = 0;
			char   _usedChannelCount = 1;
			short  _height[16 * 16];
			char   _mainLayer[16 * 16];

			TerrainPatch();
			void BindTBN(PTBN p);
			void InitFlat(short height, int mainLayer);
			void UpdateNormals(Terrain* p, int lod);
			int GetSplatChannelForLayer(int layer);
		};
		VERUS_TYPEDEFS(TerrainPatch);

		class Terrain : public Object, public Math::QuadtreeIntegralDelegate, public Physics::UserPtr
		{
		public:
#include "../Shaders/DS_Terrain.inc.hlsl"

			enum PIPE
			{
				PIPE_LIST,
				PIPE_STRIP,
				PIPE_DEPTH_LIST,
				PIPE_DEPTH_STRIP,
				PIPE_MAX
			};

			enum TEX
			{
				TEX_HEIGHTMAP,
				TEX_NORMALS,
				TEX_BLEND,
				TEX_LAYERS,
				TEX_LAYERS_NM,
				TEX_MAIN_LAYER,
				TEX_MAX
			};

			static const int s_maxLayers = 16;

			struct PerInstanceData
			{
				short _posPatch[4];
				short _layers[4];
			};

			struct LayerData
			{
				float _specAmount = 1;
				float _detailAmount = 1;
			};

		protected:
			static CGI::ShaderPwn   s_shader;
			static UB_DrawDepth     s_ubDrawDepth;
			static UB_PerMaterialFS s_ubPerMaterialFS;

			CGI::GeometryPwn            _geo;
			CGI::PipelinePwns<PIPE_MAX> _pipe;
			CGI::TexturePwns<TEX_MAX>   _tex;
			Vector<TerrainPatch>        _vPatches;
			Vector<TerrainPatch::TBN>   _vPatchTBNs;
			Vector<UINT16>              _vSortedPatchIndices;
			Vector<PerInstanceData>     _vInstanceBuffer;
			Vector<String>              _vLayerUrls;
			Vector<UINT32>              _vBlend;
			float                       _quadtreeFatten = 0.5f;
			int                         _mapSide = 0;
			int                         _mapShift = 0;
			int                         _visiblePatchCount = 0;
			int                         _vertCount = 0;
			int                         _indexCount = 0;
			int                         _instanceCount = 0;
			int                         _csidVS = -1;
			int                         _csidFS = -1;
			TerrainLOD                  _lods[5]; // Level of detail data for (16x16, 8x8, 4x4, 2x2, 1x1).
			LayerData                   _layerData[s_maxLayers];
			TerrainPhysics              _physics;
			Math::QuadtreeIntegral      _quadtree;

		public:
			struct Desc
			{
				int   _mapSide = 256;
				int   _layer = 0;
				short _height = 0;
				short _debugHills = 0;

				Desc() {}
			};
			VERUS_TYPEDEFS(Desc);

			Terrain();
			virtual ~Terrain();

			static void InitStatic();
			static void DoneStatic();

			void Init(RcDesc desc = Desc());
			void Done();

			virtual int UserPtr_GetType() override;

			void ResetInstanceCount();
			void Layout();
			void Draw();

			void SortVisiblePatches();

			int GetMapSide() const { return _mapSide; }

			RTerrainPatch GetPatch(int index) { return _vPatches[index]; }

			// Quadtree:
			Math::RQuadtreeIntegral GetQuadtree() { return _quadtree; }
			virtual void QuadtreeIntegral_ProcessVisibleNode(const short ij[2], RcPoint3 center) override;
			virtual void QuadtreeIntegral_GetHeights(const short ij[2], float height[2]) override;
			void FattenQuadtreeNodesBy(float x);

			// Height:
			static constexpr float ConvertHeight(short h) { return h * 0.01f; }
			static constexpr int ConvertHeight(float h) { return static_cast<int>(h * 100); }
			float GetHeightAt(const float xz[2]) const;
			float GetHeightAt(RcPoint3 pos) const;
			float GetHeightAt(const int ij[2], int lod = 0, short* pRaw = nullptr) const;
			void SetHeightAt(const int ij[2], short h);

			// Normals:
			const char* GetNormalAt(const int ij[2], int lod = 0, TerrainTBN tbn = TerrainTBN::normal) const;
			Matrix3 GetBasisAt(const int ij[2]) const;

			// Layers:
			void LoadLayerTexture(int layer, CSZ url);
			void LoadLayerTextures();
			int GetMainLayerAt(const int ij[2]) const;

			// Textures:
			void UpdateHeightmapTexture();
			CGI::TexturePtr GetHeightmapTexture() const;
			void UpdateNormalsTexture();
			CGI::TexturePtr GetNormalsTexture() const;
			void UpdateMainLayerTexture();
			CGI::TexturePtr GetMainLayerTexture() const;
			void OnHeightModified();

			// Physics:
			void AddNewRigidBody();
			void UpdateRigidBodyData();
		};
		VERUS_TYPEDEFS(Terrain);
	}
}
