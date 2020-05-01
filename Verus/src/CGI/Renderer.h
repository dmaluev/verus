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

			enum S
			{
				S_GENERATE_MIPS,
				S_QUAD,
				S_MAX
			};

			struct Vertex
			{
				glm::vec2 _pos;
			};

			App::PWindow      _pMainWindow = nullptr;
			PBaseRenderer     _pBaseRenderer = nullptr;
			PRendererDelegate _pRendererDelegate = nullptr;
			CommandBufferPwn  _commandBuffer;
			GeometryPwn       _geoQuad;
			ShaderPwns<S_MAX> _shader;
			PipelinePwn       _pipeGenerateMips;
			TexturePwn        _texDepthStencil;
			DeferredShading   _ds;
			UINT64            _frameCount = 0;
			Gapi              _gapi = Gapi::unknown;
			int               _swapChainWidth = 0;
			int               _swapChainHeight = 0;
			float             _fps = 30;
			RPHandle          _rphSwapChain;
			RPHandle          _rphSwapChainDepth;
			Vector<FBHandle>  _fbhSwapChain;
			Vector<FBHandle>  _fbhSwapChainDepth;
			UB_GenerateMips   _ubGenerateMips;
			UB_QuadVS         _ubQuadVS;
			UB_QuadFS         _ubQuadFS;

		public:
			Renderer();
			~Renderer();

			// Device-specific:
			PBaseRenderer operator->();
			static bool IsLoaded();

			void Init(PRendererDelegate pDelegate);
			void InitCmd();
			void Done();

			// Frame cycle:
			void Draw();
			void Present();

			void OnWindowResized(int w, int h);
			VERUS_P(void OnSwapChainResized(bool init, bool done));
			int GetSwapChainWidth() const { return _swapChainWidth; }
			int GetSwapChainHeight() const { return _swapChainHeight; }

			// Simple (fullscreen) quad:
			void DrawQuad(PBaseCommandBuffer pCB = nullptr);

			RDeferredShading GetDS() { return _ds; }
			CommandBufferPtr GetCommandBuffer() const { return _commandBuffer; }
			TexturePtr GetTexDepthStencil() const { return _texDepthStencil; }

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
			RPHandle GetRenderPassHandle_SwapChain() const { return _rphSwapChain; }
			RPHandle GetRenderPassHandle_SwapChainDepth() const { return _rphSwapChainDepth; }
			FBHandle GetFramebufferHandle_SwapChain(int index) const { return _fbhSwapChain[index]; }
			FBHandle GetFramebufferHandle_SwapChainDepth(int index) const { return _fbhSwapChainDepth[index]; }

			// Generate mips:
			PipelinePtr GetPipelineGenerateMips() { return _pipeGenerateMips; }
			ShaderPtr GetShaderGenerateMips() { return _shader[S_GENERATE_MIPS]; }
			UB_GenerateMips& GetUbGenerateMips() { return _ubGenerateMips; }

			// Quad:
			GeometryPtr GetGeoQuad() const { return _geoQuad; }
			ShaderPtr GetShaderQuad() { return _shader[S_QUAD]; }
			UB_QuadVS& GetUbQuadVS() { return _ubQuadVS; }
			UB_QuadFS& GetUbQuadFS() { return _ubQuadFS; }
		};
		VERUS_TYPEDEFS(Renderer);
	}
}
