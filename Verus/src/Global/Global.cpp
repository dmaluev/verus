// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

int g_singletonAllocCount;

namespace verus
{
	void Make_Global()
	{
		Timer::Make();
	}
	void Free_Global()
	{
		Timer::Free();
	}
}
