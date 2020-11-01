// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Net()
	{
		Net::Socket::Startup();
	}
	void Free_Net()
	{
		Net::Socket::Cleanup();
	}
}
