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
				CSZ    _title = nullptr;
				int    _width = 0;
				int    _height = 0;
				Uint32 _flags = 0;
				bool   _fullscreen = false;
				bool   _resizable = false;
				bool   _maximized = false;
				bool   _useSettings = true;

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
