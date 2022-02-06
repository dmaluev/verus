// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Blur.inc.hlsl"

ConstantBuffer<UB_BlurVS>      g_ubBlurVS      : register(b0, space0);
ConstantBuffer<UB_BlurFS>      g_ubBlurFS      : register(b0, space1);
ConstantBuffer<UB_ExtraBlurFS> g_ubExtraBlurFS : register(b0, space2);

Texture2D    g_tex         : register(t1, space1);
SamplerState g_sam         : register(s1, space1);
Texture2D    g_texGBuffer1 : register(t1, space2);
SamplerState g_samGBuffer1 : register(s1, space2);
Texture2D    g_texDepth    : register(t2, space2);
SamplerState g_samDepth    : register(s2, space2);

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

// <DefaultBlur>

float GlossBoost(float gloss)
{
	const float x = 1.f - gloss;
	return 1.f - x * x;
}

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#ifdef DEF_SSR
	const float alpha = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f).r;
	const float gloss = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).a;

	const float gloss64 = gloss * 64.f;
	const float eyeMask = saturate(1.f - gloss64);
	const float scaleByAlpha = 1.f - alpha;
	const float scaleByGloss = 1.f - GlossBoost(lerp(gloss, 0.75f, eyeMask));
	const float scale = max(0.001f, min(scaleByAlpha, scaleByGloss));

	const int sampleCount = max(4, g_ubBlurFS._sampleCount * scale);
	const float radius = g_ubBlurFS._radius_invRadius_stride.x * scale;
	const float invRadius = 1.f / radius;
	const float stride = radius * 2.f / (sampleCount - 1);
#else
	const int sampleCount = g_ubBlurFS._sampleCount;
	const float radius = g_ubBlurFS._radius_invRadius_stride.x;
	const float invRadius = g_ubBlurFS._radius_invRadius_stride.y;
	const float stride = g_ubBlurFS._radius_invRadius_stride.z;
#endif

	float4 acc = 0.f;
	float2 offset_weightSum = float2(-radius, 0);
	for (int i = 0; i < sampleCount; ++i)
	{
		// Poor man's gaussian kernel:
		const float curve = smoothstep(1.f, 0.f, abs(offset_weightSum.x) * invRadius);
		const float weight = curve * curve;
#ifdef DEF_U
		const float2 tc = si.tc0 + float2(offset_weightSum.x, 0);
#else
		const float2 tc = si.tc0 + float2(0, offset_weightSum.x);
#endif
		acc += g_tex.SampleLevel(g_sam, tc, 0.f) * weight;
		offset_weightSum += float2(stride, weight);
	}
	acc *= 1.f / offset_weightSum.y;

	so.color = acc;

	return so;
}
#endif

// </DefaultBlur>

// <DepthOfFieldBlur>

float DepthToCircleOfConfusion(float depth, float focusDist)
{
	return abs((depth - focusDist * 2.f) / depth + 1.f);
}

#ifdef _VS
VSO mainDofVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainDofFS(VSO si)
{
	FSO so;

	const float focusDist = g_ubExtraBlurFS._focusDist_blurStrength.x;
	const float blurStrength = g_ubExtraBlurFS._focusDist_blurStrength.y;

	const float rawOriginDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
	const float originDepth = ToLinearDepth(rawOriginDepth, g_ubExtraBlurFS._zNearFarEx);
	const float scale = DepthToCircleOfConfusion(originDepth, focusDist) * blurStrength;

	const int sampleCount = clamp(g_ubBlurFS._sampleCount * scale, 3, 31);
	const float radius = g_ubBlurFS._radius_invRadius_stride.x * scale;
	const float invRadius = 1.f / radius;
	const float2 blurDir = g_ubExtraBlurFS._blurDir.xy;
	const float2 blurDir2 = g_ubExtraBlurFS._blurDir.zw * radius;

	const float stride = radius * 2.f / (sampleCount - 1);
	const float2 stride2D = stride * blurDir;

	float4 acc = 0.f;
	float2 offset_weightSum = float2(-radius, 0);
	float2 tc = si.tc0 - blurDir * radius;
	for (int i = 0; i < sampleCount; ++i)
	{
		//const float origin = 1.f - abs(offset_weightSum.x) * invRadius; // Will generate buggy code in Vulkan!
		const float origin = abs(offset_weightSum.x) * invRadius;

		const float rawKernelDepth = g_texDepth.SampleLevel(g_samDepth, tc, 0.f).r;
		const float kernelDepth = ToLinearDepth(rawKernelDepth, g_ubExtraBlurFS._zNearFarEx);
		const float kernelDeeper = kernelDepth - originDepth;
		const float kernelScale = DepthToCircleOfConfusion(kernelDepth, focusDist) * blurStrength;
		// Blurry area should not sample sharp area unless it is closer to the camera.
		float weight = min(scale, min(2.f, max(kernelScale, kernelDeeper)));

#ifdef DEF_U // 1st pass - make a rhombus:
		weight *= 1.f - 0.5f * origin;
		const float4 colorA = g_tex.SampleLevel(g_sam, tc - blurDir2 + blurDir2 * origin, 0.f);
		const float4 colorB = g_tex.SampleLevel(g_sam, tc + blurDir2 - blurDir2 * origin, 0.f);
		acc += lerp(colorA, colorB, 0.5f) * weight;
#else // 2nd pass - blur the rhombus:
		acc += g_tex.SampleLevel(g_sam, tc, 0.f) * weight;
#endif

		offset_weightSum += float2(stride, weight);
		tc += stride2D;
	}
	acc *= 1.f / offset_weightSum.y;

	so.color = acc;

	return so;
}
#endif

// </DepthOfFieldBlur>

// <SsaoBlur>

#ifdef _VS
VSO mainSsaoVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainSsaoFS(VSO si)
{
	FSO so;

	// 4x4 box blur, using texture filtering.
	float4 acc = 0.f;
	[unroll] for (int i = -1; i <= 1; i += 2)
	{
#ifdef DEF_U
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.f, int2(i, 0));
#else // Sampling from sRGB, use alpha channel:
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.f, int2(0, i)).aaaa;
#endif
	}
	acc *= 0.5f;

	so.color = acc;
#ifndef DEF_V // Make sure to write R channel to alpha, destination is sRGB.
	so.color = acc.rrrr;
#endif

	return so;
}
#endif

// </SsaoBlur>

#ifdef _VS
VSO mainAntiAliasingVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainAntiAliasingFS(VSO si)
{
	FSO so;

	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);

	// <DepthBased>
	float originDeeper;
	float depthBasedEdge;
	{
		const float rawOriginDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
		const float originDepth = ToLinearDepth(rawOriginDepth, g_ubExtraBlurFS._zNearFarEx);
		const float4 rawKernelDepths = float4(
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f, int2(-1, +0)).r, // L
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f, int2(+0, -1)).r, // T
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f, int2(+1, +0)).r, // R
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f, int2(+0, +1)).r); // B
		const float4 kernelDepths = ToLinearDepth(rawKernelDepths, g_ubExtraBlurFS._zNearFarEx);
		const float minDepth = min(min(kernelDepths[0], kernelDepths[1]), min(kernelDepths[2], kernelDepths[3]));
		const float equalize = 1.f / originDepth;
		originDeeper = saturate((originDepth - minDepth) * equalize);
		const float4 depthOffsets = abs((originDepth - kernelDepths) * equalize);
		depthBasedEdge = saturate(dot(depthOffsets, 1.f));
	}
	// </DepthBased>

	// <NormalBased>
	float normalBasedEdge;
	{
		const float4 rawNrmLT = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f, int2(-1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f, int2(+0, -1)).rg);
		const float4 rawNrmRB = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f, int2(+1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f, int2(+0, +1)).rg);
		const float4 diffA = rawNrmLT - rawNrmRB;
		const float4 diffB = rawNrmLT - rawNrmRB.zwxy;
		const float4 dots = float4(
			dot(diffA.xy, diffA.xy),
			dot(diffA.zw, diffA.zw),
			dot(diffB.xy, diffB.xy),
			dot(diffB.zw, diffB.zw));
		const float maxDot = max(max(dots.x, dots.y), max(dots.z, dots.w));
		normalBasedEdge = saturate(maxDot * maxDot * 10.f);
	}
	// </NormalBased>

	// Directional blur:
	// {y, -x} is perpendicular vector. Also flip Y axis: normal XY to texture UV.
	const float3 perp = normalWV.yxz;
	const float omni = max(perp.z * perp.z * perp.z, originDeeper);
	const float2 dirs[4] =
	{
		lerp(perp.xy * +4.f, float2(-0.6f, -0.3f), omni),
		lerp(perp.xy * -2.f, float2(+0.3f, -0.6f), omni),
		lerp(perp.xy * -4.f, float2(-0.3f, +0.6f), omni),
		lerp(perp.xy * +2.f, float2(+0.6f, +0.3f), omni)
	};
	const float2 offsetScale = g_ubExtraBlurFS._textureSize.zw * max(normalBasedEdge, depthBasedEdge);
	const float3 kernelColors[4] =
	{
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[0] * offsetScale, 0.f).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[1] * offsetScale, 0.f).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[2] * offsetScale, 0.f).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[3] * offsetScale, 0.f).rgb
	};
	so.color.rgb = (kernelColors[0] + kernelColors[1] + kernelColors[2] + kernelColors[3]) * 0.25f;
	so.color.a = 1.f;

	return so;
}
#endif

#ifdef _VS
VSO mainMotionVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainMotionFS(VSO si)
{
	FSO so;

	const float3 rand = Rand(si.pos.xy);
	const float offsetScale = 0.45f + 0.05f * rand.x; // Blur 45% - 50% of frame time.

#if _SHADER_QUALITY <= _Q_LOW
	const int sampleCount = 12;
#elif _SHADER_QUALITY <= _Q_MEDIUM
	const int sampleCount = 16;
#elif _SHADER_QUALITY <= _Q_HIGH
	const int sampleCount = 24;
#elif _SHADER_QUALITY <= _Q_ULTRA
	const int sampleCount = 32;
#endif

	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f);

	const float rawOriginDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
	const float originDepth = ToLinearDepth(rawOriginDepth, g_ubExtraBlurFS._zNearFarEx);
	const float equalize = 1.f / originDepth;

	float2 tcFrom;
	{
		const float2 ndcPos = ToNdcPos(si.tc0);
		const float3 posW = DS_GetPosition(rawOriginDepth, g_ubExtraBlurFS._matInvVP, ndcPos);
		const float4 prevClipSpacePos = mul(float4(posW, 1), g_ubExtraBlurFS._matPrevVP);
		const float2 prevNdcPos = prevClipSpacePos.xy / prevClipSpacePos.w;
		tcFrom = ToTexCoords(prevNdcPos);
	}

	const float2 stride = (si.tc0 - tcFrom) * offsetScale / (sampleCount - 1);
	const float2 tcOrigin = lerp(tcFrom, si.tc0, 0.75f); // Start from this location up to offsetScale.

	float4 acc = float4(g_tex.SampleLevel(g_sam, si.tc0, 0.f).rgb, 1); // Must have at least one sample.
	[unroll] for (int i = 0; i < sampleCount; ++i)
	{
		if (i == sampleCount / 2)
			continue; // Midpoint already sampled.

		const float2 kernelCoords = tcOrigin + stride * i;

		const float rawKernelDepth = g_texDepth.SampleLevel(g_samDepth, kernelCoords, 0.f).r;
		const float kernelDepth = ToLinearDepth(rawKernelDepth, g_ubExtraBlurFS._zNearFarEx);
		const float kernelDeeper = kernelDepth - originDepth;
		const float allowed = saturate(1.f + kernelDeeper * equalize) * rawGBuffer1.a; // Closer points require extra care.
		const float weight = 1.f + saturate(kernelDeeper); // To fix the seam between foreground and background.

		const float3 kernelColor = g_tex.SampleLevel(g_sam, kernelCoords, 0.f).rgb;
		acc += lerp(0.f, float4(kernelColor * weight, weight), allowed);
	}
	acc /= acc.a;

	so.color = float4(acc.rgb, 1);

	return so;
}
#endif

//@main:#U U
//@main:#V V
//@mainDof:#UDoF DOF U
//@mainDof:#VDoF DOF V
//@mainSsao:#USsao SSAO U
//@mainSsao:#VSsao SSAO V
//@main:#USsr SSR U
//@main:#VSsr SSR V
//@mainAntiAliasing:#AntiAliasing AA
//@mainMotion:#Motion MOTION
