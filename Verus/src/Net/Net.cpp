// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Net()
	{
		Net::Socket::Startup();
		Net::Multiplayer::Make();
	}
	void Free_Net()
	{
		Net::Multiplayer::Free();
		Net::Socket::Cleanup();
	}
}
