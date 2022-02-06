// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		enum class LightType : int
		{
			none,
			dir,
			omni,
			spot
		};

		class DeferredShading : public Object
		{
#include "../Shaders/DS.inc.hlsl"
#include "../Shaders/DS_AO.inc.hlsl"
#include "../Shaders/DS_Compose.inc.hlsl"
#include "../Shaders/DS_Reflection.inc.hlsl"
#include "../Shaders/DS_BakeSprites.inc.hlsl"

			enum SHADER
			{
				SHADER_LIGHT,
				SHADER_AO,
				SHADER_COMPOSE,
				SHADER_REFLECTION,
				SHADER_BAKE_SPRITES,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_INSTANCED_DIR,
				PIPE_INSTANCED_OMNI,
				PIPE_INSTANCED_SPOT,
				PIPE_INSTANCED_AO,
				PIPE_COMPOSE,
				PIPE_REFLECTION,
				PIPE_TONE_MAPPING,
				PIPE_QUAD,
				PIPE_BAKE_SPRITES,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_GBUFFER_0, // Albedo, Spec.
				TEX_GBUFFER_1, // RG - Normals, Emission, Motion.
				TEX_GBUFFER_2, // LamScale, LamBias, Metallicity, Gloss.
				TEX_LIGHT_ACC_DIFF,
				TEX_LIGHT_ACC_SPEC,
				TEX_COMPOSED_A,
				TEX_COMPOSED_B,
				TEX_COUNT
			};

			static UB_PerFrame      s_ubPerFrame;
			static UB_TexturesFS    s_ubTexturesFS;
			static UB_PerMeshVS     s_ubPerMeshVS;
			static UB_ShadowFS      s_ubShadowFS;
			static UB_PerObject     s_ubPerObject;

			static UB_AOPerFrame    s_ubAOPerFrame;
			static UB_AOTexturesFS  s_ubAOTexturesFS;
			static UB_AOPerMeshVS   s_ubAOPerMeshVS;
			static UB_AOPerObject   s_ubAOPerObject;

			static UB_ComposeVS     s_ubComposeVS;
			static UB_ComposeFS     s_ubComposeFS;

			static UB_ReflectionVS  s_ubReflectionVS;
			static UB_ReflectionFS  s_ubReflectionFS;

			static UB_BakeSpritesVS s_ubBakeSpritesVS;
			static UB_BakeSpritesFS s_ubBakeSpritesFS;

			ShaderPwns<SHADER_COUNT> _shader;
			PipelinePwns<PIPE_COUNT> _pipe;
			TexturePwns<TEX_COUNT>   _tex;
			TexturePtr               _texShadowAtmo;
			TexturePtr               _texAO;
			UINT64                   _frame = 0;

			RPHandle                 _rph;
			RPHandle                 _rphAO;
			RPHandle                 _rphCompose;
			RPHandle                 _rphExtraCompose;
			RPHandle                 _rphReflection;
			RPHandle                 _rphBakeSprites;

			FBHandle                 _fbh;
			FBHandle                 _fbhAO;
			FBHandle                 _fbhCompose;
			FBHandle                 _fbhExtraCompose;
			FBHandle                 _fbhReflection;
			FBHandle                 _fbhBakeSprites;

			CSHandle                 _cshLight;
			CSHandle                 _cshAO;
			CSHandle                 _cshCompose;
			CSHandle                 _cshReflection;
			CSHandle                 _cshToneMapping;
			CSHandle                 _cshQuad[5];
			CSHandle                 _cshBakeSprites;

			float                    _lightGlossScale = 1;
			bool                     _activeGeometryPass = false;
			bool                     _activeLightingPass = false;
			bool                     _async_initPipe = false;

		public:
			DeferredShading();
			~DeferredShading();

			void Init();
			void InitGBuffers(int w, int h);
			void InitByAtmosphere(TexturePtr texShadow);
			void InitByBloom(TexturePtr tex);
			void InitBySsao(TexturePtr tex);
			void Done();

			void OnSwapChainResized(bool init, bool done);

			static bool IsLoaded();

			void ResetInstanceCount();
			void Draw(int gbuffer);

			bool IsActiveGeometryPass() const { return _activeGeometryPass; }
			bool IsActiveLightingPass() const { return _activeLightingPass; }

			RPHandle GetRenderPassHandle() const { return _rph; }
			RPHandle GetRenderPassHandle_ExtraCompose() const { return _rphExtraCompose; }

			void BeginGeometryPass(bool onlySetRT = false, bool spriteBaking = false);
			void EndGeometryPass(bool resetRT = false);
			bool BeginLightingPass();
			void EndLightingPass();
			void BeginAmbientOcclusion();
			void EndAmbientOcclusion();
			void BeginCompose(RcVector4 bgColor = Vector4(0));
			void EndCompose();
			void DrawReflection();
			void ToneMapping();

			static bool IsLightUrl(CSZ url);
			void OnNewLightType(CommandBufferPtr cb, LightType type, bool wireframe = false);
			void BindDescriptorsPerMeshVS(CommandBufferPtr cb);
			static UB_PerMeshVS& GetUbPerMeshVS() { return s_ubPerMeshVS; }

			void OnNewAOType(CommandBufferPtr cb, LightType type);
			void BindDescriptorsAOPerMeshVS(CommandBufferPtr cb);
			static UB_AOPerMeshVS& GetUbAOPerMeshVS() { return s_ubAOPerMeshVS; }

			void Load();

			TexturePtr GetGBuffer(int index);
			TexturePtr GetLightAccDiffTexture();
			TexturePtr GetLightAccSpecTexture();
			TexturePtr GetComposedTextureA();
			TexturePtr GetComposedTextureB();

			static Vector4 GetClearValueForGBuffer0();
			static Vector4 GetClearValueForGBuffer1();
			static Vector4 GetClearValueForGBuffer2();

			void BakeSprites(TexturePtr texGBufferIn[3], TexturePtr texGBufferOut[3], PBaseCommandBuffer pCB = nullptr);
			void BakeSpritesCleanup();

			// For Editor:
			float GetLightGlossScale() const { return _lightGlossScale; }
			void SetLightGlossScale(float s) { _lightGlossScale = s; }
		};
		VERUS_TYPEDEFS(DeferredShading);
	}
}
