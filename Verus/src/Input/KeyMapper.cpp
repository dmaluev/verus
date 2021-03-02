// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Input;

KeyMapper::KeyMapper()
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

KeyMapper::~KeyMapper()
{
	Done();
}

void KeyMapper::Init()
{
	VERUS_INIT();

	const int count = SDL_NumJoysticks();
	_vJoysticks.reserve(count);
	VERUS_FOR(i, count)
	{
		SDL_Joystick* pJoystick = SDL_JoystickOpen(i);
		if (pJoystick)
			_vJoysticks.push_back(pJoystick);
	}
}

void KeyMapper::Done()
{
	VERUS_FOREACH_CONST(Vector<SDL_Joystick*>, _vJoysticks, it)
		SDL_JoystickClose(*it);
	_vJoysticks.clear();

	VERUS_DONE(KeyMapper);
}

bool KeyMapper::HandleSdlEvent(SDL_Event& event)
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_QREF_CONST_SETTINGS;

	static bool firstTime = true;

	switch (event.type)
	{
		// Keyboard:
	case SDL_KEYDOWN:
	{
		if (ImGui::GetIO().WantCaptureKeyboard)
			return false;
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
		if (SDL_SCANCODE_KP_ENTER == event.key.keysym.scancode)
		{
			const SDL_bool rel = SDL_GetRelativeMouseMode();
			SDL_SetRelativeMouseMode(rel ? SDL_FALSE : SDL_TRUE);
		}
#endif
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
		OnChar(wide[0]);
	}
	break;

	// Mouse:
	case SDL_MOUSEMOTION:
	{
		if (!firstTime)
			OnMouseMove(event.motion.xrel, event.motion.yrel);
		else
			firstTime = false;
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

	// Joystick:
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

void KeyMapper::Load(Action* pAction)
{
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "Keys.xml";
	IO::Xml xml(_C(ss.str()));
}

bool KeyMapper::IsKeyPressed(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStatePressed[id] || TranslateJoyPress(id, false);
}

bool KeyMapper::IsKeyDownEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateDownEvent[id];
}

bool KeyMapper::IsKeyUpEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	return _kbStateUpEvent[id];
}

bool KeyMapper::IsMousePressed(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStatePressed[id] || TranslateJoyPress(id, true);
}

bool KeyMapper::IsMouseDownEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDownEvent[id];
}

bool KeyMapper::IsMouseUpEvent(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateUpEvent[id];
}

bool KeyMapper::IsMouseDoubleClick(int id) const
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	return _mouseStateDoubleClick[id];
}

bool KeyMapper::IsActionPressed(int actionID) const
{
	return false;
}

bool KeyMapper::IsActionDownEvent(int actionID) const
{
	return false;
}

bool KeyMapper::IsActionUpEvent(int actionID) const
{
	return false;
}

float KeyMapper::GetJoyAxisState(int id) const
{
	id = Math::Clamp(id, 0, JOY_AXIS_MAX - 1);
	return _joyStateAxis[id];
}

void KeyMapper::BuildLookup()
{
}

void KeyMapper::ResetClickState()
{
	VERUS_ZERO_MEM(_kbStateDownEvent);
	VERUS_ZERO_MEM(_kbStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDownEvent);
	VERUS_ZERO_MEM(_mouseStateUpEvent);
	VERUS_ZERO_MEM(_mouseStateDoubleClick);
	VERUS_ZERO_MEM(_joyStateDownEvent);
	VERUS_ZERO_MEM(_joyStateUpEvent);
}

void KeyMapper::OnKeyDown(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[id] = true;
	_kbStateDownEvent[id] = true;
	if (_pKeyMapperDelegate)
		_pKeyMapperDelegate->KeyMapper_OnKey(id);
}

void KeyMapper::OnKeyUp(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_KB - 1);
	_kbStatePressed[id] = false;
	_kbStateUpEvent[id] = true;
}

void KeyMapper::OnChar(wchar_t c)
{
	if (c && _pKeyMapperDelegate)
		_pKeyMapperDelegate->KeyMapper_OnChar(c);
}

void KeyMapper::OnMouseMove(int x, int y)
{
	if (_pKeyMapperDelegate)
		_pKeyMapperDelegate->KeyMapper_OnMouseMove(x, y);
}

void KeyMapper::OnMouseDown(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[id] = true;
	_mouseStateDownEvent[id] = true;
}

void KeyMapper::OnMouseUp(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStatePressed[id] = false;
	_mouseStateUpEvent[id] = true;
}

void KeyMapper::OnMouseDoubleClick(int id)
{
	id = Math::Clamp(id, 0, VERUS_INPUT_MAX_MOUSE - 1);
	_mouseStateDoubleClick[id] = true;
}

void KeyMapper::OnJoyAxis(int id, int value)
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

void KeyMapper::OnJoyDown(int id)
{
	id = Math::Clamp(id, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[id] = true;
	_joyStateDownEvent[id] = true;
	TranslateJoy(id, false);
}

void KeyMapper::OnJoyUp(int id)
{
	id = Math::Clamp(id, 0, JOY_BUTTON_MAX - 1);
	_joyStatePressed[id] = false;
	_joyStateUpEvent[id] = true;
	TranslateJoy(id, true);
}

void KeyMapper::TranslateJoy(int id, bool up)
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

bool KeyMapper::TranslateJoyPress(int id, bool mouse) const
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
