// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		struct RendererDelegate
		{
			virtual void Renderer_OnDraw() = 0;
			virtual void Renderer_OnDrawOverlay() = 0;
			virtual void Renderer_OnPresent() = 0;
			virtual void Renderer_OnDrawCubeMap() {}
		};
		VERUS_TYPEDEFS(RendererDelegate);

		class Renderer : public Singleton<Renderer>, public Object
		{
#include "../Shaders/GenerateMips.inc.hlsl"
#include "../Shaders/Quad.inc.hlsl"

			enum SHADER
			{
				SHADER_GENERATE_MIPS,
				SHADER_QUAD,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_GENERATE_MIPS,
				PIPE_GENERATE_MIPS_EXPOSURE,
				PIPE_OFFSCREEN_COLOR,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_OFFSCREEN_COLOR,
				TEX_DEPTH_STENCIL,
				TEX_DEPTH_STENCIL_SCALED,
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
			int                      _swapChainWidth = 0;
			int                      _swapChainHeight = 0;
			float                    _fps = 30;
			float                    _exposure[2] = {}; // Linear and EV.
			RPHandle                 _rphSwapChain;
			RPHandle                 _rphSwapChainWithDepth;
			Vector<FBHandle>         _fbhSwapChain;
			Vector<FBHandle>         _fbhSwapChainWithDepth;
			RPHandle                 _rphOffscreen;
			RPHandle                 _rphOffscreenWithDepth;
			FBHandle                 _fbhOffscreen;
			FBHandle                 _fbhOffscreenWithDepth;
			CSHandle                 _cshOffscreenColor;
			UB_GenerateMips          _ubGenerateMips;
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
			void Present();

			bool OnWindowSizeChanged(int w, int h);
			VERUS_P(void OnSwapChainResized(bool init, bool done));
			int GetSwapChainWidth() const { return _swapChainWidth; }
			int GetSwapChainHeight() const { return _swapChainHeight; }

			// Simple (fullscreen) quad:
			void DrawQuad(PBaseCommandBuffer pCB = nullptr);
			void DrawOffscreenColor(PBaseCommandBuffer pCB = nullptr, bool endRenderPass = true);
			void DrawOffscreenColorSwitchRenderPass(PBaseCommandBuffer pCB = nullptr);

			RDeferredShading GetDS() { return _ds; }
			CommandBufferPtr GetCommandBuffer() const { return _commandBuffer; }
			TexturePtr GetTexOffscreenColor() const;
			TexturePtr GetTexDepthStencil(bool scaled = true) const;

			void OnShaderError(CSZ s);
			void OnShaderWarning(CSZ s);

			// Window:
			App::PWindow GetMainWindow() const { return _pMainWindow; }
			App::PWindow SetMainWindow(App::PWindow p) { return Utils::Swap(_pMainWindow, p); }
			float GetSwapChainAspectRatio() const;

			// ImGui:
			virtual void ImGuiSetCurrentContext(ImGuiContext* pContext);
			void ImGuiUpdateStyle();

			// Frame rate:
			float GetFps() const { return _fps; }
			UINT64 GetFrameCount() const { return _frameCount; }

			// RenderPass & Framebuffer:
			RPHandle GetRenderPassHandle_SwapChain() const;
			RPHandle GetRenderPassHandle_SwapChainWithDepth() const;
			RPHandle GetRenderPassHandle_Offscreen() const;
			RPHandle GetRenderPassHandle_OffscreenWithDepth() const;
			RPHandle GetRenderPassHandle_Auto() const;
			RPHandle GetRenderPassHandle_AutoWithDepth() const;
			FBHandle GetFramebufferHandle_SwapChain(int index) const;
			FBHandle GetFramebufferHandle_SwapChainWithDepth(int index) const;
			FBHandle GetFramebufferHandle_Offscreen(int index) const;
			FBHandle GetFramebufferHandle_OffscreenWithDepth(int index) const;
			FBHandle GetFramebufferHandle_Auto(int index) const;
			FBHandle GetFramebufferHandle_AutoWithDepth(int index) const;

			// Generate mips:
			ShaderPtr GetShaderGenerateMips() const;
			PipelinePtr GetPipelineGenerateMips(bool exposure = false) const;
			UB_GenerateMips& GetUbGenerateMips();

			// Quad:
			GeometryPtr GetGeoQuad() const;
			ShaderPtr GetShaderQuad() const;
			UB_QuadVS& GetUbQuadVS();
			UB_QuadFS& GetUbQuadFS();

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
