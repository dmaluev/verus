// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_Reflection.inc.hlsl"

ConstantBuffer<UB_ReflectionVS> g_ubReflectionVS : register(b0, space0);
ConstantBuffer<UB_ReflectionFS> g_ubReflectionFS : register(b0, space1);

Texture2D    g_texGBuffer0 : register(t1, space1);
SamplerState g_samGBuffer0 : register(s1, space1);
Texture2D    g_texGBuffer2 : register(t2, space1);
SamplerState g_samGBuffer2 : register(s2, space1);
Texture2D    g_texReflect  : register(t3, space1);
SamplerState g_samReflect  : register(s3, space1);

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
	so.pos = float4(mul(si.pos, g_ubReflectionVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubReflectionVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 rawGBuffer0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.f);
	const float4 rawGBuffer2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.f);
	const float4 rawReflect = g_texReflect.SampleLevel(g_samReflect, si.tc0, 0.f);

	const float specMask = rawGBuffer0.a;
	const float3 metalColor = ToMetalColor(rawGBuffer0.rgb);
	const float2 metalMask = DS_GetMetallicity(rawGBuffer2);
	so.color.rgb = rawReflect.rgb * lerp(1.f, metalColor, metalMask.x) * specMask;
	so.color.a = 1.f;

	return so;
}
#endif

//@main:#
