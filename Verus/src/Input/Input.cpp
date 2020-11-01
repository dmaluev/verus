// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_Input()
	{
		Input::KeyMapper::Make();
	}
	void Free_Input()
	{
		Input::KeyMapper::Free();
	}
}
