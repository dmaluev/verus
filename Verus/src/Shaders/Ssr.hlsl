// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Ssr.inc.hlsl"

ConstantBuffer<UB_SsrVS> g_ubSsrVS : register(b0, space0);
ConstantBuffer<UB_SsrFS> g_ubSsrFS : register(b0, space1);

Texture2D    g_texColor    : register(t1, space1);
SamplerState g_samColor    : register(s1, space1);
Texture2D    g_texGBuffer1 : register(t2, space1);
SamplerState g_samGBuffer1 : register(s2, space1);
Texture2D    g_texDepth    : register(t3, space1);
SamplerState g_samDepth    : register(s3, space1);
TextureCube  g_texEnv      : register(t4, space1);
SamplerState g_samEnv      : register(s4, space1);

static const float3 g_tetrahedron[4] =
{
	float3(-1.f, -1.f, -1.f),
	float3(+1.f, +1.f, -1.f),
	float3(-1.f, +1.f, +1.f),
	float3(+1.f, -1.f, +1.f)
};

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos    : SV_Position;
	float2 tc0    : TEXCOORD0;
};

struct FSO
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
};

float SoftBorderMask(float2 tc)
{
	const float2 mask = saturate((1.f - abs(0.5f - tc)) * 10.f - 5.f);
	return mask.x * mask.y;
}

float AlphaBoost(float alpha)
{
	const float x = 1.f - alpha;
	return 1.f - x * x;
}

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubSsrVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubSsrVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 rand = Rand(si.pos.xy);

	const float radius = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.x;
	const float depthBias = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.y;
	const float thickness = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.z;
	const float equalizeDist = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.w;

	// Position:
	const float2 ndcPos = ToNdcPos(si.tc0);
	const float rawDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
	const float3 posWV = DS_GetPosition(rawDepth, g_ubSsrFS._matInvP, ndcPos);

	// Normal:
	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);

	// Ray from eye:
	const float depth = length(posWV);
	const float3 rayFromEye = posWV / depth;

	// Reflected ray:
	const float3 reflectedDir = reflect(rayFromEye, normalWV);
	const float complement = saturate(dot(-rayFromEye, normalWV));
	const float fresnel = 1.f - complement * complement;

	// Environment map:
	const float3 reflectedDirW = mul(reflectedDir, (float3x3)g_ubSsrFS._matInvV);
	float3 envColor = 0.f;
#if _SHADER_QUALITY <= _Q_MEDIUM
	envColor = g_texEnv.SampleLevel(g_samEnv, reflectedDirW, 0.f).rgb * fresnel;
#else
	[unroll] for (int i = 0; i < 4; ++i)
	{
		const float3 dir = reflectedDirW + g_tetrahedron[i] * 0.003f;
		envColor += g_texEnv.SampleLevel(g_samEnv, dir, 0.f).rgb;
	}
	envColor *= 0.25f * fresnel;
#endif

	float4 color = float4(envColor, 0.f);

#ifdef DEF_SSR
	const float originDepth = ToLinearDepth(rawDepth, g_ubSsrFS._zNearFarEx);
	const float equalize = equalizeDist / originDepth;

	const float invRadius = 1.f / radius;
#if _SHADER_QUALITY <= _Q_LOW
	const int maxSampleCount = 6;
#elif _SHADER_QUALITY <= _Q_MEDIUM
	const int maxSampleCount = 8;
#elif _SHADER_QUALITY <= _Q_HIGH
	const int maxSampleCount = 12;
#elif _SHADER_QUALITY <= _Q_ULTRA
	const int maxSampleCount = 16;
#endif
	const int sampleCount = clamp(maxSampleCount * equalize, 3, maxSampleCount);
	const float stride = radius / sampleCount;
	const float3 rayStride = reflectedDir * stride;

	float3 kernelPosWV = posWV + normalWV * depthBias + rayStride * rand.x;
	float2 prevTcRaySample = si.tc0;
	float prevRayDeeper = _SINGULARITY_FIX;
	for (int j = 0; j < sampleCount; ++j)
	{
		float4 tcRaySample = mul(float4(kernelPosWV, 1), g_ubSsrFS._matPTex);
		tcRaySample /= tcRaySample.w;
		const float rawKernelDepth = g_texDepth.SampleLevel(g_samDepth, tcRaySample.xy, 0.f).r;

		const float2 rayDepth_texDepth = ToLinearDepth(float2(tcRaySample.z, rawKernelDepth), g_ubSsrFS._zNearFarEx);
		const float rayDeeper = rayDepth_texDepth.x - rayDepth_texDepth.y;

		if (rayDeeper > 0.f && rayDeeper < thickness) // Ray deeper, but not too deep?
		{
			const float ratio = prevRayDeeper / (prevRayDeeper + rayDeeper);
			const float2 tcFinal = lerp(prevTcRaySample, tcRaySample.xy, ratio);

			const float tcMask = SoftBorderMask(tcFinal);

			const float distMask = 1.f - ((j + ratio) * stride * invRadius);

			const float alpha = tcMask * AlphaBoost(distMask) * fresnel;

			color.rgb = lerp(envColor, g_texColor.SampleLevel(g_samColor, tcFinal, 0.f).rgb, alpha);
			color.a = alpha;
			break;
		}

		prevTcRaySample = tcRaySample.xy;
		prevRayDeeper = abs(rayDeeper);
		kernelPosWV += rayStride;
	}
#endif

	so.target0 = color;
	so.target1 = color.a;

	return so;
}
#endif

//@main:#
