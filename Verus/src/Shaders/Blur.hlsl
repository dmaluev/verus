// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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

#if _ANISOTROPY_LEVEL > 4
	const int stride = 2;
#else
	const int stride = 4;
#endif

	float4 acc = 0.0;
#ifdef DEF_SSAO
	const float weightSum = 2.0;
	[unroll] for (int i = -1; i <= 1; i += 2)
#else
	float weightSum = _SINGULARITY_FIX;
	[unroll] for (int i = -8; i <= 7; i += stride)
#endif
	{
#ifdef DEF_VERTICAL
		const int2 offset = int2(0, i);
#else
		const int2 offset = int2(i, 0);
#endif

#ifdef DEF_SSAO
		const float weight = 1.0;
#else
		const float weight = smoothstep(0.0, 1.0, (1.0 - abs(i) * 0.1));
		weightSum += weight;
#endif

#if defined(DEF_SSAO) && defined(DEF_VERTICAL) // Sampling from sRGB, use alpha channel:
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.0, offset).aaaa;
#else
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.0, offset) * weight;
#endif
	}
	acc *= (1.0 / weightSum);

	so.color = acc;
#if defined(DEF_SSAO) && !defined(DEF_VERTICAL) // Make sure to write R channel to alpha, destination is sRGB.
	so.color = acc.rrrr;
#endif

	return so;
}
#endif

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

	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float coarseAlpha = normalWV.b * normalWV.b * normalWV.b;

	// <DepthBased>
	float3 coarseNorm;
	float originDeeper;
	float depthBasedEdge;
	{
		const float rawOriginDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
		const float originDepth = ToLinearDepth(rawOriginDepth, g_ubExtraBlurFS._zNearFarEx);
		const float4 rawKernelDepths = float4(
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0, int2(-1, +0)).r, // L
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0, int2(+0, -1)).r, // T
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0, int2(+1, +0)).r, // R
			g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0, int2(+0, +1)).r); // B
		const float4 kernelDepths = ToLinearDepth(rawKernelDepths, g_ubExtraBlurFS._zNearFarEx);
		const float minDepth = min(min(kernelDepths[0], kernelDepths[1]), min(kernelDepths[2], kernelDepths[3]));
		const float equalize = 1.0 / originDepth;
		originDeeper = saturate((originDepth - minDepth) * equalize);
		const float4 depthOffsets = abs((originDepth - kernelDepths) * equalize);
		depthBasedEdge = saturate(dot(depthOffsets, 1.0));

		const float3 v0 = float3(1, 0, kernelDepths[0] - kernelDepths[2]);
		const float3 v1 = float3(0, 1, kernelDepths[3] - kernelDepths[1]);
		coarseNorm = normalize(cross(v0, v1));
	}
	// </DepthBased>

	// <NormalBased>
	float normalBasedEdge;
	{
		const float4 rawNrmLT = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(-1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(+0, -1)).rg);
		const float4 rawNrmRB = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(+1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(+0, +1)).rg);
		const float4 diffA = rawNrmLT - rawNrmRB;
		const float4 diffB = rawNrmLT - rawNrmRB.zwxy;
		const float4 dots = float4(
			dot(diffA.xy, diffA.xy),
			dot(diffA.zw, diffA.zw),
			dot(diffB.xy, diffB.xy),
			dot(diffB.zw, diffB.zw));
		const float maxDot = max(max(dots.x, dots.y), max(dots.z, dots.w));
		normalBasedEdge = saturate(maxDot * maxDot * 10.0);
	}
	// </NormalBased>

	// Directional blur:
	const float3 normal = lerp(normalWV, coarseNorm, coarseAlpha * 0.5);
	// {y, -x} is perpendicular vector. Also flip Y axis: normal XY to texture UV.
	const float3 perp = normal.yxz;
	const float omni = max(perp.z * perp.z * perp.z, originDeeper);
	const float2 dirs[4] =
	{
		lerp(perp.xy * +4.0, float2(-0.4, -0.2), omni),
		lerp(perp.xy * -2.0, float2(+0.2, -0.4), omni),
		lerp(perp.xy * -4.0, float2(-0.2, +0.4), omni),
		lerp(perp.xy * +2.0, float2(+0.4, +0.2), omni)
	};
	const float2 offsetScale = g_ubExtraBlurFS._textureSize.zw * max(normalBasedEdge, depthBasedEdge);
	const float3 kernelColors[4] =
	{
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[0] * offsetScale, 0.0).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[1] * offsetScale, 0.0).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[2] * offsetScale, 0.0).rgb,
		g_tex.SampleLevel(g_sam, si.tc0 + dirs[3] * offsetScale, 0.0).rgb
	};
	const float3 sdrKernelColors[4] =
	{
		ToneMappingReinhard(kernelColors[0] * 0.001),
		ToneMappingReinhard(kernelColors[1] * 0.001),
		ToneMappingReinhard(kernelColors[2] * 0.001),
		ToneMappingReinhard(kernelColors[3] * 0.001)
	};
	so.color.rgb = (sdrKernelColors[0] + sdrKernelColors[1] + sdrKernelColors[2] + sdrKernelColors[3]) * 0.25;
	so.color.rgb = ToneMappingInvReinhard(so.color.rgb) * 1000.0;
	so.color.a = 1.0;

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
	const float offsetScale = 0.5 + 0.3 * rand.x; // Blur 50% - 80% of frame time.

	const int sampleCount = max(4, _ANISOTROPY_LEVEL);

	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);

	const float rawOriginDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
	const float originDepth = ToLinearDepth(rawOriginDepth, g_ubExtraBlurFS._zNearFarEx);

	float2 tcFrom;
	{
		const float2 ndcPos = ToNdcPos(si.tc0);
		const float3 posW = DS_GetPosition(rawOriginDepth, g_ubExtraBlurFS._matInvVP, ndcPos);
		const float4 prevClipSpacePos = mul(float4(posW, 1), g_ubExtraBlurFS._matPrevVP);
		const float2 prevNdcPos = prevClipSpacePos.xy / prevClipSpacePos.w;
		tcFrom = ToTexCoords(prevNdcPos);
	}

	const float2 stride = (si.tc0 - tcFrom) * offsetScale / (sampleCount - 1);
	const float2 tcOrigin = lerp(tcFrom, si.tc0, 0.7);

	float4 acc = float4(g_tex.SampleLevel(g_sam, si.tc0, 0.0).rgb, 1);
	[unroll] for (int i = 0; i < sampleCount; i++)
	{
		if (i == sampleCount / 2)
			continue;

		const float2 kernelCoords = tcOrigin + stride * i;

		const float rawKernelDepth = g_texDepth.SampleLevel(g_samDepth, kernelCoords, 0.0).r;
		const float kernelDepth = ToLinearDepth(rawKernelDepth, g_ubExtraBlurFS._zNearFarEx);
		const float kernelDeeper = kernelDepth - originDepth;
		const float allowed = saturate(kernelDeeper * 0.2 + 1.0) * rawGBuffer1.a;
		const float weight = saturate(kernelDeeper * 0.2) + 1.0;

		const float3 kernelColor = g_tex.SampleLevel(g_sam, kernelCoords, 0.0).rgb;
		acc += lerp(0.0, float4(kernelColor * weight, weight), allowed);
	}
	acc /= acc.a;

	so.color = float4(acc.rgb, 1);

	return so;
}
#endif

//@main:#H
//@main:#V VERTICAL
//@main:#HSsao SSAO
//@main:#VSsao SSAO VERTICAL
//@mainAntiAliasing:#AntiAliasing AA
//@mainMotion:#Motion MOTION
