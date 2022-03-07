// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "Quad.inc.hlsl"

ConstantBuffer<UB_QuadVS> g_ubQuadVS : register(b0, space0);
ConstantBuffer<UB_QuadFS> g_ubQuadFS : register(b0, space1);

Texture2D    g_tex : register(t1, space1);
SamplerState g_sam : register(s1, space1);

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
	so.color = g_tex.SampleLevel(g_sam, si.tc0, 0.0);
#endif

	return so;
}
#endif

//@main:#
//@main:#HemicubeMask HEMICUBE_MASK
