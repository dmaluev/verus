// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::App;

// Window::Desc:

void Window::Desc::ApplySettings()
{
	VERUS_QREF_CONST_SETTINGS;
	_width = settings._displaySizeWidth;
	_height = settings._displaySizeHeight;
	_displayMode = settings._displayMode;
}

// Window:

Window::Window()
{
}

Window::~Window()
{
	Done();
}

void Window::Init(RcDesc constDesc)
{
	VERUS_INIT();
	VERUS_QREF_SETTINGS;

	Desc desc = constDesc;
	if (desc._useSettings)
		desc.ApplySettings();

	Uint32 flags = desc._flags;
	if (DisplayMode::exclusiveFullscreen == desc._displayMode)
		flags |= SDL_WINDOW_FULLSCREEN;
	if (DisplayMode::borderlessWindowed == desc._displayMode)
		flags |= SDL_WINDOW_BORDERLESS;
	if (desc._resizable)
		flags |= SDL_WINDOW_RESIZABLE;
	if (desc._maximized)
		flags |= SDL_WINDOW_MAXIMIZED;
	if (settings._displayAllowHighDPI)
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
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

	if (DisplayMode::exclusiveFullscreen == desc._displayMode || DisplayMode::borderlessWindowed == desc._displayMode)
	{
		if (DisplayMode::exclusiveFullscreen == desc._displayMode)
			VERUS_LOG_INFO("Window is using Exclusive Fullscreen display mode");
		if (DisplayMode::borderlessWindowed == desc._displayMode)
			VERUS_LOG_INFO("Window is using Borderless Windowed display mode");
	}
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
