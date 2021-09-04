// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace App
	{
		class Window : public Object
		{
			SDL_Window* _pWnd = nullptr;

		public:
			struct Desc
			{
				CSZ         _title = nullptr;
				int         _x = SDL_WINDOWPOS_CENTERED;
				int         _y = SDL_WINDOWPOS_CENTERED;
				int         _width = 0;
				int         _height = 0;
				Uint32      _flags = 0;
				DisplayMode _displayMode = DisplayMode::windowed;
				UINT32      _color = VERUS_COLOR_BLACK;
				bool        _resizable = true;
				bool        _maximized = false;
				bool        _useSettings = true;

				void ApplySettings();
			};
			VERUS_TYPEDEFS(Desc);

			Window();
			~Window();

			void Init(RcDesc desc = Desc());
			void Done();

			SDL_Window* GetSDL() const { return _pWnd; }
		};
		VERUS_TYPEDEFS(Window);
	}
}
