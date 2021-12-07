// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	VERUS_ZERO_MEM(_joyStateAxis);
	VERUS_ZERO_MEM(_joyStatePressed);
	VERUS_ZERO_MEM(_joyStateDownEvent);
	VERUS_ZERO_MEM(_joyStateUpEvent);
}

InputManager::~InputManager()
{
	Done();
}

void InputManager::Init()
{
	VERUS_INIT();

	_vInputFocusStack.reserve(16);

	const int count = SDL_NumJoysticks();
	_vJoysticks.reserve(count);
	VERUS_FOR(i, count)
	{
		SDL_Joystick* pJoystick = SDL_JoystickOpen(i);
		if (pJoystick)
			_vJoysticks.push_back(pJoystick);
	}
}

void InputManager::Done()
{
	VERUS_FOREACH_CONST(Vector<SDL_Joystick*>, _vJoysticks, it)
		SDL_JoystickClose(*it);
	_vJoysticks.clear();

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
	}
	break;
	case SDL_KEYUP:
	{
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;
		OnKeyUp(event.key.keysym.scancode);
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
	}
	break;
	case SDL_MOUSEBUTTONUP:
	{
		if (ImGui::GetIO().WantCaptureMouse)
			return false;
		if (event.button.button < VERUS_BUTTON_WHEELUP)
			OnMouseUp(event.button.button);
	}
	break;
	case SDL_MOUSEWHEEL:
	{
		if (ImGui::GetIO().WantCaptureMouse)
			return false;
		if (event.wheel.y >= 0)
		{
			OnMouseDown(VERUS_BUTTON_WHEELUP);
			OnMouseUp(VERUS_BUTTON_WHEELUP);
		}
		else
		{
			OnMouseDown(VERUS_BUTTON_WHEELDOWN);
			OnMouseUp(VERUS_BUTTON_WHEELDOWN);
		}
	}
	break;

	// JOYSTICK:
	case SDL_JOYAXISMOTION:
	{
		OnJoyAxis(event.jaxis.axis, event.jaxis.value);
	}
	break;
	case SDL_JOYBUTTONDOWN:
	{
		OnJoyDown(event.jbutton.button);
	}
	break;
	case SDL_JOYBUTTONUP:
	{
		OnJoyUp(event.jbutton.button);
	}
	break;

	default:
		return false;
	}
	return true;
}

void InputManager::HandleInput()
{
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

void InputManager::Load(Action* pAction)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "Keys.xml";
	IO::Xml xml(_C(ss.str()));
}

bool InputManager::IsKeyPressed(int scancode) const
{
	scancode = Math::Clamp(scancode, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStatePressed[scancode] || TranslateJoyPress(scancode, false);
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
	return _mouseStatePressed[button] || TranslateJoyPress(button, true);
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

bool InputManager::IsActionPressed(int actionID) const
{
	return false;
}

bool InputManager::IsActionDownEvent(int actionID) const
{
	return false;
}

bool InputManager::IsActionUpEvent(int actionID) const
{
	return false;
}

float InputManager::GetJoyAxisState(int button) const
{
	button = Math::Clamp(button, 0, JOY_AXIS_MAX - 1);
	return _joyStateAxis[button];
}

void InputManager::BuildLookup()
{
}

void InputManager::ResetInputState()
{
	VERUS_ZERO_MEM(_kbStateDownEvent);
	VERUS_ZERO_MEM(_kbStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDownEvent);
	VERUS_ZERO_MEM(_mouseStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDoubleClick);
	VERUS_ZERO_MEM(_joyStateDownEvent);
	VERUS_ZERO_MEM(_joyStateUpEvent);
}

float InputManager::GetMouseScale()
{
	VERUS_QREF_CONST_SETTINGS;
	const float rad = (VERUS_2PI / 360.f) / 3.f; // 3 pixels = 1 degree.
	return rad * settings._inputMouseSensitivity;
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

void InputManager::OnJoyAxis(int button, int value)
{
	button = Math::Clamp(button, 0, JOY_AXIS_MAX - 1);
	const float amount = value * (1.f / SHRT_MAX);
	const bool oneWay = (JOY_AXIS_TRIGGERLEFT == button || JOY_AXIS_TRIGGERRIGHT == button);
	float fixedAmount = amount;
	if (oneWay)
	{
		fixedAmount = fixedAmount * 0.5f + 0.5f;
		fixedAmount = Math::Clamp<float>((fixedAmount - VERUS_INPUT_JOYAXIS_THRESHOLD) / (1 - VERUS_INPUT_JOYAXIS_THRESHOLD), 0, 1);
	}
	else
	{
		fixedAmount = Math::Clamp<float>((abs(fixedAmount) - VERUS_INPUT_JOYAXIS_THRESHOLD) / (1 - VERUS_INPUT_JOYAXIS_THRESHOLD), 0, 1);
		fixedAmount *= glm::sign(amount);
	}
	_joyStateAxis[button] = fixedAmount;
}

void InputManager::OnJoyDown(int button)
{
	button = Math::Clamp(button, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[button] = true;
	_joyStateDownEvent[button] = true;
	TranslateJoy(button, false);
}

void InputManager::OnJoyUp(int button)
{
	button = Math::Clamp(button, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[button] = false;
	_joyStateUpEvent[button] = true;
	TranslateJoy(button, true);
}

void InputManager::TranslateJoy(int button, bool up)
{
	switch (button)
	{
	case JOY_BUTTON_DPAD_UP:
		break;
	case JOY_BUTTON_DPAD_DOWN:
		break;
	case JOY_BUTTON_DPAD_LEFT:
		break;
	case JOY_BUTTON_DPAD_RIGHT:
		break;
	case JOY_BUTTON_START:
		break;
	case JOY_BUTTON_BACK:
		break;
	case JOY_BUTTON_LEFTSTICK:
		break;
	case JOY_BUTTON_RIGHTSTICK:
		break;
	case JOY_BUTTON_LEFTSHOULDER:
		up ? OnMouseUp(VERUS_BUTTON_WHEELDOWN) : OnMouseDown(VERUS_BUTTON_WHEELDOWN);
		break;
	case JOY_BUTTON_RIGHTSHOULDER:
		up ? OnMouseUp(VERUS_BUTTON_WHEELUP) : OnMouseDown(VERUS_BUTTON_WHEELUP);
		break;
	case JOY_BUTTON_A:
		up ? OnKeyUp(SDL_SCANCODE_SPACE) : OnKeyDown(SDL_SCANCODE_SPACE);
		break;
	case JOY_BUTTON_B:
		up ? OnKeyUp(SDL_SCANCODE_LCTRL) : OnKeyDown(SDL_SCANCODE_LCTRL);
		break;
	case JOY_BUTTON_X:
		up ? OnMouseUp(SDL_BUTTON_LEFT) : OnMouseDown(SDL_BUTTON_LEFT);
		break;
	case JOY_BUTTON_Y:
		break;
	case JOY_BUTTON_GUIDE:
		break;
	}
}

bool InputManager::TranslateJoyPress(int button, bool mouse) const
{
	if (mouse)
	{
		switch (button)
		{
		case SDL_BUTTON_LEFT:
			return _joyStateAxis[JOY_AXIS_TRIGGERRIGHT] >= 0.25f;
		}
	}
	else
	{
		const float forward = 0.1f;
		const float walk = 0.25f;
		const float run = 0.6f;
		switch (button)
		{
		case SDL_SCANCODE_A: // Strafe left when attacking?
		{
			if (IsMousePressed(SDL_BUTTON_LEFT))
				return _joyStateAxis[JOY_AXIS_LEFTX] < -walk;
		}
		break;
		case SDL_SCANCODE_D: // Strafe right when attacking?
		{
			if (IsMousePressed(SDL_BUTTON_LEFT))
				return _joyStateAxis[JOY_AXIS_LEFTX] >= walk;
		}
		break;
		case SDL_SCANCODE_S:
		{
			if (IsMousePressed(SDL_BUTTON_LEFT))
			{
				return _joyStateAxis[JOY_AXIS_LEFTY] >= walk;
			}
			else
			{
				if (_joyStateAxis[JOY_AXIS_LEFTY] < forward)
					return false;
				const glm::vec2 v(_joyStateAxis[JOY_AXIS_LEFTX], _joyStateAxis[JOY_AXIS_LEFTY]);
				const float len = glm::length(v);
				return len >= walk;
			}
		}
		break;
		case SDL_SCANCODE_W:
		{
			if (IsMousePressed(SDL_BUTTON_LEFT))
			{
				return _joyStateAxis[JOY_AXIS_LEFTY] < -walk;
			}
			else
			{
				if (_joyStateAxis[JOY_AXIS_LEFTY] >= forward)
					return false;
				const glm::vec2 v(_joyStateAxis[JOY_AXIS_LEFTX], _joyStateAxis[JOY_AXIS_LEFTY]);
				const float len = glm::length(v);
				return len >= walk;
			}
		}
		break;
		case SDL_SCANCODE_SPACE:
		{
			return _joyStateAxis[JOY_AXIS_TRIGGERLEFT] >= 0.25f;
		}
		break;
		case SDL_SCANCODE_LSHIFT:
		{
			const glm::vec2 v(_joyStateAxis[JOY_AXIS_LEFTX], _joyStateAxis[JOY_AXIS_LEFTY]);
			const float len = glm::length(v);
			return len >= walk && len < run;
		}
		break;
		}
	}
	return false;
}
