// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "Quad.inc.hlsl"

CBUFFER(0, UB_QuadVS, g_ubQuadVS)
CBUFFER(1, UB_QuadFS, g_ubQuadFS)

Texture2D    g_tex : REG(t1, space1, t0);
SamplerState g_sam : REG(s1, space1, s0);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos : SV_Position;
	float2 tc0 : TEXCOORD0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubQuadVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubQuadVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#ifdef DEF_HEMICUBE_MASK
	const float weight = ComputeHemicubeMask(si.tc0);
	so.color = weight;
#else
	const float4 colorSam = g_tex.SampleLevel(g_sam, si.tc0, 0.0);
	so.color.r = dot(colorSam, g_ubQuadFS._rMultiplexer);
	so.color.g = dot(colorSam, g_ubQuadFS._gMultiplexer);
	so.color.b = dot(colorSam, g_ubQuadFS._bMultiplexer);
	so.color.a = dot(colorSam, g_ubQuadFS._aMultiplexer);
#endif

	return so;
}
#endif

//@main:#
//@main:#HemicubeMask HEMICUBE_MASK
