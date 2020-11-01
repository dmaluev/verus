// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Bloom.inc.hlsl"

ConstantBuffer<UB_BloomVS>        g_ubBloomVS        : register(b0, space0);
ConstantBuffer<UB_BloomFS>        g_ubBloomFS        : register(b0, space1);
ConstantBuffer<UB_BloomGodRaysFS> g_ubBloomGodRaysFS : register(b0, space2);

Texture2D              g_tex       : register(t1, space1);
SamplerState           g_sam       : register(s1, space1);
Texture2D              g_texDepth  : register(t1, space2);
SamplerState           g_samDepth  : register(s1, space2);
Texture2D              g_texShadow : register(t2, space2);
SamplerComparisonState g_samShadow : register(s2, space2);

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
	const float3 bloom = saturate((color - 1.0) * 0.3);

#ifdef DEF_GOD_RAYS
	const float2 ndcPos = ToNdcPos(si.tc0);
	const float rawDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
	const float3 posW = DS_GetPosition(rawDepth, g_ubBloomGodRaysFS._matInvVP, ndcPos);

	const float3 eyePos = g_ubBloomGodRaysFS._eyePos.xyz;
	const float3 toPosW = posW - eyePos;
	const float depth = length(toPosW);
	const float3 pickingRayDir = toPosW / depth;
	const float strength = pow(saturate(dot(g_ubBloomGodRaysFS._dirToSun.xyz, pickingRayDir)), 7.0) * 0.2;

	float3 godRays = 0.0;
	if (strength > 0.0)
	{
		const float3 rand = Rand(si.pos.xy);
		const int sampleCount = 20;
		float acc = 0.0;
		[unroll] for (int i = 0; i < sampleCount; ++i)
		{
			const float pickingRayLen = rand.y + i;
			const float3 pos = eyePos + pickingRayDir * pickingRayLen;

			const float4 tcShadow = ShadowCoords(float4(pos, 1), g_ubBloomGodRaysFS._matSunShadow, pickingRayLen);
			const float shadowMask = SimpleShadowMapCSM(
				g_texShadow,
				g_samShadow,
				tcShadow,
				g_ubBloomGodRaysFS._shadowConfig,
				g_ubBloomGodRaysFS._splitRanges,
				g_ubBloomGodRaysFS._matSunShadow,
				g_ubBloomGodRaysFS._matSunShadowCSM1,
				g_ubBloomGodRaysFS._matSunShadowCSM2,
				g_ubBloomGodRaysFS._matSunShadowCSM3);
			acc += shadowMask * step(pickingRayLen, depth);
		}
		godRays = acc * (1.0 / sampleCount) * g_ubBloomGodRaysFS._sunColor.rgb * g_ubBloomFS._exposure.x * strength;
	}

	so.color.rgb = bloom + godRays;
#else
	so.color.rgb = bloom;
#endif

	so.color.a = 1.0;

	return so;
}
#endif

//@main:#
//@main:#GodRays GOD_RAYS
