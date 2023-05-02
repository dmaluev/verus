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
#include "../Shaders/DS_Ambient.inc.hlsl"
#include "../Shaders/DS_AmbientNode.inc.hlsl"
#include "../Shaders/DS_Compose.inc.hlsl"
#include "../Shaders/DS_Reflection.inc.hlsl"
#include "../Shaders/DS_BakeSprites.inc.hlsl"

			enum SHADER
			{
				SHADER_LIGHT,
				SHADER_AMBIENT,
				SHADER_AMBIENT_NODE,
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
				PIPE_AMBIENT,
				PIPE_AMBIENT_NODE_MUL,
				PIPE_AMBIENT_NODE_ADD,
				PIPE_COMPOSE,
				PIPE_REFLECTION,
				PIPE_REFLECTION_DEBUG,
				PIPE_TONE_MAPPING,
				PIPE_QUAD,
				PIPE_BAKE_SPRITES,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_GBUFFER_0, // {Albedo.rgb, SSSHue}.
				TEX_GBUFFER_1, // {Normal.xy, Emission, MotionBlur}.
				TEX_GBUFFER_2, // {Occlusion, Roughness, Metallic, WrapDiffuse}.
				TEX_GBUFFER_3, // {Tangent.xy, AnisoSpec, RoughDiffuse}.

				TEX_LIGHT_ACC_AMBIENT,
				TEX_LIGHT_ACC_DIFFUSE,
				TEX_LIGHT_ACC_SPECULAR,

				TEX_COMPOSED_A,
				TEX_COMPOSED_B,

				TEX_COUNT
			};

			static UB_PerView               s_ubPerView;
			static UB_TexturesFS            s_ubTexturesFS;
			static UB_PerMeshVS             s_ubPerMeshVS;
			static UB_ShadowFS              s_ubShadowFS;
			static UB_PerObject             s_ubPerObject;

			static UB_AmbientVS             s_ubAmbientVS;
			static UB_AmbientFS             s_ubAmbientFS;

			static UB_AmbientNodePerView    s_ubAmbientNodePerView;
			static UB_AmbientNodeTexturesFS s_ubAmbientNodeTexturesFS;
			static UB_AmbientNodePerMeshVS  s_ubAmbientNodePerMeshVS;
			static UB_AmbientNodePerObject  s_ubAmbientNodePerObject;

			static UB_ComposeVS             s_ubComposeVS;
			static UB_ComposeFS             s_ubComposeFS;

			static UB_ReflectionVS          s_ubReflectionVS;
			static UB_ReflectionFS          s_ubReflectionFS;

			static UB_BakeSpritesVS         s_ubBakeSpritesVS;
			static UB_BakeSpritesFS         s_ubBakeSpritesFS;

			Vector4                  _backgroundColor = Vector4(0);
			ShaderPwns<SHADER_COUNT> _shader;
			PipelinePwns<PIPE_COUNT> _pipe;
			TexturePwns<TEX_COUNT>   _tex;
			TexturePtr               _texTerrainHeightmap;
			TexturePtr               _texTerrainBlend;
			UINT64                   _frame = 0;

			RPHandle                 _rph;
			RPHandle                 _rphCompose;
			RPHandle                 _rphForwardRendering;
			RPHandle                 _rphReflection;
			RPHandle                 _rphBakeSprites;

			FBHandle                 _fbh;
			FBHandle                 _fbhCompose;
			FBHandle                 _fbhForwardRendering;
			FBHandle                 _fbhReflection;
			FBHandle                 _fbhBakeSprites;

			CSHandle                 _cshLight;
			CSHandle                 _cshLightShadow;
			CSHandle                 _cshAmbient;
			CSHandle                 _cshAmbientNode;
			CSHandle                 _cshCompose;
			CSHandle                 _cshReflection;
			CSHandle                 _cshToneMapping;
			CSHandle                 _cshQuad[7];
			CSHandle                 _cshBakeSprites;

			int                      _terrainMapSide = 1;

			bool                     _activeGeometryPass = false;
			bool                     _activeLightingPass = false;
			bool                     _activeForwardRendering = false;
			bool                     _async_initPipe = false;

		public:
			DeferredShading();
			~DeferredShading();

			void Init();
			void InitGBuffers(int w, int h);
			void InitByBloom(TexturePtr tex);
			void InitByCascadedShadowMapBaker(TexturePtr texShadow);
			void InitByTerrain(TexturePtr texHeightmap, TexturePtr texBlend, int mapSide);

			void Done();

			void OnSwapChainResized(bool init, bool done);

			static bool IsLoaded();

			void ResetInstanceCount();
			void Draw(int gbuffer,
				RcVector4 rMultiplexer = Vector4(1, 0, 0, 0),
				RcVector4 gMultiplexer = Vector4(0, 1, 0, 0),
				RcVector4 bMultiplexer = Vector4(0, 0, 1, 0),
				RcVector4 aMultiplexer = Vector4(0, 0, 0, 1));

			RPHandle GetRenderPassHandle() const { return _rph; }
			RPHandle GetRenderPassHandle_ForwardRendering() const { return _rphForwardRendering; }

			// Steps:
			void BeginGeometryPass();
			void EndGeometryPass();
			bool BeginLightingPass(bool ambient = true, bool terrainOcclusion = true);
			void EndLightingPass();
			void BeginComposeAndForwardRendering(bool underwaterMask = true);
			void EndComposeAndForwardRendering();
			void DrawReflection();
			void ToneMapping();
			bool IsActiveGeometryPass() const { return _activeGeometryPass; }
			bool IsActiveLightingPass() const { return _activeLightingPass; }
			bool IsActiveForwardRendering() const { return _activeForwardRendering; }

			// Lighting:
			static bool IsLightUrl(CSZ url);
			void OnNewLightType(CommandBufferPtr cb, LightType type, bool wireframe = false);
			void BindDescriptorsPerMeshVS(CommandBufferPtr cb);
			static UB_PerMeshVS& GetUbPerMeshVS() { return s_ubPerMeshVS; }
			CSHandle BindDescriptorSetTexturesForVSM(TexturePtr texShadow);
			void BindDescriptorsForVSM(CommandBufferPtr cb, int firstInstance, CSHandle complexSetHandle, float presence);

			// Ambient:
			void OnNewAmbientNodePriority(CommandBufferPtr cb, int firstInstance, bool add);
			void BindDescriptorsAmbientNodePerMeshVS(CommandBufferPtr cb);
			static UB_AmbientNodePerMeshVS& GetUbAmbientNodePerMeshVS() { return s_ubAmbientNodePerMeshVS; }

			void Load();

			ShaderPtr GetLightShader() const;
			ShaderPtr GetAmbientNodeShader() const;

			TexturePtr GetGBuffer(int index) const;
			TexturePtr GetLightAccAmbientTexture() const;
			TexturePtr GetLightAccDiffuseTexture() const;
			TexturePtr GetLightAccSpecularTexture() const;
			TexturePtr GetComposedTextureA() const;
			TexturePtr GetComposedTextureB() const;

			static Vector4 GetClearValueForGBuffer0(float albedo = 0);
			static Vector4 GetClearValueForGBuffer1(float normal = 0, float motionBlur = 1);
			static Vector4 GetClearValueForGBuffer2(float roughness = 0);
			static Vector4 GetClearValueForGBuffer3(float tangent = 0);

			void SetBackgroundColor(RcVector4 backgroundColor) { _backgroundColor = backgroundColor; }

			void BakeSprites(TexturePtr texGBufferIn[4], TexturePtr texGBufferOut[4], PBaseCommandBuffer pCB = nullptr);
			void BakeSpritesCleanup();
		};
		VERUS_TYPEDEFS(DeferredShading);
	}
}
