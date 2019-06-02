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
		struct KeyMapperDelegate
		{
			virtual void KeyMapper_OnMouseMove(int x, int y) = 0;
			virtual void KeyMapper_OnKey(int scancode) = 0;
			virtual void KeyMapper_OnChar(wchar_t c) = 0;
		};
		VERUS_TYPEDEFS(KeyMapperDelegate);

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

		class KeyMapper : public Singleton<KeyMapper>, public Object
		{
			struct Action
			{
				int    _key;
				int    _keyEx;
				int    _actionID;
				String _actionName;
			};

			typedef Map<int, int> TMapLookup;

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
			PKeyMapperDelegate    _pKeyMapperDelegate = nullptr;
			Vector<SDL_Joystick*> _vJoysticks;
			float                 _joyStateAxis[JOY_AXIS_MAX];
			bool                  _joyStatePressed[JOY_BUTTON_MAX];
			bool                  _joyStateDownEvent[JOY_BUTTON_MAX];
			bool                  _joyStateUpEvent[JOY_BUTTON_MAX];

		public:
			KeyMapper();
			~KeyMapper();

			void Init();
			void Done();

			bool HandleSdlEvent(SDL_Event& event);

			void Load(Action* pAction);

			bool IsKeyPressed(int id) const;
			bool IsKeyDownEvent(int id) const;
			bool IsKeyUpEvent(int id) const;

			bool IsMousePressed(int id) const;
			bool IsMouseDownEvent(int id) const;
			bool IsMouseUpEvent(int id) const;
			bool IsMouseDoubleClick(int id) const;

			bool IsActionPressed(int actionID) const;
			bool IsActionDownEvent(int actionID) const;
			bool IsActionUpEvent(int actionID) const;

			float GetJoyAxisState(int id) const;

			void BuildLookup();

			void ResetClickState();

			PKeyMapperDelegate SetDelegate(PKeyMapperDelegate p) { return Utils::Swap(_pKeyMapperDelegate, p); }

			static CSZ GetSingletonFailMessage() { return "Make_Input(); // FAIL.\r\n"; }

		private:
			void OnKeyDown(int id);
			void OnKeyUp(int id);
			void OnChar(wchar_t c);

			void OnMouseMove(int x, int y);
			void OnMouseDown(int id);
			void OnMouseUp(int id);
			void OnMouseDoubleClick(int id);

			void OnJoyAxis(int id, int value);
			void OnJoyDown(int id);
			void OnJoyUp(int id);
			void TranslateJoy(int id, bool up);
			bool TranslateJoyPress(int id, bool mouse) const;
		};
		VERUS_TYPEDEFS(KeyMapper);
	}
}
