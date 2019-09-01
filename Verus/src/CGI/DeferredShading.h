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
			//#include "../Cg/DS.inc.cg"
			//#include "../Cg/DS_Compose.inc.cg"

			enum S
			{
				S_LIGHT,
				S_COMPOSE,
				S_MAX
			};

			enum SB
			{
				SB_ACC_DIR,
				SB_ACC_NON_DIR,
				SB_ACC_WIREFRAME_DIR,
				SB_ACC_WIREFRAME_NON_DIR,
				SB_COMPOSE,
				SB_COMPOSE_AA,
				SB_MAX
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

			ShaderPwns<S_MAX>       _shader;
			TexturePwns<TEX_MAX>    _tex;
			UINT64                  _frame = 0;
			int                     _width = 0;
			int                     _height = 0;
			int                     _renderPassID = -1;
			int                     _framebufferID = -1;
			bool                    _activeGeometryPass = false;
			bool                    _activeLightingPass = false;

		public:
			DeferredShading();
			~DeferredShading();

			void Init();
			void InitGBuffers(int w, int h);
			void Done();

			void Draw(int gbuffer);

			int GetWidth() const { return _width; }
			int GetHeight() const { return _height; }

			bool IsActiveGeometryPass() const { return _activeGeometryPass; }
			bool IsActiveLightingPass() const { return _activeLightingPass; }

			void BeginGeometryPass(bool onlySetRT = false, bool spriteBaking = false);
			void EndGeometryPass(bool resetRT = false);
			void BeginLightingPass();
			void EndLightingPass();
			void Compose();
			void AntiAliasing();

			static bool IsLightUrl(CSZ url);
			//static CB_PerMesh& GetCB_PerMesh() { return ms_cbPerMesh; }
			void OnNewLightType(LightType type, bool wireframe = false);
			void UpdateBufferPerMesh();
			void UpdateBufferPerFrame();
			void EndLights();

			void Load();

			TexturePtr GetGBuffer(int index);
		};
		VERUS_TYPEDEFS(DeferredShading);
	}
}
