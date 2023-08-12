// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class Grass : public Object, public Math::QuadtreeIntegralDelegate
	{
	public:
#include "../Shaders/DS_Grass.inc.hlsl"

		enum PIPE
		{
			PIPE_MAIN,
			PIPE_BILLBOARDS,
			PIPE_COUNT
		};

		struct Vertex
		{
			short _pos[4]; // {xyz, phaseShift}.
			short _tc[4]; // {tc0, center.xz}.
		};
		VERUS_TYPEDEFS(Vertex);

		struct PerInstanceData
		{
			short _patchPos[4];
		};
		VERUS_TYPEDEFS(PerInstanceData);

		struct Patch
		{
			short _i;
			short _j;
			short _h;
			short _type;
		};
		VERUS_TYPEDEFS(Patch);

		class Magnet
		{
		public:
			Point3 _pos;
			float  _radius = 1;
			float  _radiusSq = 1;
			float  _radiusSqInv = 1;
			float  _strength = 1;
			bool   _active = false;
		};
		VERUS_TYPEDEFS(Magnet);

		static const int s_maxBushTypes = 16;

	private:
		static CGI::ShaderPwn s_shader;
		static UB_GrassVS     s_ubGrassVS;
		static UB_GrassFS     s_ubGrassFS;

		CGI::GeometryPwn              _geo;
		CGI::PipelinePwns<PIPE_COUNT> _pipe;
		CGI::TexturePwn               _tex;
		CGI::TextureRAM               _texLoaded[s_maxBushTypes];
		PTerrain                      _pTerrain = nullptr;
		Vector<Vertex>                _vPatchMeshVB;
		Vector<UINT16>                _vPatchMeshIB;
		Vector<PerInstanceData>       _vInstanceBuffer;
		Vector<UINT32>                _vTextureSubresData;
		Vector<Patch>                 _vPatches;
		Vector<Magnet>                _vMagnets;
		Physics::Spring               _warpSpring = Physics::Spring(55, 2.5f);
		float                         _turbulence = 0;
		int                           _mapSide = 0;
		int                           _mapShift = 0;
		int                           _vertCount = 0;
		int                           _bbVertCount = 0;
		int                           _instanceCount = 0;
		int                           _visiblePatchCount = 0;
		CGI::CSHandle                 _cshVS;
		CGI::CSHandle                 _cshFS;
		char                          _bushTexCoords[s_maxBushTypes][8][2];
		UINT32                        _bushMask = 0;
		float                         _phase = 0;

	public:
		Grass();
		~Grass();

		static void InitStatic();
		static void DoneStatic();

		void Init(RTerrain terrain, CSZ atlasUrl = nullptr);
		void Done();

		void ResetInstanceCount();
		void Update();
		void Layout();
		void Draw();

		virtual void QuadtreeIntegral_OnElementDetected(const short ij[2], RcPoint3 center) override;
		virtual void QuadtreeIntegral_GetHeights(const short ij[2], float height[2]) override;

		VERUS_P(void CreateBuffers());

		void ResetAllTextures();
		void LoadLayersFromFile(CSZ url = nullptr);
		void SetTexture(int layer, CSZ url, CSZ url2 = nullptr);
		VERUS_P(void OnTextureLoaded(int layer));
		void SaveTexture(CSZ url);

		int BeginMagnet(RcPoint3 pos, float radius);
		void EndMagnet(int index);
		void UpdateMagnet(int index, RcPoint3 pos, float radius = 0);
	};
	VERUS_TYPEDEFS(Grass);
}
