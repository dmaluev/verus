// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Input;

Gamepad::Gamepad()
{
}

Gamepad::~Gamepad()
{
	Close();
}

void Gamepad::Open(int joystickIndex)
{
	_pGameController = SDL_GameControllerOpen(joystickIndex);
	SDL_Joystick* pJoystick = SDL_GameControllerGetJoystick(_pGameController);
	_joystickID = SDL_JoystickInstanceID(pJoystick);
	_connected = true;
	if (SDL_JoystickIsHaptic(pJoystick))
	{
		_pHaptic = SDL_HapticOpenFromJoystick(pJoystick);
		if (SDL_HapticRumbleSupported(_pHaptic))
		{
			if (SDL_HapticRumbleInit(_pHaptic))
			{
				SDL_HapticClose(_pHaptic);
				_pHaptic = nullptr;
			}
		}
		else
		{
			SDL_HapticClose(_pHaptic);
			_pHaptic = nullptr;
		}
	}
}

void Gamepad::Close()
{
	if (_connected)
	{
		_connected = false;
		if (_pHaptic)
		{
			SDL_HapticClose(_pHaptic);
			_pHaptic = nullptr;
		}
		SDL_GameControllerClose(_pGameController);
		_pGameController = nullptr;
		_joystickID = -1;
	}
}
