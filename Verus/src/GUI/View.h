// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace GUI
	{
		struct InputFocus
		{
			virtual bool InputFocus_KeyOnly() { return false; }
			virtual void InputFocus_AddChar(wchar_t c) {}
			virtual void InputFocus_DeleteChar() {}
			virtual void InputFocus_BackspaceChar() {}
			virtual void InputFocus_SetCursor(int pos) {}
			virtual void InputFocus_MoveCursor(int delta) {}
			virtual void InputFocus_Enter() {}
			virtual void InputFocus_Tab() {}
			virtual void InputFocus_Key(int scancode) {}
			virtual void InputFocus_OnFocus() {}
		};
		VERUS_TYPEDEFS(InputFocus);

		struct ViewDelegate
		{
			virtual void View_SetViewData(View* pView) = 0;
			virtual void View_GetViewData(View* pView) = 0;
		};
		VERUS_TYPEDEFS(ViewDelegate);

		// Views contain other controls.
		// By default they have "done" state.
		// You should call ViewManager::FadeTo() after loading.
		class View : public Widget, public Container
		{
			friend class ViewManager;

			enum class State : int
			{
				fadeIn,
				fadeOut,
				active,
				done
			};

			typedef Map<int, String> TMapStrings;

			CGI::CSHandle     _csh;
			PViewDelegate     _pDelegate = nullptr;
			String            _url;
			String            _name;
			String            _locale = "RU";
			TMapStrings       _mapStrings;
			Vector<PWidget>   _vTabList;
			Scene::TexturePwn _tex;
			PWidget           _pLastHovered = nullptr;
			PInputFocus       _pInputFocus = nullptr;
			State             _state = State::done;
			Linear<float>     _fade;
			float             _fadeSpeed = 4;
			bool              _cursor = true;
			bool              _debug = false;
			bool              _skipNextKey = false;

		public:
			View();
			~View();

			virtual void Update() override;
			virtual void Draw() override;
			virtual void Parse(pugi::xml_node node) override;

			PViewDelegate SetDelegate(PViewDelegate p) { return Utils::Swap(_pDelegate, p); }

			void ResetAnimators(float reverseTime = 0);

			void SetData() { if (_pDelegate) _pDelegate->View_SetViewData(this); }
			void GetData() { if (_pDelegate) _pDelegate->View_GetViewData(this); }

			void OnClick();
			void OnDoubleClick();
			void OnKey(int scancode);
			void OnChar(wchar_t c);

			CSZ GetString(int id) const;

			void BeginFadeIn();
			void BeginFadeOut();

			Str GetUrl() const { return _C(_url); }
			Str GetName() const { return _C(_name); }
			Str GetLocale() const { return _C(_locale); }

			void HideCursor(bool hide = true) { _cursor = !hide; }
			bool HasCursor() const { return _cursor; }
			bool IsDebug() const { return _debug; }

			State GetState() const { return _state; }
			void SetState(State state) { _state = state; }

			PInputFocus GetInFocus() const { return _pInputFocus; }
			bool IsInFocus(PInputFocus p) const { return _pInputFocus == p; }
			void SetFocus(PInputFocus p);
			void SkipNextKey() { _skipNextKey = true; }

			void AddControlToTabList(PWidget p) { _vTabList.push_back(p); }
		};
		VERUS_TYPEDEFS(View);
	}
}
