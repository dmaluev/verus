// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Settings.h"
#include "Window.h"

namespace verus
{
	void Make_App();
	void Free_App();
}

namespace verus
{
	namespace App
	{
		void RunEventLoop();
	}
}
