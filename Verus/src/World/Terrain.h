// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class Terrain;
	class Forest;

	enum class TerrainTBN : int
	{
		tangent,
		binormal,
		normal
	};

	class TerrainPhysics
	{
		LocalPtr<btHeightfieldTerrainShape> _pShape;
		Physics::LocalRigidBody             _pRigidBody;
		bool                                _debugDraw = false;

	public:
		TerrainPhysics();
		~TerrainPhysics();

		void Init(Physics::PUserPtr p, int w, int h, const void* pData, float heightScale = 0.01f);
		void Done();

		void EnableDebugDraw(bool b);
		bool IsDebugDrawEnabled() const { return _debugDraw; }
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

		void Init(int sidePoly, int step, bool addEdgeTess);
		void InitGeo(short* pV, UINT16* pI, int vertexOffset, bool addEdgeTess);
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
		int    _distToHeadSq = 0;
		short  _patchHeight = 0;
		char   _quadtreeLOD = 0;
		char   _usedChannelCount = 1;
		short  _height[16 * 16];
		char   _mainLayer[16 * 16];

		TerrainPatch();
		void BindTBN(PTBN p);
		void InitFlat(short height, int mainLayer);
		void UpdateNormals(Terrain* p, int lod);
		int GetSplatChannelForLayer(int layer) const;
	};
	VERUS_TYPEDEFS(TerrainPatch);

	class Terrain : public Object, public Math::QuadtreeIntegralDelegate, public Physics::UserPtr
	{
	public:
#include "../Shaders/DS_Terrain.inc.hlsl"
#include "../Shaders/SimpleTerrain.inc.hlsl"

		enum SHADER
		{
			SHADER_MAIN,
			SHADER_SIMPLE,
			SHADER_COUNT
		};

		enum PIPE
		{
			PIPE_LIST,
			PIPE_STRIP,
			PIPE_TESS,

			PIPE_DEPTH_LIST,
			PIPE_DEPTH_STRIP,
			PIPE_DEPTH_TESS,

			PIPE_WIREFRAME_LIST,
			PIPE_WIREFRAME_STRIP,
			PIPE_SIMPLE_ENV_MAP_LIST,
			PIPE_SIMPLE_ENV_MAP_STRIP,
			PIPE_SIMPLE_PLANAR_REF_LIST,
			PIPE_SIMPLE_PLANAR_REF_STRIP,
			PIPE_SIMPLE_UNDERWATER_LIST,
			PIPE_SIMPLE_UNDERWATER_STRIP,
			PIPE_COUNT
		};

		enum TEX
		{
			TEX_HEIGHTMAP,
			TEX_NORMALS,
			TEX_BLEND,
			TEX_LAYERS,
			TEX_LAYERS_N,
			TEX_LAYERS_X,
			TEX_MAIN_LAYER,
			TEX_COUNT
		};

		static const int s_maxLayers = 32;

		struct PerInstanceData
		{
			short _posPatch[4];
			short _layers[4];
		};

		struct LayerData
		{
			float _detailStrength = 0.1f;
			float _roughStrength = 0.1f;
		};

	protected:
		static CGI::ShaderPwns<SHADER_COUNT> s_shader;
		static UB_TerrainVS                  s_ubTerrainVS;
		static UB_TerrainFS                  s_ubTerrainFS;
		static UB_SimpleTerrainVS            s_ubSimpleTerrainVS;
		static UB_SimpleTerrainFS            s_ubSimpleTerrainFS;

		CGI::GeometryPwn              _geo;
		CGI::PipelinePwns<PIPE_COUNT> _pipe;
		CGI::TexturePwns<TEX_COUNT>   _tex;
		Vector<TerrainPatch>          _vPatches;
		Vector<TerrainPatch::TBN>     _vPatchTBNs;
		Vector<UINT16>                _vSortedPatchIndices;
		Vector<UINT16>                _vRandomPatchIndices;
		Vector<PerInstanceData>       _vInstanceBuffer;
		Vector<String>                _vLayerUrls;
		Vector<short>                 _vHeightBuffer;
		Vector<half>                  _vHeightmapSubresData;
		Vector<UINT32>                _vNormalsSubresData;
		Vector<UINT32>                _vBlendBuffer;
		Vector<BYTE>                  _vMainLayerSubresData;
		float                         _quadtreeFatten = 0.5f;
		int                           _mapSide = 0;
		int                           _mapShift = 0;
		int                           _vertCount = 0;
		int                           _indexCount = 0;
		int                           _instanceCount = 0;
		int                           _visiblePatchCount = 0;
		int                           _visibleSortedPatchCount = 0;
		int                           _visibleRandomPatchCount = 0;
		CGI::CSHandle                 _cshVS;
		CGI::CSHandle                 _cshFS;
		CGI::CSHandle                 _cshSimpleVS;
		CGI::CSHandle                 _cshSimpleFS;
		TerrainLOD                    _lods[5]; // Level of detail data for (16x16, 8x8, 4x4, 2x2, 1x1).
		LayerData                     _layerData[s_maxLayers];
		TerrainPhysics                _physics;
		Math::QuadtreeIntegral        _quadtree;

	public:
		float _lamScale = 1.9f;
		float _lamBias = -0.9f;

		struct Desc
		{
			CSZ   _heightmapUrl = nullptr;
			float _heightmapScale = 1;
			float _heightmapBias = 0;
			int   _mapSide = 256;
			int   _layer = 0;
			short _height = 0;
			short _debugHills = 0;

			Desc() {}
		};
		VERUS_TYPEDEFS(Desc);

		struct DrawDesc
		{
			bool _allowTess = false;
			bool _wireframe = false;

			void Reset()
			{
				*this = DrawDesc();
			}
		};
		VERUS_TYPEDEFS(DrawDesc);

		Terrain();
		virtual ~Terrain();

		static void InitStatic();
		static void DoneStatic();

		void Init(RcDesc desc = Desc());
		void InitByWater();
		void Done();

		void ResetInstanceCount();
		void Layout();
		void SortVisible();
		void Draw(RcDrawDesc dd = DrawDesc());
		void DrawSimple(DrawSimpleMode mode);

		virtual int UserPtr_GetType() override;

		int GetMapSide() const { return _mapSide; }

		RTerrainPatch GetPatch(int index) { return _vPatches[index]; }

		// Quadtree:
		Math::RQuadtreeIntegral GetQuadtree() { return _quadtree; }
		virtual void QuadtreeIntegral_OnElementDetected(const short ij[2], RcPoint3 center) override;
		virtual void QuadtreeIntegral_GetHeights(const short ij[2], float height[2]) override;
		void FattenQuadtreeNodesBy(float x);

		// Height:
		static constexpr float ConvertHeight(short h) { return h * 0.01f; }
		static constexpr int ConvertHeight(float h) { return static_cast<int>(h * 100); }
		float GetHeightAt(const float xz[2]) const;
		float GetHeightAt(RcPoint3 pos) const;
		float GetHeightAt(const int ij[2], int lod = 0, short* pRaw = nullptr) const;
		void SetHeightAt(const int ij[2], short h);
		void UpdateHeightBuffer();

		// Normals:
		const char* GetNormalAt(const int ij[2], int lod = 0, TerrainTBN tbn = TerrainTBN::normal) const;
		Matrix3 GetBasisAt(const int ij[2]) const;

		// Layers:
		void InsertLayerUrl(int layer, CSZ url);
		void DeleteAllLayerUrls();
		void GetLayerUrls(Vector<CSZ>& v);
		void LoadLayersFromFile(CSZ url = nullptr);
		void LoadLayerTextures();
		int GetMainLayerAt(const int ij[2]) const;
		void UpdateMainLayerAt(const int ij[2]);

		float GetDetailStrength(int layer) const { return _layerData[layer]._detailStrength; }
		float GetRoughStrength(int layer) const { return _layerData[layer]._roughStrength; }
		void SetDetailStrength(int layer, float x) { _layerData[layer]._detailStrength = x; }
		void SetRoughStrength(int layer, float x) { _layerData[layer]._roughStrength = x; }

		// Textures:
		void UpdateHeightmapTexture();
		CGI::TexturePtr GetHeightmapTexture() const;
		void UpdateNormalsTexture();
		CGI::TexturePtr GetNormalsTexture() const;
		void UpdateBlendTexture();
		CGI::TexturePtr GetBlendTexture() const;
		void UpdateMainLayerTexture();
		CGI::TexturePtr GetMainLayerTexture() const;
		void ComputeOcclusion();
		void UpdateOcclusion(Forest* pForest);
		void OnHeightModified();

		// Physics:
		void AddNewRigidBody();
		RTerrainPhysics GetPhysics() { return _physics; }

		void Serialize(IO::RSeekableStream stream);
		void Deserialize(IO::RStream stream);
	};
	VERUS_TYPEDEFS(Terrain);
}
