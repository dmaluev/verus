#pragma once

namespace verus
{
	namespace Game
	{
		class BaseGame : public Input::KeyMapperDelegate
		{
			struct Pimpl;
			Pimpl* _p = nullptr;
			AlignedAllocator _alloc;
			EngineInit       _engineInit;
			App::Window      _window;

		public:
			BaseGame();
			~BaseGame();

			void Initialize(VERUS_MAIN_DEFAULT_ARGS, App::Window::RcDesc desc = App::Window::Desc());
			void Run(bool relativeMouseMode = true);
			void Exit();

			virtual void BaseGame_UpdateSettings() {};
			virtual void BaseGame_LoadContent() = 0;
			virtual void BaseGame_UnloadContent() = 0;
			virtual void BaseGame_HandleInput() = 0;
			virtual void BaseGame_Update() = 0;
			virtual void BaseGame_Draw() = 0;
			virtual void BaseGame_DrawOverlay() = 0;
			virtual void BaseGame_OnWindowResized() {}
			virtual void BaseGame_OnActivated() {}
			virtual void BaseGame_OnDeactivated() {}
			virtual void BaseGame_OnMouseMove(float x, float y) {}
			virtual void BaseGame_OnKey(int scancode) {}
			virtual void BaseGame_OnChar(wchar_t c) {}
			virtual bool BaseGame_CanQuitEventLoop() { return true; }

			// Raw SDL events, for editor:
			virtual void BaseGame_SDL_OnMouseMotion(int x, int y) {}
			virtual void BaseGame_SDL_OnMouseButtonDown(int button) {}
			virtual void BaseGame_SDL_OnMouseDoubleClick(int button) {}
			virtual void BaseGame_SDL_OnMouseButtonUp(int button) {}
			virtual void BaseGame_SDL_OnMouseWheel(int delta) {}

			virtual void KeyMapper_OnMouseMove(int x, int y) override;
			virtual void KeyMapper_OnKey(int scancode) override;
			virtual void KeyMapper_OnChar(wchar_t c) override;

			REngineInit GetEngineInit() { return _engineInit; }

			App::RcWindow GetWindow() const { return _window; }

			// Camera:
			Scene::RCamera GetCamera();
			RSpirit GetCameraSpirit();

			// Configuration:
			void EnableBulletDebugDraw(bool b);
			void EnableDefaultCameraMovement(bool b);
			void EnableEscapeKeyExitGame(bool b);
			void EnableRawInputEvents(bool b);
			void ShowFPS(bool b);

			bool IsBulletDebugDrawEnabled() const;
			void BulletDebugDraw();
		};
		VERUS_TYPEDEFS(BaseGame);
	}
}
