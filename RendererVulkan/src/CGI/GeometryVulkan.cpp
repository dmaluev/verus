#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

GeometryVulkan::GeometryVulkan()
{
}

GeometryVulkan::~GeometryVulkan()
{
	Done();
}

void GeometryVulkan::Init(RcGeometryDesc desc)
{
	VERUS_INIT();
}

void GeometryVulkan::Done()
{
	VERUS_DONE(GeometryVulkan);
}
