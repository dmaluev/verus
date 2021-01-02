// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Bloom.inc.hlsl"

ConstantBuffer<UB_BloomVS>        g_ubBloomVS        : register(b0, space0);
ConstantBuffer<UB_BloomFS>        g_ubBloomFS        : register(b0, space1);
ConstantBuffer<UB_BloomGodRaysFS> g_ubBloomGodRaysFS : register(b0, space2);

Texture2D              g_texColor  : register(t1, space1);
SamplerState           g_samColor  : register(s1, space1);
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

#ifdef DEF_GOD_RAYS
	const float maxDist = g_ubBloomGodRaysFS._maxDist_sunGloss_wideStrength_sunStrength.x;
	const float sunGloss = g_ubBloomGodRaysFS._maxDist_sunGloss_wideStrength_sunStrength.y;
	const float wideStrength = g_ubBloomGodRaysFS._maxDist_sunGloss_wideStrength_sunStrength.z;
	const float sunStrength = g_ubBloomGodRaysFS._maxDist_sunGloss_wideStrength_sunStrength.w;

	const float2 ndcPos = ToNdcPos(si.tc0);
	const float rawDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
	const float3 posW = DS_GetPosition(rawDepth, g_ubBloomGodRaysFS._matInvVP, ndcPos);

	const float3 eyePos = g_ubBloomGodRaysFS._eyePos.xyz;
	const float3 toPosW = posW - eyePos;
	const float depth = length(toPosW);
	const float3 pickingRayDir = toPosW / depth;
	const float2 spec = pow(saturate(dot(g_ubBloomGodRaysFS._dirToSun.xyz, pickingRayDir)), float2(7, sunGloss));
	const float strength = dot(spec, float2(wideStrength, sunStrength));

	float3 godRays = 0.f;
	if (strength >= 0.0001f)
	{
		const float3 rand = Rand(si.pos.xy);

#if _SHADER_QUALITY <= _Q_LOW
		const int sampleCount = 8;
#elif _SHADER_QUALITY <= _Q_MEDIUM
		const int sampleCount = 16;
#elif _SHADER_QUALITY <= _Q_HIGH
		const int sampleCount = 32;
#elif _SHADER_QUALITY <= _Q_ULTRA
		const int sampleCount = 64;
#endif
		const float stride = maxDist / sampleCount;

		float pickingRayLen = rand.y * stride;
		float acc = 0.f;
		[unroll] for (int i = 0; i < sampleCount; ++i)
		{
			const float3 pos = eyePos + pickingRayDir * pickingRayLen;
			const float shadowMask = SimpleShadowMapCSM(
				g_texShadow,
				g_samShadow,
				pos,
				pos,
				g_ubBloomGodRaysFS._matShadow,
				g_ubBloomGodRaysFS._matShadowCSM1,
				g_ubBloomGodRaysFS._matShadowCSM2,
				g_ubBloomGodRaysFS._matShadowCSM3,
				g_ubBloomGodRaysFS._matScreenCSM,
				g_ubBloomGodRaysFS._csmSplitRanges,
				g_ubBloomGodRaysFS._shadowConfig);
			acc += shadowMask * step(pickingRayLen, depth);
			pickingRayLen += stride;
		}
		godRays = acc * (1.f / sampleCount) * g_ubBloomGodRaysFS._sunColor.rgb * g_ubBloomFS._exposure.x * strength;
	}

	so.color.rgb = godRays;
#else
	const float colorScale = g_ubBloomFS._colorScale_colorBias.x;
	const float colorBias = g_ubBloomFS._colorScale_colorBias.y;

	const float4 rawColor = g_texColor.SampleLevel(g_samColor, si.tc0, 0.f);
	const float3 color = rawColor.rgb * g_ubBloomFS._exposure.x;
	const float3 bloom = saturate((color - colorBias) * colorScale);

	so.color.rgb = bloom;
#endif

	so.color.a = 1.f;

	return so;
}
#endif

//@main:#
//@main:#GodRays GOD_RAYS
