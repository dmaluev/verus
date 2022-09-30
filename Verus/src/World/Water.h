// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		class Water : public Singleton<Water>, public Object
		{
		public:
#include "../Shaders/Water.inc.hlsl"
#include "../Shaders/WaterGen.inc.hlsl"

			enum SHADER
			{
				SHADER_MAIN,
				SHADER_GEN,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_GEN_HEIGHTMAP,
				PIPE_GEN_NORMALS,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_FOAM,
				TEX_SOURCE_HEIGHTMAP,
				TEX_GEN_HEIGHTMAP,
				TEX_GEN_NORMALS,
				TEX_REFLECTION,
				TEX_REFLECTION_DEPTH,
				TEX_COUNT
			};

		private:
			static const int s_maxHarmonics = 4;

			struct Vertex
			{
				glm::vec4 _pos;
			};

			static UB_WaterVS        s_ubWaterVS;
			static UB_WaterFS        s_ubWaterFS;
			static UB_Gen            s_ubGen;
			static UB_GenHeightmapFS s_ubGenHeightmapFS;
			static UB_GenNormalsFS   s_ubGenNormalsFS;

			Vector3                       _diffuseColorShallow = Vector3(0.01f, 0.2f, 0.25f);
			Vector3                       _diffuseColorDeep = Vector3(0.005f, 0.015f, 0.025f);
			PTerrain                      _pTerrain = nullptr;
			CGI::GeometryPwn              _geo;
			CGI::ShaderPwns<SHADER_COUNT> _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::CSHandle                 _cshWaterVS;
			CGI::CSHandle                 _cshWaterFS;
			CGI::RPHandle                 _rphGenHeightmap;
			CGI::FBHandle                 _fbhGenHeightmap;
			CGI::CSHandle                 _cshGenHeightmap;
			CGI::RPHandle                 _rphGenNormals;
			CGI::FBHandle                 _fbhGenNormals;
			CGI::CSHandle                 _cshGenNormals;
			CGI::RPHandle                 _rphReflection;
			CGI::FBHandle                 _fbhReflection;
			MainCamera                    _headCamera;
			PCamera                       _pPrevPassCamera = nullptr;
			PMainCamera                   _pPrevHeadCamera = nullptr;
			const int                     _genSide = 1024;
			int                           _gridWidth = 128;
			int                           _gridHeight = 512;
			int                           _indexCount = 0;
			const float                   _patchSide = 64;
			const float                   _fogDensity = 0.02f;
			float                         _phase = 0;
			float                         _wavePhase = 0;
			float                         _amplitudes[s_maxHarmonics];

		public:
			Water();
			~Water();

			void Init(RTerrain terrain);
			void Done();

			void Update();
			void Draw();

			void OnSwapChainResized();

			CGI::RPHandle GetRenderPassHandle() const { return _rphReflection; }
			void BeginPlanarReflection(CGI::PBaseCommandBuffer pCB = nullptr);
			void EndPlanarReflection(CGI::PBaseCommandBuffer pCB = nullptr);

			void GenerateTextures();
			VERUS_P(void GenerateHeightmapTexture());
			VERUS_P(void GenerateNormalsTexture());

			// Caustics are highlights on ocean floor.
			CGI::TexturePtr GetCausticsTexture() const;

			VERUS_P(void CreateWaterPlane());

			static float PhillipsSpectrum(float k);

			bool IsUnderwater() const;
			bool IsUnderwater(RcPoint3 eyePos) const;

			RcVector3 GetDiffuseColorShallow() const { return _diffuseColorShallow; }
			RcVector3 GetDiffuseColorDeep() const { return _diffuseColorDeep; }
			float GetFogDensity() const { return _fogDensity; }
		};
		VERUS_TYPEDEFS(Water);
	}
}
