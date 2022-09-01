// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Input;

InputManager::InputManager()
{
	VERUS_ZERO_MEM(_kbStatePressed);
	VERUS_ZERO_MEM(_kbStateDownEvent);
	VERUS_ZERO_MEM(_kbStateUpEvent);
	VERUS_ZERO_MEM(_mouseStatePressed);
	VERUS_ZERO_MEM(_mouseStateDownEvent);
	VERUS_ZERO_MEM(_mouseStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDoubleClick);
}

InputManager::~InputManager()
{
	Done();
}

void InputManager::Init()
{
	VERUS_INIT();

	_vInputFocusStack.reserve(16);
	_vActionSets.reserve(8);
	_vActions.reserve(32);
	_vActionIndex.reserve(64);
}

void InputManager::Done()
{
	for (auto& x : _gamepads)
		x.Close();

	VERUS_DONE(InputManager);
}

bool InputManager::HandleEvent(SDL_Event& event)
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_QREF_CONST_SETTINGS;

	switch (event.type)
	{
		// KEYBOARD:
	case SDL_KEYDOWN:
	{
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;
		SwitchRelativeMouseMode(event.key.keysym.scancode);
		OnKeyDown(event.key.keysym.scancode);
		const UINT32 atom = ToAtom(1, PathEntity::keyboard, 1, event.key.keysym.scancode);
		UpdateActionStateBoolean(atom, true);
	}
	break;
	case SDL_KEYUP:
	{
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;
		OnKeyUp(event.key.keysym.scancode);
		const UINT32 atom = ToAtom(1, PathEntity::keyboard, 1, event.key.keysym.scancode);
		UpdateActionStateBoolean(atom, false);
	}
	break;
	case SDL_TEXTINPUT:
	{
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;
		wchar_t wide[4];
		Str::Utf8ToWide(event.text.text, wide, 4);
		OnTextInput(wide[0]);
	}
	break;

	// MOUSE:
	case SDL_MOUSEMOTION:
	{
		OnMouseMove(
			event.motion.xrel,
			event.motion.yrel,
			event.motion.x,
			event.motion.y);
	}
	break;
	case SDL_MOUSEBUTTONDOWN:
	{
		if (ImGui::GetIO().WantCaptureMouse)
			return false;
		if (2 == event.button.clicks)
		{
			if (event.button.button < VERUS_BUTTON_WHEELUP)
				OnMouseDoubleClick(event.button.button);
		}
		else
		{
			if (event.button.button < VERUS_BUTTON_WHEELUP)
				OnMouseDown(event.button.button);
		}
		const UINT32 atom = ToAtom(1, PathEntity::mouse, 1, event.button.button);
		UpdateActionStateBoolean(atom, true);
	}
	break;
	case SDL_MOUSEBUTTONUP:
	{
		if (ImGui::GetIO().WantCaptureMouse)
			return false;
		if (event.button.button < VERUS_BUTTON_WHEELUP)
			OnMouseUp(event.button.button);
		const UINT32 atom = ToAtom(1, PathEntity::mouse, 1, event.button.button);
		UpdateActionStateBoolean(atom, false);
	}
	break;
	case SDL_MOUSEWHEEL:
	{
		if (ImGui::GetIO().WantCaptureMouse)
			return false;
		const int button = (event.wheel.y >= 0) ? VERUS_BUTTON_WHEELUP : VERUS_BUTTON_WHEELDOWN;
		OnMouseDown(button);
		OnMouseUp(button);
		const UINT32 atom = ToAtom(1, PathEntity::mouse, 1, button);
		UpdateActionStateBoolean(atom, true);
		UpdateActionStateBoolean(atom, false);
	}
	break;

	// CONTROLLER:
	case SDL_CONTROLLERAXISMOTION:
	{
		const int player = 1 + GetGamepadIndex(event.caxis.which);
		PathIdentifier identifier = PathIdentifier::undefined;
		PathLocation location = PathLocation::undefined;
		PathComponent component = PathComponent::undefined;
		int sign = 1; // To match OpenXR.
		switch (event.caxis.axis)
		{
		case SDL_CONTROLLER_AXIS_LEFTX:        identifier = PathIdentifier::thumbstick; location = PathLocation::left; component = PathComponent::scalarX; break;
		case SDL_CONTROLLER_AXIS_LEFTY:        identifier = PathIdentifier::thumbstick; location = PathLocation::left; component = PathComponent::scalarY; sign = -1; break;
		case SDL_CONTROLLER_AXIS_RIGHTX:       identifier = PathIdentifier::thumbstick; location = PathLocation::right; component = PathComponent::scalarX; break;
		case SDL_CONTROLLER_AXIS_RIGHTY:       identifier = PathIdentifier::thumbstick; location = PathLocation::right; component = PathComponent::scalarY; sign = -1; break;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  identifier = PathIdentifier::trigger; location = PathLocation::left; component = PathComponent::value; break;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: identifier = PathIdentifier::trigger; location = PathLocation::right; component = PathComponent::value; break;
		}
		const UINT32 atom = ToAtom(player, PathEntity::gamepad, 1, identifier, location, component);
		const int value = (abs(event.caxis.value) >= _axisThreshold) ? event.caxis.value * sign : 0;
		if (value > 0)
			UpdateActionStateFloat(atom, Math::Min(1.f, (value - _axisThreshold) / static_cast<float>(SHRT_MAX - _axisThreshold)));
		else if (value < 0)
			UpdateActionStateFloat(atom, Math::Max(-1.f, (value + _axisThreshold) / static_cast<float>(SHRT_MAX - _axisThreshold)));
		else
			UpdateActionStateFloat(atom, 0);
	}
	break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		const int player = 1 + GetGamepadIndex(event.cbutton.which);
		PathIdentifier identifier = PathIdentifier::undefined;
		PathLocation location = PathLocation::undefined;
		switch (event.cbutton.button)
		{
		case SDL_CONTROLLER_BUTTON_A:             identifier = PathIdentifier::buttonA; break;
		case SDL_CONTROLLER_BUTTON_B:             identifier = PathIdentifier::buttonB; break;
		case SDL_CONTROLLER_BUTTON_X:             identifier = PathIdentifier::buttonX; break;
		case SDL_CONTROLLER_BUTTON_Y:             identifier = PathIdentifier::buttonY; break;
		case SDL_CONTROLLER_BUTTON_BACK:          identifier = PathIdentifier::miscView; break;
		case SDL_CONTROLLER_BUTTON_START:         identifier = PathIdentifier::miscMenu; break;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK:     identifier = PathIdentifier::thumbstick; location = PathLocation::left; break;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    identifier = PathIdentifier::thumbstick; location = PathLocation::right; break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  identifier = PathIdentifier::shoulder; location = PathLocation::left; break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: identifier = PathIdentifier::shoulder; location = PathLocation::right; break;
		case SDL_CONTROLLER_BUTTON_DPAD_UP:       identifier = PathIdentifier::dpadUp; break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     identifier = PathIdentifier::dpadDown; break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     identifier = PathIdentifier::dpadLeft; break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    identifier = PathIdentifier::dpadRight; break;
		}
		const UINT32 atom = ToAtom(player, PathEntity::gamepad, 1, identifier, location, PathComponent::click);
		const bool value = (SDL_PRESSED == event.cbutton.state);
		UpdateActionStateBoolean(atom, value);
	}
	break;
	case SDL_CONTROLLERDEVICEADDED:
	{
		if (event.cdevice.which < s_maxGamepads && SDL_IsGameController(event.cdevice.which))
			_gamepads[event.cdevice.which].Open(event.cdevice.which);
	}
	break;
	case SDL_CONTROLLERDEVICEREMOVED:
	{
		const int gamepadIndex = GetGamepadIndex(event.cdevice.which);
		if (gamepadIndex >= 0)
			_gamepads[gamepadIndex].Close();
	}
	break;

	default:
		return false;
	}
	return true;
}

void InputManager::HandleInput()
{
	const int actionCount = static_cast<int>(_vActions.size());
	VERUS_FOR(i, actionCount)
	{
		RAction action = _vActions[i];
		switch (action._type)
		{
		case ActionType::inBoolean:
		{
			const bool prevValue = action._booleanValue;
			action._booleanValue = false;
			bool forceChangedState = false;
			for (const auto& x : _vActionIndex)
			{
				if (x._actionIndex == i)
				{
					forceChangedState = forceChangedState || x._changedState;
					action._booleanValue = action._booleanValue || x._booleanValue;
				}
			}
			action._changedState = (prevValue != action._booleanValue) || forceChangedState;
		}
		break;
		case ActionType::inFloat:
		{
			const float prevValue = action._floatValue;
			action._floatValue = 0;
			for (const auto& x : _vActionIndex)
			{
				if (x._actionIndex == i)
				{
					if (abs(x._floatValue) > abs(action._floatValue))
						action._floatValue = x._floatValue;
				}
			}
			action._changedState = prevValue != action._floatValue;
		}
		break;
		}
	}

	for (auto& x : _vActionIndex)
		x._changedState = false;

	VERUS_FOREACH_REVERSE(Vector<PInputFocus>, _vInputFocusStack, it)
	{
		(*it)->InputFocus_HandleInput();
		if ((*it)->InputFocus_Veto())
			break;
	}
}

int InputManager::GainFocus(PInputFocus p)
{
	auto it = std::find(_vInputFocusStack.begin(), _vInputFocusStack.end(), p);
	if (it != _vInputFocusStack.end())
		std::rotate(it, it + 1, _vInputFocusStack.end());
	else
		_vInputFocusStack.push_back(p);
	return static_cast<int>(_vInputFocusStack.size()) - 1;
}

int InputManager::LoseFocus(PInputFocus p)
{
	auto it = std::find(_vInputFocusStack.begin(), _vInputFocusStack.end(), p);
	if (it != _vInputFocusStack.end())
	{
		const int index = Utils::Cast32(it - _vInputFocusStack.begin());
		_vInputFocusStack.erase(std::remove(_vInputFocusStack.begin(), _vInputFocusStack.end(), p), _vInputFocusStack.end());
		return index;
	}
	return -1;
}

void InputManager::ResetInputState()
{
	VERUS_ZERO_MEM(_kbStateDownEvent);
	VERUS_ZERO_MEM(_kbStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDownEvent);
	VERUS_ZERO_MEM(_mouseStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDoubleClick);
}

bool InputManager::IsKeyPressed(int scancode) const
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStatePressed[scancode];
}

bool InputManager::IsKeyDownEvent(int scancode) const
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateDownEvent[scancode];
}

bool InputManager::IsKeyUpEvent(int scancode) const
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateUpEvent[scancode];
}

bool InputManager::IsMousePressed(int button) const
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStatePressed[button];
}

bool InputManager::IsMouseDownEvent(int button) const
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDownEvent[button];
}

bool InputManager::IsMouseUpEvent(int button) const
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateUpEvent[button];
}

bool InputManager::IsMouseDoubleClick(int button) const
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDoubleClick[button];
}

float InputManager::GetMouseScale()
{
	VERUS_QREF_CONST_SETTINGS;
	const float rad = (VERUS_2PI / 360.f) / 3.f; // 3 pixels = 1 degree.
	return rad * settings._inputMouseSensitivity;
}

int InputManager::GetGamepadIndex(SDL_JoystickID joystickID) const
{
	VERUS_FOR(i, s_maxGamepads)
	{
		if (_gamepads[i].IsConnected() && _gamepads[i].GetJoystickID() == joystickID)
			return i;
	}
	return -1;
}

int InputManager::CreateActionSet(CSZ name, CSZ localizedName, bool xrCompatible, int priority)
{
	const int index = Utils::Cast32(_vActionSets.size());
	_vActionSets.resize(index + 1);
	_vActionSets[index]._name = name;
	_vActionSets[index]._localizedName = localizedName;
	_vActionSets[index]._priority = priority;
	_vActionSets[index]._xrCompatible = xrCompatible;
	return index;
}

int InputManager::CreateAction(int setIndex, CSZ name, CSZ localizedName, ActionType type,
	std::initializer_list<CSZ> ilSubactionPaths, std::initializer_list<CSZ> ilBindingPaths)
{
	if (setIndex < 0 || setIndex >= _vActionSets.size())
	{
		VERUS_RT_FAIL("Invalid set index.");
		return -1;
	}
	const int index = Utils::Cast32(_vActions.size());
	_vActions.resize(index + 1);
	RAction action = _vActions[index];
	action._vSubactionPaths.reserve(ilSubactionPaths.size());
	for (CSZ path : ilSubactionPaths)
		action._vSubactionPaths.push_back(path);
	action._vBindingPaths.reserve(ilBindingPaths.size());
	for (CSZ path : ilBindingPaths)
		action._vBindingPaths.push_back(path);
	action._name = name;
	action._localizedName = localizedName;
	action._type = type;
	action._setIndex = setIndex;

	for (CSZ bindingPath : ilBindingPaths)
	{
		ActionIndex actionIndex;
		actionIndex._atom = ToAtom(bindingPath);
		if (actionIndex._atom)
		{
			actionIndex._actionIndex = index;
			if (IsKeyboardAtom(actionIndex._atom) || IsMouseAtom(actionIndex._atom))
				actionIndex._negativeBoolean = Str::EndsWith(bindingPath, "/-");
			_vActionIndex.push_back(actionIndex);
		}
	}

	return index;
}

UINT32 InputManager::ToAtom(CSZ path)
{
	if (!path || !(*path) || path[0] != '/')
		return 0;

	// Bits 31-28: Player.
	// Bits 27-20: Entity.
	// Bits 19-16: In/Out.
	// Bits 15- 8: Identifier.
	// Bits  7- 4: Location.
	// Bits  3- 0: Component.

	auto Compare = [](CSZ s, size_t len, CSZ begin, PathLocation* pLoc = nullptr)
	{
		const size_t beginLen = strlen(begin);
		if (beginLen > len)
			return false;
		const bool fuzzy = (len > beginLen) && ((s[beginLen] >= '0' && s[beginLen] <= '9') || s[beginLen] == '_');
		const size_t compareLen = fuzzy ? beginLen : len;
		if (!strncmp(s, begin, compareLen))
		{
			if (pLoc)
			{
				const CSZ locations[] =
				{
					"",
					"_left",
					"_left_lower",
					"_left_upper",
					"_lower",
					"_right",
					"_right_lower",
					"_right_upper",
					"_upper"
				};
				for (int i = 1; i < VERUS_COUNT_OF(locations); ++i)
				{
					const size_t locLen = strlen(locations[i]);
					if (len >= locLen)
					{
						if (!strncmp(s + len - locLen, locations[i], locLen))
						{
							*pLoc = static_cast<PathLocation>(i);
							break;
						}
					}
				}
			}
			return true;
		}
		return false;
	};

	UINT32 ret = 0;

	if (*path) // User:
	{
		path++;

		CSZ pEnd = strchr(path, '/');
		const size_t len = pEnd ? pEnd - path : strlen(path);
		if (Compare(path, len, "player"))
			ret |= atoi(path + 6) << 28;
		else if (Compare(path, len, "user"))
			ret |= 1 << 28;

		path += len;
	}

	if (*path) // Entity:
	{
		path++;

		CSZ pEnd = strchr(path, '/');
		const size_t len = pEnd ? pEnd - path : strlen(path);
		PathEntity entity = PathEntity::undefined;
		if (Compare(path, len, "gamepad"))
			entity = PathEntity::gamepad;
		else if (Compare(path, len, "hand") && Str::StartsWith(path, "hand/left"))
			entity = PathEntity::handLeft;
		else if (Compare(path, len, "hand") && Str::StartsWith(path, "hand/right"))
			entity = PathEntity::handRight;
		else if (Compare(path, len, "head"))
			entity = PathEntity::head;
		else if (Compare(path, len, "keyboard"))
			entity = PathEntity::keyboard;
		else if (Compare(path, len, "mouse"))
			entity = PathEntity::mouse;
		else if (Compare(path, len, "treadmill"))
			entity = PathEntity::treadmill;
		ret |= +entity << 20;

		path += len;
		if (PathEntity::handLeft == entity || PathEntity::handRight == entity)
			path = strchr(path + 1, '/');
	}

	if (*path)
	{
		path++;

		CSZ pEnd = strchr(path, '/');
		const size_t len = pEnd ? pEnd - path : strlen(path);
		if (Compare(path, len, "input"))
			ret |= 1 << 16;
		else if (Compare(path, len, "output"))
			ret |= 2 << 16;

		path += len;
	}

	if (IsKeyboardAtom(ret) || IsMouseAtom(ret))
	{
		if (*path)
		{
			path++;

			ret |= atoi(path);
		}
		return ret;
	}

	if (*path) // Identifier:
	{
		path++;

		CSZ pEnd = strchr(path, '/');
		const size_t len = pEnd ? pEnd - path : strlen(path);
		PathIdentifier identifier = PathIdentifier::undefined;
		PathLocation location = PathLocation::undefined;
		if (Compare(path, len, "aim", &location))
			identifier = PathIdentifier::aim;
		else if (Compare(path, len, "a", &location))
			identifier = PathIdentifier::buttonA;
		else if (Compare(path, len, "b", &location))
			identifier = PathIdentifier::buttonB;
		else if (Compare(path, len, "end", &location))
			identifier = PathIdentifier::buttonEnd;
		else if (Compare(path, len, "home", &location))
			identifier = PathIdentifier::buttonHome;
		else if (Compare(path, len, "select", &location))
			identifier = PathIdentifier::buttonSelect;
		else if (Compare(path, len, "start", &location))
			identifier = PathIdentifier::buttonStart;
		else if (Compare(path, len, "x", &location))
			identifier = PathIdentifier::buttonX;
		else if (Compare(path, len, "y", &location))
			identifier = PathIdentifier::buttonY;
		else if (Compare(path, len, "diamond_down"))
			identifier = PathIdentifier::diamondDown;
		else if (Compare(path, len, "diamond_left"))
			identifier = PathIdentifier::diamondLeft;
		else if (Compare(path, len, "diamond_right"))
			identifier = PathIdentifier::diamondRight;
		else if (Compare(path, len, "diamond_up"))
			identifier = PathIdentifier::diamondUp;
		else if (Compare(path, len, "dpad_down"))
			identifier = PathIdentifier::dpadDown;
		else if (Compare(path, len, "dpad_left"))
			identifier = PathIdentifier::dpadLeft;
		else if (Compare(path, len, "dpad_right"))
			identifier = PathIdentifier::dpadRight;
		else if (Compare(path, len, "dpad_up"))
			identifier = PathIdentifier::dpadUp;
		else if (Compare(path, len, "grip", &location))
			identifier = PathIdentifier::grip;
		else if (Compare(path, len, "haptic", &location))
			identifier = PathIdentifier::haptic;
		else if (Compare(path, len, "joystick", &location))
			identifier = PathIdentifier::joystick;
		else if (Compare(path, len, "back", &location))
			identifier = PathIdentifier::miscBack;
		else if (Compare(path, len, "menu", &location))
			identifier = PathIdentifier::miscMenu;
		else if (Compare(path, len, "mute_mic", &location))
			identifier = PathIdentifier::miscMuteMic;
		else if (Compare(path, len, "play_pause", &location))
			identifier = PathIdentifier::miscPlayPause;
		else if (Compare(path, len, "view", &location))
			identifier = PathIdentifier::miscView;
		else if (Compare(path, len, "volume_down"))
			identifier = PathIdentifier::miscVolumeDown;
		else if (Compare(path, len, "volume_up"))
			identifier = PathIdentifier::miscVolumeUp;
		else if (Compare(path, len, "pedal", &location))
			identifier = PathIdentifier::pedal;
		else if (Compare(path, len, "shoulder", &location))
			identifier = PathIdentifier::shoulder;
		else if (Compare(path, len, "squeeze", &location))
			identifier = PathIdentifier::squeeze;
		else if (Compare(path, len, "system", &location))
			identifier = PathIdentifier::system;
		else if (Compare(path, len, "throttle", &location))
			identifier = PathIdentifier::throttle;
		else if (Compare(path, len, "thumbrest", &location))
			identifier = PathIdentifier::thumbrest;
		else if (Compare(path, len, "thumbstick", &location))
			identifier = PathIdentifier::thumbstick;
		else if (Compare(path, len, "trackball", &location))
			identifier = PathIdentifier::trackball;
		else if (Compare(path, len, "trackpad", &location))
			identifier = PathIdentifier::trackpad;
		else if (Compare(path, len, "trigger", &location))
			identifier = PathIdentifier::trigger;
		else if (Compare(path, len, "wheel", &location))
			identifier = PathIdentifier::wheel;
		ret |= +identifier << 8;
		ret |= +location << 4;

		path += len;
	}

	if (*path) // Component:
	{
		path++;

		CSZ pEnd = strchr(path, '/');
		const size_t len = pEnd ? pEnd - path : strlen(path);
		PathComponent component = PathComponent::undefined;
		if (Compare(path, len, "click"))
			component = PathComponent::click;
		else if (Compare(path, len, "force"))
			component = PathComponent::force;
		else if (Compare(path, len, "pose"))
			component = PathComponent::pose;
		else if (Compare(path, len, "x"))
			component = PathComponent::scalarX;
		else if (Compare(path, len, "y"))
			component = PathComponent::scalarY;
		else if (Compare(path, len, "touch"))
			component = PathComponent::touch;
		else if (Compare(path, len, "twist"))
			component = PathComponent::twist;
		else if (Compare(path, len, "value"))
			component = PathComponent::value;
		ret |= +component;

		path += len;
	}

	return ret;
}

UINT32 InputManager::ToAtom(int player, PathEntity entity, int inOut,
	PathIdentifier identifier, PathLocation location, PathComponent component)
{
	UINT32 ret = 0;

	ret |= player << 28;
	ret |= +entity << 20;
	ret |= inOut << 16;
	ret |= +identifier << 8;
	ret |= +location << 4;
	ret |= +component;

	return ret;
}

UINT32 InputManager::ToAtom(int player, PathEntity entity, int inOut, int scancode)
{
	UINT32 ret = 0;

	ret |= player << 28;
	ret |= +entity << 20;
	ret |= inOut << 16;
	ret |= scancode & 0xFFFF;

	return ret;
}

bool InputManager::IsKeyboardAtom(UINT32 atom)
{
	return +PathEntity::keyboard == ((atom >> 20) & 0xFF);
}

bool InputManager::IsMouseAtom(UINT32 atom)
{
	return +PathEntity::mouse == ((atom >> 20) & 0xFF);
}

bool InputManager::GetActionStateBoolean(int actionIndex, bool* pChangedState, int subaction) const
{
	RcAction action = _vActions[actionIndex];
	VERUS_RT_ASSERT(ActionType::inBoolean == action._type);
	if (pChangedState)
		*pChangedState = false;

	bool currentState = false;
	auto pExtReality = CGI::Renderer::I()->GetExtReality();
	if (pExtReality->IsInitialized() && pExtReality->GetActionStateBoolean(actionIndex, currentState, pChangedState, subaction))
		return currentState;

	if (pChangedState)
		*pChangedState = action._changedState;
	return action._booleanValue;
}

float InputManager::GetActionStateFloat(int actionIndex, bool* pChangedState, int subaction) const
{
	RcAction action = _vActions[actionIndex];
	VERUS_RT_ASSERT(ActionType::inFloat == action._type);
	if (pChangedState)
		*pChangedState = false;

	float currentState = 0;
	auto pExtReality = CGI::Renderer::I()->GetExtReality();
	if (pExtReality->IsInitialized() && pExtReality->GetActionStateFloat(actionIndex, currentState, pChangedState, subaction))
		return currentState;

	if (pChangedState)
		*pChangedState = action._changedState;
	return action._floatValue;
}

bool InputManager::GetActionStatePose(int actionIndex, Math::RPose pose, int subaction) const
{
	RcAction action = _vActions[actionIndex];
	VERUS_RT_ASSERT(ActionType::inPose == action._type);

	bool active = false;
	auto pExtReality = CGI::Renderer::I()->GetExtReality();
	if (pExtReality->IsInitialized() && pExtReality->GetActionStatePose(actionIndex, active, pose, subaction))
		return active;

	return false;
}

void InputManager::SwitchRelativeMouseMode(int scancode)
{
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	if (SDL_SCANCODE_KP_ENTER == scancode)
	{
		const SDL_bool rel = SDL_GetRelativeMouseMode();
		SDL_SetRelativeMouseMode(rel ? SDL_FALSE : SDL_TRUE);
	}
#endif
}

void InputManager::OnKeyDown(int scancode)
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[scancode] = true;
	_kbStateDownEvent[scancode] = true;
	VERUS_FOREACH_REVERSE(Vector<PInputFocus>, _vInputFocusStack, it)
	{
		(*it)->InputFocus_OnKeyDown(scancode);
		if ((*it)->InputFocus_Veto())
			break;
	}
}

void InputManager::OnKeyUp(int scancode)
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[scancode] = false;
	_kbStateUpEvent[scancode] = true;
	VERUS_FOREACH_REVERSE(Vector<PInputFocus>, _vInputFocusStack, it)
	{
		(*it)->InputFocus_OnKeyUp(scancode);
		if ((*it)->InputFocus_Veto())
			break;
	}
}

void InputManager::OnTextInput(wchar_t c)
{
	VERUS_FOREACH_REVERSE(Vector<PInputFocus>, _vInputFocusStack, it)
	{
		(*it)->InputFocus_OnTextInput(c);
		if ((*it)->InputFocus_Veto())
			break;
	}
}

void InputManager::OnMouseMove(int dx, int dy, int x, int y)
{
	const float scale = GetMouseScale();
	const float fdx = dx * scale;
	const float fdy = dy * scale;
	VERUS_FOREACH_REVERSE(Vector<PInputFocus>, _vInputFocusStack, it)
	{
		(*it)->InputFocus_OnMouseMove(dx, dy, x, y);
		(*it)->InputFocus_OnMouseMove(fdx, fdy);
		if ((*it)->InputFocus_Veto())
			break;
	}
}

void InputManager::OnMouseDown(int button)
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[button] = true;
	_mouseStateDownEvent[button] = true;
}

void InputManager::OnMouseUp(int button)
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[button] = false;
	_mouseStateUpEvent[button] = true;
}

void InputManager::OnMouseDoubleClick(int button)
{
	button = Math::Clamp(button, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStateDoubleClick[button] = true;
}

void InputManager::UpdateActionStateBoolean(UINT32 atom, bool value)
{
	for (auto& x : _vActionIndex)
	{
		if (atom == x._atom)
		{
			x._changedState = x._changedState || (x._booleanValue != value);
			x._booleanValue = value;
			if (x._negativeBoolean)
				x._floatValue = value ? -1.f : 0.f;
			else
				x._floatValue = value ? 1.f : 0.f;
		}
	}
}

void InputManager::UpdateActionStateFloat(UINT32 atom, float value)
{
	for (auto& x : _vActionIndex)
	{
		if (atom == x._atom)
		{
			x._booleanValue = value >= 0.5f;
			x._floatValue = value;
		}
	}
}
