// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

// CommandBufferPtr:

void CommandBufferPtr::Init()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertCommandBuffer();
	_p->Init();
}

void CommandBufferPtr::InitOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertCommandBuffer();
	_p->InitOneTimeSubmit();
}

void CommandBufferPwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteCommandBuffer(_p);
		_p = nullptr;
	}
}
