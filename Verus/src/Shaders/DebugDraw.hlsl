// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "DebugDraw.inc.hlsl"

VK_PUSH_CONSTANT
CBUFFER(0, UB_DebugDraw, g_ubDebugDraw)

struct VSI
{
	VK_LOCATION_POSITION float4 pos   : POSITION;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
};

struct VSO
{
	float4 pos   : SV_Position;
	float4 color : COLOR0;
	VK_POINT_SIZE
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	so.pos = mul(si.pos, g_ubDebugDraw._matWVP);
	so.color = ColorToLinear(si.color);
	VK_SET_POINT_SIZE;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = si.color;

	return so;
}
#endif

//@main:#
