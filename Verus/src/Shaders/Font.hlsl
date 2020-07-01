// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "Font.inc.hlsl"

ConstantBuffer<UB_FontFS> g_ubFontFS : register(b0, space0);

Texture2D    g_tex : register(t1, space0);
SamplerState g_sam : register(s1, space0);

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

	so.pos = si.pos;
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
