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
	for (auto p : _vInputFocusStack)
	{
		p->HandleInput();
		if (p->VetoInputFocus())
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

bool InputManager::IsKeyPressed(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStatePressed[id] || TranslateJoyPress(id, false);
}

bool InputManager::IsKeyDownEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateDownEvent[id];
}

bool InputManager::IsKeyUpEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateUpEvent[id];
}

bool InputManager::IsMousePressed(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStatePressed[id] || TranslateJoyPress(id, true);
}

bool InputManager::IsMouseDownEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDownEvent[id];
}

bool InputManager::IsMouseUpEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateUpEvent[id];
}

bool InputManager::IsMouseDoubleClick(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDoubleClick[id];
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

float InputManager::GetJoyAxisState(int id) const
{
	id = Math::Clamp(id, 0, JOY_AXIS_MAX - 1);
	return _joyStateAxis[id];
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

void InputManager::OnKeyDown(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[id] = true;
	_kbStateDownEvent[id] = true;
}

void InputManager::OnKeyUp(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[id] = false;
	_kbStateUpEvent[id] = true;
}

void InputManager::OnTextInput(wchar_t c)
{
	for (auto p : _vInputFocusStack)
	{
		p->OnTextInput(c);
		if (p->VetoInputFocus())
			break;
	}
}

void InputManager::OnMouseMove(int dx, int dy, int x, int y)
{
	const float scale = GetMouseScale();
	const float fdx = dx * scale;
	const float fdy = dy * scale;
	for (auto p : _vInputFocusStack)
	{
		p->OnMouseMove(dx, dy, x, y);
		p->OnMouseMove(fdx, fdy);
		if (p->VetoInputFocus())
			break;
	}
}

void InputManager::OnMouseDown(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[id] = true;
	_mouseStateDownEvent[id] = true;
}

void InputManager::OnMouseUp(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[id] = false;
	_mouseStateUpEvent[id] = true;
}

void InputManager::OnMouseDoubleClick(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStateDoubleClick[id] = true;
}

void InputManager::OnJoyAxis(int id, int value)
{
	id = Math::Clamp(id, 0, JOY_AXIS_MAX - 1);
	const float amount = value * (1.f / SHRT_MAX);
	const bool oneWay = (JOY_AXIS_TRIGGERLEFT == id || JOY_AXIS_TRIGGERRIGHT == id);
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
	_joyStateAxis[id] = fixedAmount;
}

void InputManager::OnJoyDown(int id)
{
	id = Math::Clamp(id, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[id] = true;
	_joyStateDownEvent[id] = true;
	TranslateJoy(id, false);
}

void InputManager::OnJoyUp(int id)
{
	id = Math::Clamp(id, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[id] = false;
	_joyStateUpEvent[id] = true;
	TranslateJoy(id, true);
}

void InputManager::TranslateJoy(int id, bool up)
{
	switch (id)
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

bool InputManager::TranslateJoyPress(int id, bool mouse) const
{
	if (mouse)
	{
		switch (id)
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
		switch (id)
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
