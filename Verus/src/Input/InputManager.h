// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_INPUT_MAX_KB     SDL_NUM_SCANCODES
#define VERUS_INPUT_MAX_MOUSE  8
#define VERUS_BUTTON_WHEELUP   6
#define VERUS_BUTTON_WHEELDOWN 7

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

		enum class ActionType : int
		{
			inBoolean,
			inFloat,
			inVector2,
			inPose,
			outVibration
		};

		class InputManager : public Singleton<InputManager>, public Object
		{
		public:
			struct ActionSet
			{
				String _name; // Must be lowercase.
				String _localizedName;
				UINT32 _priority = 0;
				bool   _xrCompatible = false;
			};
			VERUS_TYPEDEFS(ActionSet);

			struct Action
			{
				Vector<String> _vSubactionPaths;
				Vector<String> _vBindingPaths;
				String         _name; // Must be lowercase.
				String         _localizedName;
				ActionType     _type = ActionType::inBoolean;
				int            _setIndex = 0;

				bool           _changedState = false;
				bool           _booleanValue = false;
				float          _floatValue = 0;
			};
			VERUS_TYPEDEFS(Action);

			struct ActionIndex
			{
				UINT32 _atom = 0;
				int    _actionIndex = 0;

				bool   _changedState = false;
				bool   _booleanValue = false;
				bool   _negativeBoolean = false;
				float  _floatValue = 0;
			};
			VERUS_TYPEDEFS(ActionIndex);

		private:
			enum class PathEntity : int
			{
				undefined,
				gamepad,
				handLeft,
				handRight,
				head,
				keyboard,
				mouse,
				treadmill
			};

			enum class PathIdentifier : int
			{
				undefined,
				aim,
				buttonA,
				buttonB,
				buttonEnd,
				buttonHome,
				buttonSelect,
				buttonStart,
				buttonX,
				buttonY,
				diamondDown,
				diamondLeft,
				diamondRight,
				diamondUp,
				dpadDown,
				dpadLeft,
				dpadRight,
				dpadUp,
				grip,
				haptic,
				joystick,
				miscBack,
				miscMenu,
				miscMuteMic,
				miscPlayPause,
				miscView,
				miscVolumeDown,
				miscVolumeUp,
				pedal,
				shoulder,
				squeeze,
				system,
				throttle,
				thumbrest,
				thumbstick,
				trackball,
				trackpad,
				trigger,
				wheel
			};

			enum class PathLocation : int
			{
				undefined,
				left,
				leftLower,
				leftUpper,
				lower,
				right,
				rightLower,
				rightUpper,
				upper
			};

			enum class PathComponent : int
			{
				undefined,
				click,
				force,
				pose,
				scalarX,
				scalarY,
				touch,
				twist,
				value
			};

			static const int s_maxGamepads = 4;

			Vector<PInputFocus> _vInputFocusStack;
			Vector<ActionSet>   _vActionSets;
			Vector<Action>      _vActions;
			Vector<ActionIndex> _vActionIndex;
			Gamepad             _gamepads[s_maxGamepads];
			int                 _axisThreshold = 8192;
			bool                _kbStatePressed[VERUS_INPUT_MAX_KB];
			bool                _kbStateDownEvent[VERUS_INPUT_MAX_KB];
			bool                _kbStateUpEvent[VERUS_INPUT_MAX_KB];
			bool                _mouseStatePressed[VERUS_INPUT_MAX_MOUSE];
			bool                _mouseStateDownEvent[VERUS_INPUT_MAX_MOUSE];
			bool                _mouseStateUpEvent[VERUS_INPUT_MAX_MOUSE];
			bool                _mouseStateDoubleClick[VERUS_INPUT_MAX_MOUSE];

		public:
			InputManager();
			~InputManager();

			void Init();
			void Done();

			bool HandleEvent(SDL_Event& event);
			void HandleInput();

			int GainFocus(PInputFocus p);
			int LoseFocus(PInputFocus p);

			void ResetInputState();

			bool IsKeyPressed(int scancode) const;
			bool IsKeyDownEvent(int scancode) const;
			bool IsKeyUpEvent(int scancode) const;

			bool IsMousePressed(int button) const;
			bool IsMouseDownEvent(int button) const;
			bool IsMouseUpEvent(int button) const;
			bool IsMouseDoubleClick(int button) const;

			static float GetMouseScale();

			int GetGamepadIndex(SDL_JoystickID joystickID) const;

			int CreateActionSet(CSZ name, CSZ localizedName, bool xrCompatible = false, int priority = 0);
			int CreateAction(int setIndex, CSZ name, CSZ localizedName, ActionType type,
				std::initializer_list<CSZ> ilSubactionPaths, std::initializer_list<CSZ> ilBindingPaths);

			int GetActionSetCount() const { return static_cast<int>(_vActionSets.size()); }
			int GetActionCount() const { return static_cast<int>(_vActions.size()); }

			RcActionSet GetActionSet(int index) { return _vActionSets[index]; }
			RcAction GetAction(int index) { return _vActions[index]; }

			static UINT32 ToAtom(CSZ path);
			static UINT32 ToAtom(int player, PathEntity entity, int inOut,
				PathIdentifier identifier, PathLocation location, PathComponent component);
			static UINT32 ToAtom(int player, PathEntity entity, int inOut, int scancode);
			static bool IsKeyboardAtom(UINT32 atom);
			static bool IsMouseAtom(UINT32 atom);

			bool GetActionStateBoolean(int actionIndex, bool* pChangedState = nullptr, int subaction = -1) const;
			float GetActionStateFloat(int actionIndex, bool* pChangedState = nullptr, int subaction = -1) const;
			bool GetActionStatePose(int actionIndex, Math::RPose pose, int subaction = -1) const;

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

			void UpdateActionStateBoolean(UINT32 atom, bool value);
			void UpdateActionStateFloat(UINT32 atom, float value);
		};
		VERUS_TYPEDEFS(InputManager);
	}
}
