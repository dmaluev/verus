// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

namespace verus
{
	void Make_CGI()
	{
		CGI::Renderer::Make();
		CGI::DebugDraw::Make();
	}
	void Free_CGI()
	{
		CGI::DebugDraw::Free();
		CGI::Renderer::Free();
	}
}
