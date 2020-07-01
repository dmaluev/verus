// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "Bloom.inc.hlsl"

ConstantBuffer<UB_BloomVS> g_ubBloomVS : register(b0, space0);
ConstantBuffer<UB_BloomFS> g_ubBloomFS : register(b0, space1);

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
	so.pos = float4(mul(si.pos, g_ubBloomVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBloomVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 rawColor = g_tex.SampleLevel(g_sam, si.tc0, 0.0);
	const float3 color = rawColor.rgb * g_ubBloomFS._exposure.x;
	so.color.rgb = (color - 1.0) * 0.4;
	so.color.a = 1.0;

	return so;
}
#endif

//@main:#
