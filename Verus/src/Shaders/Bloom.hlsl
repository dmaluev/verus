// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Bloom.inc.hlsl"

CBUFFER(0, UB_BloomVS, g_ubBloomVS)
CBUFFER(1, UB_BloomFS, g_ubBloomFS)
CBUFFER(2, UB_BloomLightShaftsFS, g_ubBloomLightShaftsFS)

Texture2D              g_texColor  : REG(t1, space1, t0);
SamplerState           g_samColor  : REG(s1, space1, s0);
Texture2D              g_texDepth  : REG(t1, space2, t1);
SamplerState           g_samDepth  : REG(s1, space2, s1);
Texture2D              g_texShadow : REG(t2, space2, t2);
SamplerComparisonState g_samShadow : REG(s2, space2, s2);

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
	so.pos = float4(mul(si.pos, g_ubBloomVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBloomVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBloomVS._tcViewScaleBias.xy + g_ubBloomVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#ifdef DEF_LIGHT_SHAFTS
	const float2 ndcPos = ToNdcPos(si.tc0.xy);

	const float maxDist = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.x;
	const float sunGloss = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.y;
	const float wideStrength = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.z;
	const float sunStrength = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.w;

	const float depthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0).r;
	const float3 posW = DS_GetPosition(depthSam, g_ubBloomLightShaftsFS._matInvVP, ndcPos);

	const float3 eyePos = g_ubBloomLightShaftsFS._eyePos.xyz;
	const float3 toPosW = posW - eyePos;
	const float distToEye = length(toPosW);
	const float3 pickingRayDir = toPosW / distToEye;
	const float2 spec = pow(saturate(dot(g_ubBloomLightShaftsFS._dirToSun.xyz, pickingRayDir)), float2(7, sunGloss));
	const float strength = dot(spec, float2(wideStrength, sunStrength));

	float3 lightShafts = 0.0;
	if (strength >= 0.0001)
	{
		const float3 rand = Rand(si.pos.xy);

#if _SHADER_QUALITY <= _Q_LOW
		const int sampleCount = 8;
#elif _SHADER_QUALITY <= _Q_MEDIUM
		const int sampleCount = 12;
#elif _SHADER_QUALITY <= _Q_HIGH
		const int sampleCount = 16;
#elif _SHADER_QUALITY <= _Q_ULTRA
		const int sampleCount = 24;
#endif
		const float stride = maxDist / sampleCount;

		float pickingRayLen = rand.y * stride;
		float acc = 0.0;
		[unroll] for (int i = 0; i < sampleCount; ++i)
		{
			const float3 pos = eyePos + pickingRayDir * pickingRayLen;
			const float shadowMask = SimpleShadowMapCSM(
				g_texShadow,
				g_samShadow,
				pos,
				pos,
				g_ubBloomLightShaftsFS._matShadow,
				g_ubBloomLightShaftsFS._matShadowCSM1,
				g_ubBloomLightShaftsFS._matShadowCSM2,
				g_ubBloomLightShaftsFS._matShadowCSM3,
				g_ubBloomLightShaftsFS._matScreenCSM,
				g_ubBloomLightShaftsFS._csmSplitRanges,
				g_ubBloomLightShaftsFS._shadowConfig,
				false);
			acc += shadowMask * step(pickingRayLen, distToEye);
			pickingRayLen += stride;
		}
		lightShafts = acc * (1.0 / sampleCount) * g_ubBloomLightShaftsFS._sunColor.rgb * g_ubBloomFS._colorScale_colorBias_exposure.z * strength;
	}

	so.color.rgb = lightShafts;
#else
	const float colorScale = g_ubBloomFS._colorScale_colorBias_exposure.x;
	const float colorBias = g_ubBloomFS._colorScale_colorBias_exposure.y;

	const float4 colorSam = g_texColor.SampleLevel(g_samColor, si.tc0.zw, 0.0);
	const float3 color = colorSam.rgb * g_ubBloomFS._colorScale_colorBias_exposure.z;
	const float3 bloom = saturate((color - colorBias) * colorScale);

	so.color.rgb = bloom;
#endif

	so.color.a = 1.0;

	return so;
}
#endif

//@main:#
//@main:#LightShafts LIGHT_SHAFTS
