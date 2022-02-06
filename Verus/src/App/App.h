// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Settings.h"
#include "Window.h"
#include "UndoManager.h"

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
