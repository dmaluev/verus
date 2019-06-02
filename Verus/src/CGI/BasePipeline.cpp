#include "verus.h"

using namespace verus;
using namespace verus::CGI;

// PipelinePtr:

void PipelinePtr::Init(RcPipelineDesc desc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertPipeline();
	_p->Init(desc);
}

void PipelinePwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeletePipeline(_p);
		_p = nullptr;
	}
}
