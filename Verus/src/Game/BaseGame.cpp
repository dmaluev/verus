// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

struct MyRendererDelegate : CGI::RendererDelegate
{
	PBaseGame _p = nullptr;

	MyRendererDelegate(PBaseGame p) : _p(p) {}
	~MyRendererDelegate() {}

	virtual void Renderer_OnDraw() override
	{
		_p->BaseGame_Draw();
	}

	virtual void Renderer_OnDrawOverlay() override
	{
		_p->BaseGame_DrawOverlay();
	}

	virtual void Renderer_OnPresent() override
	{
		VERUS_QREF_RENDERER;
		renderer->Present();
	}

	virtual void Renderer_OnDrawCubeMap() override
	{
	}
};
VERUS_TYPEDEFS(MyRendererDelegate);

struct BaseGame::Pimpl : AllocatorAware
{
	PBaseGame         _p = nullptr;
	Scene::MainCamera _camera;
	Spirit            _cameraSpirit;
	bool              _defaultCameraMovement = true;
	bool              _escapeKeyExitGame = true;
	bool              _minimized = false;
	bool              _rawInputEvents = false;
	bool              _showFPS = true;

	Pimpl(PBaseGame p) : _p(p)
	{
	}

	~Pimpl()
	{
	}
};

BaseGame::BaseGame()
{
	Utils::MakeEx(&_alloc); // For paths.
	Make_D(); // For log.
	_p = new Pimpl(this);
}

BaseGame::~BaseGame()
{
	delete _p;

	_engineInit.Free();

	Free_D();
	Utils::FreeEx(&_alloc);
	SDL_Quit();

	if (_restartApp)
		RestartApp();
}

void BaseGame::Initialize(VERUS_MAIN_DEFAULT_ARGS, App::Window::RcDesc windowDesc)
{
	const int ret = SDL_Init(SDL_INIT_EVERYTHING);
	if (ret)
		throw VERUS_RUNTIME_ERROR << "SDL_Init(); " << ret;

	Utils::I().InitPaths();

	App::Settings::Make();
	VERUS_QREF_SETTINGS;
	settings.ParseCommandLineArgs(argc, argv);
	settings.Load();
	settings.HandleHighDpi();
	settings.HandleCommandLineArgs();
	settings.Validate();
	settings.Save();
	App::Window::Desc updatedWindowDesc = windowDesc;
	BaseGame_UpdateSettings(updatedWindowDesc);
	if (App::Settings::Platform::uwp == settings._platform)
		settings._gapi = 12;

	// Allocate memory:
	_engineInit.Make();

#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	Utils::TestAll();
#endif

	_window.Init(updatedWindowDesc);
	CGI::Renderer::I().SetMainWindow(&_window);
	_engineInit.Init(new MyRendererDelegate(this));

	VERUS_QREF_IM;
	im.GainFocus(this);

	// Configure:
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_MM;
	_p->_camera.SetAspectRatio(renderer.GetSwapChainAspectRatio());
	_p->_camera.Update();
	if (Scene::SceneManager::IsValidSingleton())
		Scene::SceneManager::I().SetCamera(&_p->_camera);

	renderer->BeginFrame(false); // Begin recording a command buffer.
	renderer.InitCmd();
	_engineInit.InitCmd();
	if (Scene::MaterialManager::IsValidSingleton())
		Scene::MaterialManager::I().InitCmd();
	BaseGame_LoadContent();
	renderer->EndFrame(false); // End recording a command buffer.
	renderer->Sync(false);
}

void BaseGame::Loop(bool relativeMouseMode)
{
	VERUS_QREF_ASYNC;
	VERUS_QREF_IM;
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

	if (relativeMouseMode)
	{
		if (SDL_SetRelativeMouseMode(SDL_TRUE))
			throw VERUS_RUNTIME_ERROR << "SDL_SetRelativeMouseMode(SDL_TRUE)";
	}

	timer.Update();

	SDL_Event event;
	bool quit = false;

	do // The Game Loop.
	{
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			// Toggle fullscreen (Alt+Enter):
			if ((SDL_KEYDOWN == event.type) && (SDLK_RETURN == event.key.keysym.sym) && (event.key.keysym.mod & KMOD_ALT))
				ToggleFullscreen();

			bool keyboardShortcut = false;
			if (_p->_rawInputEvents && !ImGui::GetIO().WantCaptureMouse)
			{
				switch (event.type)
				{
				case SDL_KEYDOWN:
				{
					if (event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT))
						keyboardShortcut = BaseGame_SDL_OnKeyboardShortcut(event.key.keysym.sym, event.key.keysym.mod);
				}
				break;
				case SDL_MOUSEMOTION:
				{
					BaseGame_SDL_OnMouseMotion(event.motion.xrel, event.motion.yrel);
				}
				break;
				case SDL_MOUSEBUTTONDOWN:
				{
					if (2 == event.button.clicks)
						BaseGame_SDL_OnMouseDoubleClick(event.button.button);
					else
						BaseGame_SDL_OnMouseButtonDown(event.button.button);
				}
				break;
				case SDL_MOUSEBUTTONUP:
				{
					BaseGame_SDL_OnMouseButtonUp(event.button.button);
				}
				break;
				case SDL_MOUSEWHEEL:
				{
					BaseGame_SDL_OnMouseWheel(event.wheel.y);
				}
				break;
				}
			}

			if (!keyboardShortcut && !im.HandleEvent(event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
				{
					if (BaseGame_CanQuitEventLoop())
						quit = true;
				}
				break;
				case SDL_WINDOWEVENT:
				{
					switch (event.window.event)
					{
					case SDL_WINDOWEVENT_MOVED:
					{
						BaseGame_OnWindowMoved();
					}
					break;
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						if (renderer.OnWindowSizeChanged(event.window.data1, event.window.data2))
						{
							Scene::PCamera pCamera = Scene::SceneManager::IsValidSingleton() ? Scene::SceneManager::I().GetCamera() : nullptr;
							if (pCamera)
							{
								pCamera->SetAspectRatio(renderer.GetSwapChainAspectRatio());
								pCamera->Update();
							}
							BaseGame_OnWindowSizeChanged();
						}
					}
					break;
					case SDL_WINDOWEVENT_MINIMIZED:
					{
						if (!_p->_minimized)
						{
							_p->_minimized = true;
							BaseGame_OnDeactivated();
							renderer->WaitIdle();
						}
					}
					break;
					case SDL_WINDOWEVENT_SHOWN:
					case SDL_WINDOWEVENT_RESTORED:
					{
						if (_p->_minimized)
						{
							_p->_minimized = false;
							BaseGame_OnActivated();
						}
					}
					break;
					}
				}
				break;
				}
			}
		}

		if (_p->_escapeKeyExitGame && im.IsKeyDownEvent(SDL_SCANCODE_ESCAPE))
			Exit();

		if (_p->_minimized || _restartApp)
			continue;

		//
		// UPDATE
		//

		renderer->BeginFrame();

		async.Update();

		timer.Update();

		if (_p->_defaultCameraMovement)
		{
			const float speed = im.IsKeyPressed(SDL_SCANCODE_SPACE) ? 20.f : 2.f;
			if (im.IsKeyPressed(SDL_SCANCODE_W))
				_p->_cameraSpirit.MoveFront(speed);
			if (im.IsKeyPressed(SDL_SCANCODE_S))
				_p->_cameraSpirit.MoveFront(-speed);
			if (im.IsKeyPressed(SDL_SCANCODE_A))
				_p->_cameraSpirit.MoveSide(-speed);
			if (im.IsKeyPressed(SDL_SCANCODE_D))
				_p->_cameraSpirit.MoveSide(speed);
		}

		im.HandleInput();
		if (_restartApp)
			continue;

		if (Physics::Bullet::IsValidSingleton())
			Physics::Bullet::I().Simulate();

		if (_p->_defaultCameraMovement)
		{
			_p->_cameraSpirit.HandleInput();
			_p->_cameraSpirit.Update();
			_p->_camera.MoveEyeTo(_p->_cameraSpirit.GetPosition());
			_p->_camera.MoveAtTo(_p->_cameraSpirit.GetPosition() + _p->_cameraSpirit.GetFrontDirection());
			if (Scene::Water::IsValidSingleton())
			{
				VERUS_QREF_WATER;
				if (water.IsInitialized())
					_p->_camera.ExcludeWaterLine();
			}
			_p->_camera.Update();
			if (Scene::SceneManager::IsValidSingleton())
				Scene::SceneManager::I().SetCamera(&_p->_camera);
		}

		BaseGame_Update();
		if (_restartApp)
			continue;

		if (Audio::AudioSystem::IsValidSingleton())
			Audio::AudioSystem::I().Update();

		// Draw current frame:
		renderer.Draw();
		im.ResetInputState();
		renderer->EndFrame();
		renderer.Present();
		renderer->Sync();

		// Show FPS:
		if (_p->_showFPS && timer.IsEventEvery(500))
		{
			char title[40];
			CSZ gapi = "";
			switch (renderer->GetGapi())
			{
			case CGI::Gapi::vulkan:     gapi = "Vulkan"; break;
			case CGI::Gapi::direct3D12: gapi = "Direct3D 12"; break;
			}
			sprintf_s(title, "GAPI: %s, FPS: %.1f", gapi, renderer.GetFps());
			SDL_SetWindowTitle(renderer.GetMainWindow()->GetSDL(), title);
		}
	} while (!quit); // The Game Loop.

	BaseGame_UnloadContent();

	if (relativeMouseMode)
	{
		if (SDL_SetRelativeMouseMode(SDL_FALSE))
			throw VERUS_RUNTIME_ERROR << "SDL_SetRelativeMouseMode(SDL_FALSE)";
	}
}

void BaseGame::Exit()
{
	Utils::PushQuitEvent();
}

void BaseGame::OnMouseMove(float dx, float dy)
{
	if (!SDL_GetRelativeMouseMode())
		return;
	if (_p->_defaultCameraMovement)
	{
		_p->_cameraSpirit.TurnPitch(dy);
		_p->_cameraSpirit.TurnYaw(dx);
	}
}

bool BaseGame::IsFullscreen() const
{
	const Uint32 flags = SDL_GetWindowFlags(_window.GetSDL());
	return flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void BaseGame::ToggleFullscreen()
{
	const bool fullscreen = IsFullscreen();
	if (SDL_SetWindowFullscreen(_window.GetSDL(), fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP))
		throw VERUS_RUNTIME_ERROR << "SDL_SetWindowFullscreen()";
}

Scene::RCamera BaseGame::GetCamera()
{
	return _p->_camera;
}

RSpirit BaseGame::GetCameraSpirit()
{
	return _p->_cameraSpirit;
}

void BaseGame::EnableDefaultCameraMovement(bool b)
{
	_p->_defaultCameraMovement = b;
}

void BaseGame::EnableEscapeKeyExitGame(bool b)
{
	_p->_escapeKeyExitGame = b;
}

void BaseGame::EnableRawInputEvents(bool b)
{
	_p->_rawInputEvents = b;
}

void BaseGame::ShowFPS(bool b)
{
	_p->_showFPS = b;
}

void BaseGame::BulletDebugDraw()
{
	VERUS_QREF_BULLET;
	bullet.DebugDraw();
}

void BaseGame::RestartApp()
{
#ifdef _WIN32
	wchar_t pathname[MAX_PATH] = {};
	GetModuleFileName(nullptr, pathname, MAX_PATH);

	wchar_t commandLine[MAX_PATH] = {};
	wcscpy_s(commandLine, L"--restarted");

	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);

	CreateProcess(
		pathname,
		commandLine,
		nullptr,
		nullptr,
		FALSE,
		CREATE_NEW_PROCESS_GROUP,
		nullptr,
		nullptr,
		&si,
		&pi);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#endif
}

void BaseGame::RequestAppRestart()
{
	_restartApp = true;
	Exit();
}
