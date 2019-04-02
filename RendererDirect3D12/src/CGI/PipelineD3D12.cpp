#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

PipelineD3D12::PipelineD3D12()
{
}

PipelineD3D12::~PipelineD3D12()
{
	Done();
}

void PipelineD3D12::Init()
{
	VERUS_INIT();
}

void PipelineD3D12::Done()
{
	VERUS_DONE(PipelineD3D12);
}
