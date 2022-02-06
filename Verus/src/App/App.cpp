// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_App()
	{
	}
	void Free_App()
	{
	}
}

using namespace verus;

void App::RunEventLoop()
{
	SDL_Event event = {};
	bool done = false;

	do
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
			{
				done = true;
			}
			break;
			case SDL_WINDOWEVENT:
			{
				switch (event.window.event)
				{
				case SDL_WINDOWEVENT_MINIMIZED:
					break;
				case SDL_WINDOWEVENT_RESTORED:
					break;
				}
			}
			break;
			case SDL_USEREVENT:
			{
			}
			break;
			}
		}

		if (done)
			break;
	} while (!done);
}
