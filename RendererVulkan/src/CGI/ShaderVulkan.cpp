#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

ShaderVulkan::ShaderVulkan()
{
}

ShaderVulkan::~ShaderVulkan()
{
	Done();
}

void ShaderVulkan::Init(CSZ source, CSZ* list)
{
	VERUS_INIT();
}

void ShaderVulkan::Done()
{
	VERUS_DONE(ShaderVulkan);
}
