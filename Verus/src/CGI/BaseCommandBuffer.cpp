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

void CommandBufferPwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteCommandBuffer(_p);
		_p = nullptr;
	}
}