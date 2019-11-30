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
#include "../Shaders/DS_Compose.inc.hlsl"

			enum S
			{
				S_LIGHT,
				S_COMPOSE,
				S_MAX
			};

			enum PIPE
			{
				PIPE_INSTANCED_DIR,
				PIPE_INSTANCED_OMNI,
				PIPE_INSTANCED_SPOT,
				PIPE_COMPOSE,
				PIPE_QUAD,
				PIPE_MAX
			};

			enum TEX
			{
				TEX_GBUFFER_0, // Albedo RGB, A = motion blur.
				TEX_GBUFFER_1, // Depth.
				TEX_GBUFFER_2, // RG - Normals in screen space, Emission, Metallicity.
				TEX_GBUFFER_3, // LamScale, LamBias, Specular, Gloss.
				TEX_LIGHT_ACC_DIFF,
				TEX_LIGHT_ACC_SPEC,
				TEX_MAX
			};

			static UB_PerFrame   s_ubPerFrame;
			static UB_TexturesFS s_ubTexturesFS;
			static UB_PerMeshVS  s_ubPerMeshVS;
			static UB_ShadowFS   s_ubShadowFS;
			static UB_PerObject  s_ubPerObject;
			static UB_ComposeVS  s_ubComposeVS;
			static UB_ComposeFS  s_ubComposeFS;

			ShaderPwns<S_MAX>      _shader;
			PipelinePwns<PIPE_MAX> _pipe;
			TexturePwns<TEX_MAX>   _tex;
			TexturePtr             _texShadowAtmo;
			UINT64                 _frame = 0;
			int                    _rp = -1;
			int                    _fb = -1;
			int                    _csidLight = -1;
			int                    _csidCompose = -1;
			int                    _csidQuad[6];
			bool                   _activeGeometryPass = false;
			bool                   _activeLightingPass = false;
			bool                   _async_initPipe = false;

		public:
			DeferredShading();
			~DeferredShading();

			void Init();
			void InitGBuffers(int w, int h);
			void InitByAtmosphere(TexturePtr texShadow);
			void Done();

			void OnSwapChainResized(bool init, bool done);

			static bool IsLoaded();

			void Draw(int gbuffer);

			bool IsActiveGeometryPass() const { return _activeGeometryPass; }
			bool IsActiveLightingPass() const { return _activeLightingPass; }

			int GetRenderPassID() const { return _rp; }

			void BeginGeometryPass(bool onlySetRT = false, bool spriteBaking = false);
			void EndGeometryPass(bool resetRT = false);
			bool BeginLightingPass();
			void EndLightingPass();
			void Compose(RcVector4 bgColor = Vector4(0));
			void AntiAliasing();

			static bool IsLightUrl(CSZ url);
			static UB_PerMeshVS& GetUbPerMeshVS() { return s_ubPerMeshVS; }
			void OnNewLightType(LightType type, bool wireframe = false);
			void UpdateUniformBufferPerFrame();
			void BindDescriptorsPerMeshVS();

			void Load();

			TexturePtr GetGBuffer(int index);
		};
		VERUS_TYPEDEFS(DeferredShading);
	}
}
