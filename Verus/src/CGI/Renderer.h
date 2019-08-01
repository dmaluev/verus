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
			Vector4           _clearColor = Vector4(0);
			App::PWindow      _pMainWindow = nullptr;
			PBaseRenderer     _pBaseRenderer = nullptr;
			PRendererDelegate _pRendererDelegate = nullptr;
			CommandBufferPwn  _commandBuffer;
			TexturePwn        _texDepthStencil;
			UINT64            _numFrames = 0;
			Gapi              _gapi = Gapi::unknown;
			float             _fps = 30;
			int               _rpSwapChain = 0;
			int               _rpSwapChainDepth = 0;
			int               _rpDS = 0;
			Vector<int>       _fbSwapChain;
			Vector<int>       _fbSwapChainDepth;

		public:
			Renderer();
			~Renderer();

			// Device-specific:
			PBaseRenderer operator->();
			static bool IsLoaded();

			void Init(PRendererDelegate pDelegate);
			void Done();

			// Frame cycle:
			void Draw();
			void Present();

			CommandBufferPtr GetCommandBuffer() const { return _commandBuffer; }
			TexturePwn GetTexDepthStencil() const { return _texDepthStencil; }

			void OnShaderError(CSZ s);
			void OnShaderWarning(CSZ s);

			App::PWindow GetMainWindow() const { return _pMainWindow; }
			App::PWindow SetMainWindow(App::PWindow p) { return Utils::Swap(_pMainWindow, p); }
			float GetWindowAspectRatio() const;

			RcVector4 GetClearColor() const { return _clearColor; }
			void SetClearColor(RcVector4 color) { _clearColor = color; }

			// Frame rate:
			float GetFps() const { return _fps; }
			UINT64 GetNumFrames() const { return _numFrames; }

			int GetRenderPass_SwapChain() const { return _rpSwapChain; }
			int GetRenderPass_SwapChainDepth() const { return _rpSwapChainDepth; }
			int GetRenderPass_DS() const { return _rpDS; }

			int GetFramebuffer_SwapChain(int index) const { return _fbSwapChain[index]; }
			int GetFramebuffer_SwapChainDepth(int index) const { return _fbSwapChainDepth[index]; }
		};
		VERUS_TYPEDEFS(Renderer);
	}
}
