// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Audio()
	{
		Audio::AudioSystem::Make();
	}
	void Free_Audio()
	{
		Audio::AudioSystem::Free();
	}
}
