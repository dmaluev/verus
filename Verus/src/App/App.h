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
