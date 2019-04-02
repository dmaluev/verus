#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

GeometryD3D12::GeometryD3D12()
{
}

GeometryD3D12::~GeometryD3D12()
{
	Done();
}

void GeometryD3D12::Init(RcGeometryDesc desc)
{
	VERUS_INIT();
}

void GeometryD3D12::Done()
{
	VERUS_DONE(GeometryD3D12);
}
