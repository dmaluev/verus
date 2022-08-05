// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		struct RendererDelegate
		{
			virtual void Renderer_OnDraw() = 0;
			virtual void Renderer_OnDrawView(RcViewDesc viewDesc) = 0;
		};
		VERUS_TYPEDEFS(RendererDelegate);

		class Renderer : public Singleton<Renderer>, public Object
		{
#include "../Shaders/GenerateMips.inc.hlsl"
#include "../Shaders/GenerateCubeMapMips.inc.hlsl"
#include "../Shaders/Quad.inc.hlsl"

			enum SHADER
			{
				SHADER_GENERATE_MIPS,
				SHADER_GENERATE_CUBE_MAP_MIPS,
				SHADER_QUAD,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_GENERATE_MIPS,
				PIPE_GENERATE_MIPS_EXPOSURE,
				PIPE_GENERATE_CUBE_MAP_MIPS,
				PIPE_OFFSCREEN_COLOR,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_OFFSCREEN_COLOR,
				TEX_DEPTH_STENCIL,
				TEX_COUNT
			};

			struct Utilization
			{
				String _name;
				INT64  _value = 0;
				INT64  _total = 0;
				float  _fraction = 0;
			};
			VERUS_TYPEDEFS(Utilization);

			struct Vertex
			{
				glm::vec2 _pos;
			};

			Vector<Utilization>      _vUtilization;
			App::PWindow             _pMainWindow = nullptr;
			PBaseRenderer            _pBaseRenderer = nullptr;
			PRendererDelegate        _pRendererDelegate = nullptr;
			CommandBufferPwn         _commandBuffer;
			GeometryPwn              _geoQuad;
			ShaderPwns<SHADER_COUNT> _shader;
			PipelinePwns<PIPE_COUNT> _pipe;
			TexturePwns<TEX_COUNT>   _tex;
			DeferredShading          _ds;
			UINT64                   _frameCount = 0;
			Gapi                     _gapi = Gapi::unknown;
			int                      _screenSwapChainWidth = 0;
			int                      _screenSwapChainHeight = 0;
			int                      _combinedSwapChainWidth = 0;
			int                      _combinedSwapChainHeight = 0;
			ViewType                 _currentViewType = ViewType::none;
			int                      _currentViewWidth = 0;
			int                      _currentViewHeight = 0;
			int                      _currentViewX = 0;
			int                      _currentViewY = 0;
			float                    _fps = 30;
			float                    _exposure[2] = {}; // Linear and EV.
			RPHandle                 _rphScreenSwapChain;
			RPHandle                 _rphScreenSwapChainWithDepth;
			Vector<FBHandle>         _fbhScreenSwapChain;
			Vector<FBHandle>         _fbhScreenSwapChainWithDepth;
			RPHandle                 _rphOffscreen;
			RPHandle                 _rphOffscreenWithDepth;
			FBHandle                 _fbhOffscreen;
			FBHandle                 _fbhOffscreenWithDepth;
			CSHandle                 _cshOffscreenColor;
			UB_GenerateMips          _ubGenerateMips;
			UB_GenerateCubeMapMips   _ubGenerateCubeMapMips;
			UB_QuadVS                _ubQuadVS;
			UB_QuadFS                _ubQuadFS;
			bool                     _autoExposure = true;
			bool                     _allowInitShaders = true;
			bool                     _showUtilization = false;

		public:
			Renderer();
			~Renderer();

			// Device-specific:
			PBaseRenderer operator->();
			static bool IsLoaded();

			void Init(PRendererDelegate pDelegate, bool allowInitShaders = true);
			void InitCmd();
			void Done();

			// Frame cycle:
			void Update();
			void Draw();
			void BeginFrame();
			void AcquireSwapChainImage();
			void EndFrame();

			// Window:
			App::PWindow GetMainWindow() const { return _pMainWindow; }
			App::PWindow SetMainWindow(App::PWindow p) { return Utils::Swap(_pMainWindow, p); }
			bool OnWindowSizeChanged(int w, int h);

			// Swap chain & view:
			void UpdateCombinedSwapChainSize();
			VERUS_P(void OnScreenSwapChainResized(bool init, bool done));
			int GetScreenSwapChainWidth() const { return _screenSwapChainWidth; }
			int GetScreenSwapChainHeight() const { return _screenSwapChainHeight; }
			int GetCombinedSwapChainWidth() const { return _combinedSwapChainWidth; }
			int GetCombinedSwapChainHeight() const { return _combinedSwapChainHeight; }
			ViewType GetCurrentViewType() const { return _currentViewType; }
			int GetCurrentViewWidth() const { return _currentViewWidth; }
			int GetCurrentViewHeight() const { return _currentViewHeight; }
			int GetCurrentViewX() const { return _currentViewX; }
			int GetCurrentViewY() const { return _currentViewY; }
			float GetCurrentViewAspectRatio() const;
			static Format GetSwapChainFormat() { return Format::srgbB8G8R8A8; }

			// Simple (fullscreen) quad:
			void DrawQuad(PBaseCommandBuffer pCB = nullptr);
			void DrawOffscreenColor(PBaseCommandBuffer pCB = nullptr, bool endRenderPass = true);
			void DrawOffscreenColorSwitchRenderPass(PBaseCommandBuffer pCB = nullptr);

			CommandBufferPtr GetCommandBuffer() const { return _commandBuffer; }
			TexturePtr GetTexOffscreenColor() const;
			TexturePtr GetTexDepthStencil() const;
			RDeferredShading GetDS() { return _ds; }

			void OnShaderError(CSZ s);
			void OnShaderWarning(CSZ s);

			// ImGui:
			virtual void ImGuiSetCurrentContext(ImGuiContext* pContext);
			void ImGuiUpdateStyle();

			// Frame rate:
			float GetFps() const { return _fps; }
			UINT64 GetFrameCount() const { return _frameCount; }

			// RenderPass & Framebuffer:
			RPHandle GetRenderPassHandle_ScreenSwapChain() const;
			RPHandle GetRenderPassHandle_ScreenSwapChainWithDepth() const;
			RPHandle GetRenderPassHandle_Offscreen() const;
			RPHandle GetRenderPassHandle_OffscreenWithDepth() const;
			RPHandle GetRenderPassHandle_Auto() const;
			RPHandle GetRenderPassHandle_AutoWithDepth() const;
			FBHandle GetFramebufferHandle_ScreenSwapChain(int index) const;
			FBHandle GetFramebufferHandle_ScreenSwapChainWithDepth(int index) const;
			FBHandle GetFramebufferHandle_Offscreen() const;
			FBHandle GetFramebufferHandle_OffscreenWithDepth() const;
			FBHandle GetFramebufferHandle_Auto(int index) const;
			FBHandle GetFramebufferHandle_AutoWithDepth(int index) const;

			// Generate mips:
			ShaderPtr GetShaderGenerateMips() const;
			ShaderPtr GetShaderGenerateCubeMapMips() const;
			PipelinePtr GetPipelineGenerateMips(bool exposure = false) const;
			PipelinePtr GetPipelineGenerateCubeMapMips() const;
			UB_GenerateMips& GetUbGenerateMips();
			UB_GenerateCubeMapMips& GetUbGenerateCubeMapMips();

			// Quad:
			GeometryPtr GetGeoQuad() const;
			ShaderPtr GetShaderQuad() const;
			UB_QuadVS& GetUbQuadVS();
			UB_QuadFS& GetUbQuadFS();
			void ResetQuadMultiplexer();

			// Exposure:
			bool IsAutoExposureEnabled() const { return _autoExposure; }
			void EnableAutoExposure(bool b = true) { _autoExposure = b; }
			float GetExposure() const { return _exposure[0]; }
			float GetExposureValue() const { return _exposure[1]; }
			void SetExposureValue(float ev);

			void ToggleUtilization() { _showUtilization = !_showUtilization; }
			void UpdateUtilization();
			void AddUtilization(CSZ name, INT64 value, INT64 total);
		};
		VERUS_TYPEDEFS(Renderer);
	}
}
