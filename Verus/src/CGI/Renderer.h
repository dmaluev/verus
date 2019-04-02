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
			UINT64            _numFrames = 0;
			Gapi              _gapi = Gapi::unknown;

		public:
			Renderer();
			~Renderer();

			// Device-specific:
			PBaseRenderer operator->();
			static bool IsLoaded();

			void Init(PRendererDelegate pDelegate);
			void Done();

			void Draw();
			void Present();

			App::PWindow GetMainWindow() const { return _pMainWindow; }
			App::PWindow SetMainWindow(App::PWindow p) { return Utils::Swap(_pMainWindow, p); }

			RcVector4 GetClearColor() const { return _clearColor; }
			void SetClearColor(RcVector4 color) { _clearColor = color; }
		};
		VERUS_TYPEDEFS(Renderer);
	}
}
