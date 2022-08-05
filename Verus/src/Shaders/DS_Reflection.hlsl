// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "DS_Reflection.inc.hlsl"

CBUFFER(0, UB_ReflectionVS, g_ubReflectionVS)
CBUFFER(1, UB_ReflectionFS, g_ubReflectionFS)

Texture2D    g_texGBuffer1 : REG(t1, space1, t0);
SamplerState g_samGBuffer1 : REG(s1, space1, s0);
Texture2D    g_texReflect  : REG(t2, space1, t1);
SamplerState g_samReflect  : REG(s2, space1, s1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos : SV_Position;
	float4 tc0 : TEXCOORD0;
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
	so.tc0.xy = mul(si.pos, g_ubReflectionVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubReflectionVS._tcViewScaleBias.xy + g_ubReflectionVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0);
	const float4 reflectSam = g_texReflect.SampleLevel(g_samReflect, si.tc0.zw, 0.0);

	// <BackgroundColor>
	const float2 normalWasSet = ceil(gBuffer1Sam.rg);
	const float bgMask = saturate(normalWasSet.r + normalWasSet.g);
	// </BackgroundColor>

#ifdef DEF_DEBUG_CUBE_MAP
	so.color.rgb = reflectSam.rgb;
#else
	so.color.rgb = reflectSam.rgb * bgMask;
#endif
	so.color.a = 1.0;

	return so;
}
#endif

//@main:#
//@main:#DebugCubeMap DEBUG_CUBE_MAP
