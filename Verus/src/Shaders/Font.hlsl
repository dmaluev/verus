// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "Font.inc.hlsl"

CBUFFER(0, UB_FontVS, g_ubFontVS)
CBUFFER(1, UB_FontFS, g_ubFontFS)

Texture2D    g_tex : REG(t1, space1, t0);
SamplerState g_sam : REG(s1, space1, s0);

struct VSI
{
	VK_LOCATION_POSITION float4 pos   : POSITION;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
	VK_LOCATION(8)       int4 tc0     : TEXCOORD0;
};

struct VSO
{
	float4 pos   : SV_Position;
	float4 color : COLOR0;
	float2 tc0   : TEXCOORD0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	so.pos = mul(si.pos, g_ubFontVS._matWVP);
	so.tc0 = si.tc0.xy / 32767.0;
	so.color = ColorToLinear(si.color);

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 color = g_tex.Sample(g_sam, si.tc0);
	so.color = color * si.color;

	return so;
}
#endif

//@main:#
