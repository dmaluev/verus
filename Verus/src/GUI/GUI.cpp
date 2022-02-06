// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_GUI()
	{
		GUI::ViewManager::Make();
	}
	void Free_GUI()
	{
		GUI::ViewManager::Free();
	}
}
