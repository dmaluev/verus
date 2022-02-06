// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_EVENT_HANDLER(fn) std::bind(fn, this, std::placeholders::_1)

namespace verus
{
	namespace GUI
	{
		struct InputFocus;
		class Widget;
		class Container;
		class View;

		struct EventArgs
		{
			Str     _view;
			Str     _id;
			Widget* _pWidget;
			int     _char;

			EventArgs(CSZ view, CSZ id, Widget* p, int c = 0) :
				_view(view), _id(id), _pWidget(p), _char(c) {}
		};
		VERUS_TYPEDEFS(EventArgs);

		typedef std::function<void(RcEventArgs)> TFnEvent;

		// Base class for all controls and even frames.
		// Control has position, size, id text, value text (for buttons, labels, etc.).
		// It can be enabled/disabled, visible/hidden.
		// Control belongs to certain view and it's position is relative to sizer or owner view if no sizer is specified.
		class Widget : public AllocatorAware
		{
			Animator        _animator;
			Vector4         _color = Vector4(1, 1, 1, 1);
			String          _id;
			WideString      _text;
			Vector<wchar_t> _vFixedText;
			String          _sizer;
			String          _ownerView;
			TFnEvent        _fnOnMouseEnter;
			TFnEvent        _fnOnMouseLeave;
			TFnEvent        _fnOnClick;
			TFnEvent        _fnOnDoubleClick;
			TFnEvent        _fnOnKey;
			TFnEvent        _fnOnChar;
			TFnEvent        _fnOnTimeout;
			int             _fixedTextLength = 0;
			float           _x = 0;
			float           _y = 0;
			float           _w = 1;
			float           _h = 1;
			float           _wScale = 1;
			float           _hScale = 1;
			float           _aspectOffset = 0;
			bool            _disabled = false;
			bool            _hidden = false;
			bool            _useAspect = false;

		public:
			Widget();
			virtual ~Widget();

			Str GetID() const { return _C(_id); }

			WideStr GetText() const;
			void    SetText(CWSZ text);
			void    SetText(CSZ text);

			void DrawDebug();
			void DrawInputStyle();

			virtual void Update();
			virtual void Draw() {}
			virtual void Parse(pugi::xml_node node);

			RAnimator GetAnimator() { return _animator; }

			// Cast:
			virtual InputFocus* AsInputFocus() { return nullptr; }
			virtual Container* AsContainer() { return nullptr; }

			RcVector4 GetColor(bool original = false) const { return original ? _color : _animator.GetColor(_color); }
			void      SetColor(RcVector4 color) { _color = _animator.SetColor(color); }

			// Relative:
			float GetX() const { return _animator.GetX(_x); }
			float GetY() const { return _animator.GetY(_y); }
			float GetW() const { return _animator.GetW(_w); }
			float GetH() const { return _animator.GetH(_h); }

			// Relative:
			void SetX(float x) { _x = _animator.SetX(x + _aspectOffset); }
			void SetY(float y) { _y = _animator.SetY(y); }
			void SetW(float w) { _w = _animator.SetW(w); }
			void SetH(float h) { _h = _animator.SetH(h); }

			void GetRelativePosition(float& x, float& y);
			void GetAbsolutePosition(float& x, float& y);
			void GetViewport(int xywh[4]);

			bool IsHovered(float x, float y);

			bool IsDisabled() const { return _disabled; }
			void Disable(bool disable = true) { _disabled = disable; }

			bool IsVisible() const { return !_hidden; }
			void Show() { _hidden = false; }
			void Hide(bool hide = true) { _hidden = hide; }

			float GetWScale() const { return _wScale; }
			float GetHScale() const { return _hScale; }
			void SetScale(float x, float y) { _wScale = x; _hScale = y; }

			View* GetOwnerView();
			void ResetOwnerView();
			void SetSizer(CSZ sizer) { _sizer = sizer; }

			// Events:
			void ConnectOnMouseEnter /**/(TFnEvent fn) { _fnOnMouseEnter = fn; }
			void ConnectOnMouseLeave /**/(TFnEvent fn) { _fnOnMouseLeave = fn; }
			void ConnectOnClick      /**/(TFnEvent fn) { _fnOnClick = fn; }
			void ConnectOnDoubleClick/**/(TFnEvent fn) { _fnOnDoubleClick = fn; }
			void ConnectOnKey        /**/(TFnEvent fn) { _fnOnKey = fn; }
			void ConnectOnChar       /**/(TFnEvent fn) { _fnOnChar = fn; }
			void ConnectOnTimeout    /**/(TFnEvent fn) { _fnOnTimeout = fn; }
			virtual bool InvokeOnMouseEnter();
			virtual bool InvokeOnMouseLeave();
			virtual bool InvokeOnClick(float x, float y);
			virtual bool InvokeOnDoubleClick(float x, float y);
			virtual bool InvokeOnKey(int scancode);
			virtual bool InvokeOnChar(wchar_t c);
			virtual bool InvokeOnTimeout();
		};
		VERUS_TYPEDEFS(Widget);
	}
}
