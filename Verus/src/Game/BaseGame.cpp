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
		VERUS_QREF_RENDERER;
		VERUS_QREF_TIMER;
		const float x = fmod(timer.GetTime(), 1.f);
		renderer.SetClearColor(Vector4(x, 0.5f, 0.25f, 1.f));
		renderer->Clear(0);
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
	bool              _showFPS = true;
	bool              _debugBullet = false;
	bool              _escapeKeyExitGame = true;

	Pimpl(PBaseGame p) : _p(p)
	{
	}

	~Pimpl()
	{
	}

	bool HandleUserEvents(SDL_Event* pEvent)
	{
		switch (pEvent->user.code)
		{
		case 0: return true; // Exit.
		}
		return false;
	}
};

BaseGame::BaseGame()
{
	Utils::MakeEx(&_alloc);
	_p = new Pimpl(this);
}

BaseGame::~BaseGame()
{
	delete _p;

	_engineInit.Free();

	Utils::FreeEx(&_alloc);
	SDL_Quit();
	//CGL::CRender::DoneWin32();
}

void BaseGame::Initialize(VERUS_MAIN_DEFAULT_ARGS)
{
	VERUS_SDL_CENTERED;

	App::Settings::Make();
	VERUS_QREF_SETTINGS;
	settings.ParseCommandLineArgs(argc, argv);
	//settings.LoadValidateSave();

	const int ret = SDL_Init(SDL_INIT_EVERYTHING);
	if (ret)
		throw VERUS_RUNTIME_ERROR << "SDL_Init(), " << ret;

	// Allocate memory:
	_engineInit.Make();

#ifdef _DEBUG
	//Utils::TestAll();
#endif

	_window.Init();
	CGI::Renderer::I().SetMainWindow(&_window);
	_engineInit.Init(this, new MyRendererDelegate(this));

	// Configure:
	VERUS_QREF_RENDERER;
	//VERUS_QREF_SM;
	_p->_camera.SetAspectRatio(renderer.GetWindowAspectRatio());
	//sm.SetCamera(&_p->_camera);

	renderer->BeginFrame(false); // Begin recording a command buffer.
	BaseGame_LoadContent();
	renderer->EndFrame(false); // End recording a command buffer.
}

void BaseGame::Run()
{
	VERUS_QREF_TIMER;
	VERUS_QREF_KM;
	//VERUS_QREF_BULLET;
	VERUS_QREF_RENDERER;
	VERUS_QREF_ASYNC;
	VERUS_QREF_ASYS;

	SDL_SetRelativeMouseMode(SDL_TRUE);

	timer.Update();

	SDL_Event event;
	bool done = false;

	do // The Game Loop.
	{
		if (_p->_escapeKeyExitGame && km.IsKeyDownEvent(SDL_SCANCODE_ESCAPE))
			Exit();

		while (SDL_PollEvent(&event))
		{
			if (!km.HandleSdlEvent(event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
				{
					done = true;
				}
				break;
				case SDL_WINDOWEVENT:
				{
					switch (event.window.event)
					{
					case SDL_WINDOWEVENT_MINIMIZED:
						BaseGame_OnDeactivated();
						break;
					case SDL_WINDOWEVENT_RESTORED:
						BaseGame_OnActivated();
						break;
					}
				}
				break;
				case SDL_USEREVENT:
				{
					if (_p->HandleUserEvents(&event))
					{
						SDL_Delay(100);
						done = true;
					}
				}
				break;
				}
			}
		}

		if (done)
			break;

		//
		// UPDATE
		//

		renderer->BeginFrame();

		async.Update();

		timer.Update();

		if (_p->_defaultCameraMovement)
		{
			const float speed = km.IsKeyPressed(SDL_SCANCODE_SPACE) ? 20.f : 2.f;
			if (km.IsKeyPressed(SDL_SCANCODE_W))
				_p->_cameraSpirit.MoveFront(speed);
			if (km.IsKeyPressed(SDL_SCANCODE_S))
				_p->_cameraSpirit.MoveFront(-speed);
			if (km.IsKeyPressed(SDL_SCANCODE_A))
				_p->_cameraSpirit.MoveSide(-speed);
			if (km.IsKeyPressed(SDL_SCANCODE_D))
				_p->_cameraSpirit.MoveSide(speed);
		}
		BaseGame_HandleInput();

		//bullet.Simulate();

		if (_p->_defaultCameraMovement)
		{
			_p->_cameraSpirit.HandleInput();
			_p->_cameraSpirit.Update();
			_p->_camera.MoveEyeTo(_p->_cameraSpirit.GetPosition());
			_p->_camera.MoveAtTo(_p->_cameraSpirit.GetPosition() + _p->_cameraSpirit.GetDirectionFront());
			//if (Scene::CWater::IsValidSingleton())
			{
				//	VERUS_QREF_WATER;
				//	if (water.IsInitialized())
				//		_p->_camera.ExcludeWaterLine();
			}
			_p->_camera.Update();
		}

		BaseGame_Update();

		asys.Update();

		// Draw current frame:
		renderer.Draw();
		km.ResetClickState();
		renderer->EndFrame();
		renderer.Present();

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
			sprintf_s(title, "[%s] - %.1f FPS", gapi, renderer.GetFps());
			SDL_SetWindowTitle(renderer.GetMainWindow()->GetSDL(), title);
		}
	} while (!done); // The Game Loop.

	BaseGame_UnloadContent();

	SDL_SetRelativeMouseMode(SDL_FALSE);
}

void BaseGame::Exit()
{
	Utils::ExitSdlLoop();
}

void BaseGame::KeyMapper_OnMouseMove(int x, int y)
{
	VERUS_QREF_CONST_SETTINGS;

	const float rad = (VERUS_2PI / 360.f) / 3.f; // 3 pixels = 1 degree.
	const float scale = rad * settings._inputMouseSensitivity;
	const float fx = x * scale;
	const float fy = y * scale;

	if (_p->_defaultCameraMovement)
	{
		_p->_cameraSpirit.TurnPitch(fy);
		_p->_cameraSpirit.TurnYaw(fx);
	}

	BaseGame_OnMouseMove(fx, fy);
}

void BaseGame::KeyMapper_OnKey(int scancode)
{
	BaseGame_OnKey(scancode);
}

void BaseGame::KeyMapper_OnChar(wchar_t c)
{
	BaseGame_OnChar(c);
}

Scene::RCamera BaseGame::GetCamera()
{
	return _p->_camera;
}

#if 0
RSpirit BaseGame::GetCameraSpirit()
{
	return _p->_cameraSpirit;
}
#endif

void BaseGame::ActivateDefaultCameraMovement(bool b)
{
	_p->_defaultCameraMovement = b;
}

void BaseGame::ShowFPS(bool b)
{
	_p->_showFPS = b;
}

void BaseGame::DebugBullet(bool b)
{
	_p->_debugBullet = b;
}

bool BaseGame::IsDebugBulletMode() const
{
	return _p->_debugBullet;
}