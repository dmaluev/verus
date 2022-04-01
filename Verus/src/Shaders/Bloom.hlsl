// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Bloom.inc.hlsl"

ConstantBuffer<UB_BloomVS>            g_ubBloomVS            : register(b0, space0);
ConstantBuffer<UB_BloomFS>            g_ubBloomFS            : register(b0, space1);
ConstantBuffer<UB_BloomLightShaftsFS> g_ubBloomLightShaftsFS : register(b0, space2);

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

#ifdef DEF_LIGHT_SHAFTS
	const float2 ndcPos = ToNdcPos(si.tc0);

	const float maxDist = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.x;
	const float sunGloss = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.y;
	const float wideStrength = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.z;
	const float sunStrength = g_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.w;

	const float depthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
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

	const float4 colorSam = g_texColor.SampleLevel(g_samColor, si.tc0, 0.0);
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
