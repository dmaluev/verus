#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

PipelineVulkan::PipelineVulkan()
{
}

PipelineVulkan::~PipelineVulkan()
{
	Done();
}

void PipelineVulkan::Init()
{
	VERUS_INIT();
}

void PipelineVulkan::Done()
{
	VERUS_DONE(PipelineVulkan);
}
