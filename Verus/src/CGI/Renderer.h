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
			UINT64            _numFrames = 0;
			Gapi              _gapi = Gapi::unknown;
			float             _fps = 30;
			int               _rpSwapChain = 0;
			int               _rpSwapChainDepth = 0;
			Vector<int>       _fbSwapChain;
			Vector<int>       _fbSwapChainDepth;
			UB_GenerateMips   _ubGenerateMips;
			UB_Quad           _ubQuad;

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

			// Simple (fullscreen) quad:
			void DrawQuad(PBaseCommandBuffer pCB = nullptr);

			RDeferredShading GetDS() { return _ds; }
			CommandBufferPtr GetCommandBuffer() const { return _commandBuffer; }
			GeometryPtr GetGeoQuad() const { return _geoQuad; }
			TexturePtr GetTexDepthStencil() const { return _texDepthStencil; }

			void OnShaderError(CSZ s);
			void OnShaderWarning(CSZ s);

			App::PWindow GetMainWindow() const { return _pMainWindow; }
			App::PWindow SetMainWindow(App::PWindow p) { return Utils::Swap(_pMainWindow, p); }
			float GetWindowAspectRatio() const;

			// Frame rate:
			float GetFps() const { return _fps; }
			UINT64 GetNumFrames() const { return _numFrames; }

			int GetRenderPass_SwapChain() const { return _rpSwapChain; }
			int GetRenderPass_SwapChainDepth() const { return _rpSwapChainDepth; }
			int GetFramebuffer_SwapChain(int index) const { return _fbSwapChain[index]; }
			int GetFramebuffer_SwapChainDepth(int index) const { return _fbSwapChainDepth[index]; }

			PipelinePtr GetPipelineGenerateMips() { return _pipeGenerateMips; }
			ShaderPtr GetShaderGenerateMips() { return _shader[S_GENERATE_MIPS]; }
			UB_GenerateMips& GetUbGenerateMips() { return _ubGenerateMips; }

			ShaderPtr GetShaderQuad() { return _shader[S_QUAD]; }
			UB_Quad& GetUbQuad() { return _ubQuad; }
		};
		VERUS_TYPEDEFS(Renderer);
	}
}
