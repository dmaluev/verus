#include "verus.h"

using namespace verus;
using namespace verus::App;

// Window::Desc:

void Window::Desc::ApplySettings()
{
	VERUS_QREF_CONST_SETTINGS;
	_width = settings._screenSizeWidth;
	_height = settings._screenSizeHeight;
	_fullscreen = !settings._screenWindowed;
}

// Window:

Window::Window()
{
}

Window::~Window()
{
	Done();
}

void Window::Init(RcDesc descConst)
{
	VERUS_INIT();
	VERUS_QREF_SETTINGS;

	Desc desc = descConst;
	if (desc._useSettings)
		desc.ApplySettings();

	Uint32 flags = desc._flags;
	if (desc._fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	if (desc._resizable)
		flags |= SDL_WINDOW_RESIZABLE;
	if (0 == settings._gapi)
		flags |= SDL_WINDOW_VULKAN;

	_pWnd = SDL_CreateWindow(
		desc._title ? desc._title : "",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		desc._width,
		desc._height,
		flags);
	if (!_pWnd)
		throw VERUS_RECOVERABLE << "SDL_CreateWindow(), " << SDL_GetError();

	if (desc._fullscreen)
		SDL_GetWindowSize(_pWnd, &settings._screenSizeWidth, &settings._screenSizeHeight);
}

void Window::Done()
{
	if (_pWnd)
	{
		SDL_DestroyWindow(_pWnd);
		_pWnd = nullptr;
	}
	VERUS_DONE(Window);
}
