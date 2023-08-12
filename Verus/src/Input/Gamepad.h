// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Input
{
	class Gamepad
	{
		SDL_GameController* _pGameController = nullptr;
		SDL_Haptic* _pHaptic = nullptr;
		SDL_JoystickID _joystickID = -1;
		bool           _connected = false;

	public:
		Gamepad();
		~Gamepad();

		void Open(int joystickIndex);
		void Close();

		SDL_JoystickID GetJoystickID() const { return _joystickID; }
		bool IsConnected() const { return _connected; }
	};
	VERUS_TYPEDEFS(Gamepad);
}
