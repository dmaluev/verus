// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class BaseGame : public Input::InputFocus
		{
			struct Pimpl;
			Pimpl* _p = nullptr;
			AlignedAllocator _alloc;
			EngineInit       _engineInit;
			App::Window      _window;
			bool             _restartApp = false;

		public:
			BaseGame();
			~BaseGame();

			void Initialize(VERUS_MAIN_DEFAULT_ARGS, App::Window::RcDesc windowDesc = App::Window::Desc());
			void Loop(bool relativeMouseMode = true);
			void Exit();

			virtual void BaseGame_UpdateSettings(App::Window::RDesc windowDesc) {}
			virtual void BaseGame_LoadContent() = 0;
			virtual void BaseGame_UnloadContent() = 0;
			virtual void BaseGame_Update() = 0;
			virtual void BaseGame_Draw() = 0;
			virtual void BaseGame_OnWindowMoved() {}
			virtual void BaseGame_OnWindowSizeChanged() {}
			virtual void BaseGame_OnActivated() {}
			virtual void BaseGame_OnDeactivated() {}
			virtual bool BaseGame_CanQuitEventLoop() { return true; }

			// Raw SDL events, for editor:
			virtual void BaseGame_SDL_OnMouseMotion(int x, int y) {}
			virtual void BaseGame_SDL_OnMouseButtonDown(int button) {}
			virtual void BaseGame_SDL_OnMouseDoubleClick(int button) {}
			virtual void BaseGame_SDL_OnMouseButtonUp(int button) {}
			virtual void BaseGame_SDL_OnMouseWheel(int delta) {}
			virtual bool BaseGame_SDL_OnKeyboardShortcut(int sym, int mod) { return false; }

			virtual void InputFocus_OnMouseMove(float dx, float dy) override;

			REngineInit GetEngineInit() { return _engineInit; }

			App::RcWindow GetWindow() const { return _window; }
			bool IsFullscreen() const;
			void ToggleFullscreen();

			// Camera:
			Scene::RCamera GetCamera();
			RSpirit GetCameraSpirit();

			// Configuration:
			void EnableDefaultCameraMovement(bool b = true);
			void EnableEscapeKeyExitGame(bool b = true);
			void EnableRawInputEvents(bool b = true);
			void ShowFPS(bool b);

			void BulletDebugDraw();

			bool IsRestartAppRequested() const { return _restartApp; }
			static void RestartApp();
			void RequestAppRestart();
		};
		VERUS_TYPEDEFS(BaseGame);
	}
}
