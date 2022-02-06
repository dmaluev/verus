// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_INPUT_JOYAXIS_THRESHOLD 0.25f
#define VERUS_INPUT_MAX_KB            SDL_NUM_SCANCODES
#define VERUS_INPUT_MAX_MOUSE         8
#define VERUS_BUTTON_WHEELUP          6
#define VERUS_BUTTON_WHEELDOWN        7

namespace verus
{
	namespace Input
	{
		struct InputFocus
		{
			virtual bool InputFocus_Veto() { return false; }
			virtual void InputFocus_HandleInput() {}
			virtual void InputFocus_OnKeyDown(int scancode) {}
			virtual void InputFocus_OnKeyUp(int scancode) {}
			virtual void InputFocus_OnTextInput(wchar_t c) {}
			virtual void InputFocus_OnMouseMove(int dx, int dy, int x, int y) {}
			virtual void InputFocus_OnMouseMove(float dx, float dy) {}
		};
		VERUS_TYPEDEFS(InputFocus);

		enum
		{
			JOY_AXIS_LEFTX,
			JOY_AXIS_LEFTY,
			JOY_AXIS_RIGHTX,
			JOY_AXIS_RIGHTY,
			JOY_AXIS_TRIGGERLEFT,
			JOY_AXIS_TRIGGERRIGHT,
			JOY_AXIS_MAX
		};

		enum
		{
			JOY_BUTTON_DPAD_UP,
			JOY_BUTTON_DPAD_DOWN,
			JOY_BUTTON_DPAD_LEFT,
			JOY_BUTTON_DPAD_RIGHT,
			JOY_BUTTON_START,
			JOY_BUTTON_BACK,
			JOY_BUTTON_LEFTSTICK,
			JOY_BUTTON_RIGHTSTICK,
			JOY_BUTTON_LEFTSHOULDER,
			JOY_BUTTON_RIGHTSHOULDER,
			JOY_BUTTON_A,
			JOY_BUTTON_B,
			JOY_BUTTON_X,
			JOY_BUTTON_Y,
			JOY_BUTTON_GUIDE,
			JOY_BUTTON_MAX
		};

		class InputManager : public Singleton<InputManager>, public Object
		{
			struct Action
			{
				int    _key;
				int    _keyEx;
				int    _actionID;
				String _actionName;
			};

			typedef Map<int, int> TMapLookup;

			Vector<PInputFocus>   _vInputFocusStack;
			Vector<Action>        _mapAction;
			TMapLookup            _mapLookupByKey;
			TMapLookup            _mapLookupByAction;
			bool                  _kbStatePressed[VERUS_INPUT_MAX_KB];
			bool                  _kbStateDownEvent[VERUS_INPUT_MAX_KB];
			bool                  _kbStateUpEvent[VERUS_INPUT_MAX_KB];
			bool                  _mouseStatePressed[VERUS_INPUT_MAX_MOUSE];
			bool                  _mouseStateDownEvent[VERUS_INPUT_MAX_MOUSE];
			bool                  _mouseStateUpEvent[VERUS_INPUT_MAX_MOUSE];
			bool                  _mouseStateDoubleClick[VERUS_INPUT_MAX_MOUSE];
			Vector<SDL_Joystick*> _vJoysticks;
			float                 _joyStateAxis[JOY_AXIS_MAX];
			bool                  _joyStatePressed[JOY_BUTTON_MAX];
			bool                  _joyStateDownEvent[JOY_BUTTON_MAX];
			bool                  _joyStateUpEvent[JOY_BUTTON_MAX];

		public:
			InputManager();
			~InputManager();

			void Init();
			void Done();

			bool HandleEvent(SDL_Event& event);
			void HandleInput();

			int GainFocus(PInputFocus p);
			int LoseFocus(PInputFocus p);

			void Load(Action* pAction);

			bool IsKeyPressed(int scancode) const;
			bool IsKeyDownEvent(int scancode) const;
			bool IsKeyUpEvent(int scancode) const;

			bool IsMousePressed(int button) const;
			bool IsMouseDownEvent(int button) const;
			bool IsMouseUpEvent(int button) const;
			bool IsMouseDoubleClick(int button) const;

			bool IsActionPressed(int actionID) const;
			bool IsActionDownEvent(int actionID) const;
			bool IsActionUpEvent(int actionID) const;

			float GetJoyAxisState(int button) const;

			void BuildLookup();

			void ResetInputState();

			static float GetMouseScale();

			static CSZ GetSingletonFailMessage() { return "Make_Input(); // FAIL.\r\n"; }

		private:
			void SwitchRelativeMouseMode(int scancode);

			void OnKeyDown(int scancode);
			void OnKeyUp(int scancode);
			void OnTextInput(wchar_t c);

			void OnMouseMove(int dx, int dy, int x, int y);
			void OnMouseDown(int button);
			void OnMouseUp(int button);
			void OnMouseDoubleClick(int button);

			void OnJoyAxis(int button, int value);
			void OnJoyDown(int button);
			void OnJoyUp(int button);
			void TranslateJoy(int button, bool up);
			bool TranslateJoyPress(int button, bool mouse) const;
		};
		VERUS_TYPEDEFS(InputManager);
	}
}
