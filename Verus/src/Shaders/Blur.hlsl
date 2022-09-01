// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Blur.inc.hlsl"

CBUFFER(0, UB_BlurVS, g_ubBlurVS)
CBUFFER(1, UB_BlurFS, g_ubBlurFS)
CBUFFER(2, UB_ExtraBlurFS, g_ubExtraBlurFS)

Texture2D    g_tex         : REG(t1, space1, t0);
SamplerState g_sam         : REG(s1, space1, s0);
Texture2D    g_texGBuffer1 : REG(t1, space2, t1);
SamplerState g_samGBuffer1 : REG(s1, space2, s1);
Texture2D    g_texDepth    : REG(t2, space2, t2);
SamplerState g_samDepth    : REG(s2, space2, s2);

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

// <DefaultBlur>

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const int sampleCount = g_ubBlurFS._sampleCount;
	const float radius = g_ubBlurFS._radius_invRadius_stride.x;
	const float invRadius = g_ubBlurFS._radius_invRadius_stride.y;
	const float stride = g_ubBlurFS._radius_invRadius_stride.z;

	float4 acc = 0.0;
	float2 offset_weightSum = float2(-radius, 0);
	for (int i = 0; i < sampleCount; ++i)
	{
		// Poor man's gaussian kernel:
		const float ratio = 1.0 - abs(offset_weightSum.x * invRadius);
		const float curve = smoothstep(0.0, 1.0, ratio);
		const float weight = lerp(ratio, curve * curve * curve, 0.8);
#ifdef DEF_U
		const float2 tc = si.tc0.zw + float2(offset_weightSum.x, 0) * g_ubBlurFS._tcViewScaleBias.x;
#else
		const float2 tc = si.tc0.zw + float2(0, offset_weightSum.x) * g_ubBlurFS._tcViewScaleBias.y;
#endif
		acc += g_tex.SampleLevel(g_sam, tc, 0.0) * weight;
		offset_weightSum += float2(stride, weight);
	}
	acc *= 1.0 / offset_weightSum.y;

	so.color = acc;

	return so;
}
#endif

// </DefaultBlur>

// <BoxBlur>

#ifdef _VS
VSO mainBoxVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainBoxFS(VSO si)
{
	FSO so;

	const int sampleCount = g_ubBlurFS._sampleCount;
	const float radius = g_ubBlurFS._radius_invRadius_stride.x;
	const float invRadius = g_ubBlurFS._radius_invRadius_stride.y;
	const float stride = g_ubBlurFS._radius_invRadius_stride.z;

	float4 acc = 0.0;
	float offset = -radius;
	for (int i = 0; i < sampleCount; ++i)
	{
#ifdef DEF_U
		const float2 tc = si.tc0.zw + float2(offset, 0) * g_ubBlurFS._tcViewScaleBias.x;
#else
		const float2 tc = si.tc0.zw + float2(0, offset) * g_ubBlurFS._tcViewScaleBias.y;
#endif
		acc += g_tex.SampleLevel(g_sam, tc, 0.0);
		offset += stride;
	}
	acc *= 1.0 / sampleCount;

	so.color = acc;

	return so;
}
#endif

// </BoxBlur>

// <SsaoBlur>

#ifdef _VS
VSO mainSsaoVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainSsaoFS(VSO si)
{
	FSO so;

	// 4x4 box blur, using texture filtering.
	float acc = 0.0;
	[unroll] for (int y = -1; y <= 1; y += 2)
	{
		[unroll] for (int x = -1; x <= 1; x += 2)
		{
			acc += g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0, int2(x, y)).r;
		}
	}
	acc *= (1.0 / 4.0);

	so.color = acc;

	return so;
}
#endif

// </SsaoBlur>

// <ResolveDithering>

#ifdef _VS
VSO mainResolveDitheringVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainResolveDitheringFS(VSO si)
{
	FSO so;

	float accMask = 0.0;
	[unroll] for (int y = -1; y <= 1; y++)
	{
		[unroll] for (int x = -1; x <= 1; x++)
		{
			accMask += floor(g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(x, y)).a);
		}
	}

	if (accMask >= 1.0)
	{
		const float invExposure = max(1024.0, 8192.0 - 2048.0 * accMask);
		const float exposure = 1.0 / invExposure;

		// 3x3 blur:
		float3 acc = 0.0;
		[unroll] for (int y = -1; y <= 1; y++)
		{
			[unroll] for (int x = -1; x <= 1; x++)
			{
				const float2 weights = 1.0 - 0.5 * abs(float2(x, y));
				const float weight = weights.x * weights.y;
				const float3 color = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0, int2(x, y)).rgb;
				acc += ToneMappingReinhard(color * exposure) * weight;
			}
		}
		acc *= (1.0 / 4.0);

		so.color.rgb = ToneMappingInvReinhard(acc) * invExposure;
		so.color.a = 1.0;
	}
	else
	{
		so.color.rgb = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0).rgb;
		so.color.a = 1.0;
	}

	return so;
}
#endif

// </ResolveDithering>

// <Sharpen>

#ifdef _VS
VSO mainSharpenVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainSharpenFS(VSO si)
{
	FSO so;

	float accMask = 0.0;
	[unroll] for (int y = -1; y <= 1; y++)
	{
		[unroll] for (int x = -1; x <= 1; x++)
		{
			accMask += floor(g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(x, y)).a);
		}
	}

	if (accMask >= 1.0)
	{
		const float invExposure = max(1024.0, 8192.0 - 2048.0 * accMask);
		const float exposure = 1.0 / invExposure;

		// 3x3 sharpen:
		float3 acc = 0.0;
		[unroll] for (int y = -1; y <= 1; y++)
		{
			[unroll] for (int x = -1; x <= 1; x++)
			{
				float weight = -1.0 / 8.0;
				if (abs(x) + abs(y) == 0)
					weight = 2.0;
				const float3 color = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0, int2(x, y)).rgb;
				acc += ToneMappingReinhard(color * exposure) * weight;
			}
		}

		so.color.rgb = ToneMappingInvReinhard(acc) * invExposure;
		so.color.a = 1.0;
	}
	else
	{
		so.color.rgb = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0).rgb;
		so.color.a = 1.0;
	}

	return so;
}
#endif

// </Sharpen>

// <DepthOfFieldBlur>

float DepthToCircleOfConfusion(float depth, float focusDist)
{
	return abs((depth - focusDist * 2.0) / depth + 1.0);
}

#ifdef _VS
VSO mainDofVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainDofFS(VSO si)
{
	FSO so;

	const float focusDist = g_ubExtraBlurFS._focusDist_blurStrength.x;
	const float blurStrength = g_ubExtraBlurFS._focusDist_blurStrength.y;

	const float originDepthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0).r;
	const float originDepth = ToLinearDepth(originDepthSam, g_ubExtraBlurFS._zNearFarEx);
	const float scale = DepthToCircleOfConfusion(originDepth, focusDist) * blurStrength;

	const int sampleCount = clamp(g_ubBlurFS._sampleCount * scale, 3, 31);
	const float radius = g_ubBlurFS._radius_invRadius_stride.x * scale;
	const float invRadius = 1.0 / radius;
	const float2 blurDir = g_ubExtraBlurFS._blurDir.xy;
	const float2 blurDir2 = g_ubExtraBlurFS._blurDir.zw * radius;

	const float stride = radius * 2.0 / (sampleCount - 1);
	const float2 stride2D = stride * blurDir;

	float4 acc = 0.0;
	float2 offset_weightSum = float2(-radius, 0);
	float2 tc = si.tc0.xy - blurDir * radius;
	for (int i = 0; i < sampleCount; ++i)
	{
		const float origin = abs(offset_weightSum.x) * invRadius;

		const float2 tcView = tc * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw;

		const float kernelDepthSam = g_texDepth.SampleLevel(g_samDepth, tcView, 0.0).r;
		const float kernelDepth = ToLinearDepth(kernelDepthSam, g_ubExtraBlurFS._zNearFarEx);
		const float kernelDeeper = kernelDepth - originDepth;
		const float kernelScale = DepthToCircleOfConfusion(kernelDepth, focusDist) * blurStrength;
		// Blurry area should not sample sharp area unless it is closer to the camera.
		float weight = min(scale, min(2.0, max(kernelScale, kernelDeeper)));

#ifdef DEF_U // 1st pass - make a rhombus:
		weight *= 1.0 - 0.5 * origin;
		const float4 colorA = g_tex.SampleLevel(g_sam, (tc - blurDir2 + blurDir2 * origin) *
			g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0);
		const float4 colorB = g_tex.SampleLevel(g_sam, (tc + blurDir2 - blurDir2 * origin) *
			g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0);
		acc += lerp(colorA, colorB, 0.5) * weight;
#else // 2nd pass - blur the rhombus:
		acc += g_tex.SampleLevel(g_sam, tcView, 0.0) * weight;
#endif

		offset_weightSum += float2(stride, weight);
		tc += stride2D;
	}
	acc *= 1.0 / offset_weightSum.y;

	so.color = acc;

	return so;
}
#endif

// </DepthOfFieldBlur>

#ifdef _VS
VSO mainAntiAliasingVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainAntiAliasingFS(VSO si)
{
	FSO so;

#ifdef DEF_OFF
	so.color = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0);
#else
	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0);
	const float3 normalWV = DS_GetNormal(gBuffer1Sam);

	// <DepthBased>
	float originDeeper;
	float depthBasedEdge;
	{
		const float originDepthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0).r;
		const float originDepth = ToLinearDepth(originDepthSam, g_ubExtraBlurFS._zNearFarEx);
		const float4 kernelDepthsSam = float4(
			g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0, int2(-1, +0)).r, // L
			g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0, int2(+0, -1)).r, // T
			g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0, int2(+1, +0)).r, // R
			g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0, int2(+0, +1)).r); // B
		const float4 kernelDepths = ToLinearDepth(kernelDepthsSam, g_ubExtraBlurFS._zNearFarEx);
		const float minDepth = min(min(kernelDepths[0], kernelDepths[1]), min(kernelDepths[2], kernelDepths[3]));
		const float equalize = 1.0 / originDepth;
		originDeeper = saturate((originDepth - minDepth) * equalize);
		const float4 depthOffsets = abs((originDepth - kernelDepths) * equalize);
		depthBasedEdge = saturate(dot(depthOffsets, 1.0));
	}
	// </DepthBased>

	// <NormalBased>
	float normalBasedEdge;
	{
		const float4 nrmSamLT = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(-1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(+0, -1)).rg);
		const float4 nrmSamRB = float4(
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(+1, +0)).rg,
			g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0, int2(+0, +1)).rg);
		const float4 diffA = nrmSamLT - nrmSamRB;
		const float4 diffB = nrmSamLT - nrmSamRB.zwxy;
		const float4 dots = float4(
			dot(diffA.xy, diffA.xy),
			dot(diffA.zw, diffA.zw),
			dot(diffB.xy, diffB.xy),
			dot(diffB.zw, diffB.zw));
		const float maxDot = max(max(dots.x, dots.y), max(dots.z, dots.w));
		normalBasedEdge = saturate(maxDot * maxDot * 10.0);
	}
	// </NormalBased>

	const float edge = max(normalBasedEdge, depthBasedEdge);

	// Directional blur:
	// {y, -x} is perpendicular vector. Also flip Y axis: normal XY to texture UV.
	const float3 perp = normalWV.yxz;
	const float omni = max(perp.z * perp.z * perp.z, originDeeper);
	const float2 dirs[4] =
	{
		lerp(perp.xy * +3.0, float2(-0.6, -0.2), omni),
		lerp(perp.xy * -1.0, float2(+0.2, -0.6), omni),
		lerp(perp.xy * -3.0, float2(-0.2, +0.6), omni),
		lerp(perp.xy * +1.0, float2(+0.6, +0.2), omni)
	};
	const float2 offsetScale = g_ubExtraBlurFS._textureSize.zw * edge;
	const float3 kernelColors[4] =
	{
		g_tex.SampleLevel(g_sam, (si.tc0.xy + dirs[0] * offsetScale) * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0).rgb,
		g_tex.SampleLevel(g_sam, (si.tc0.xy + dirs[1] * offsetScale) * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0).rgb,
		g_tex.SampleLevel(g_sam, (si.tc0.xy + dirs[2] * offsetScale) * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0).rgb,
		g_tex.SampleLevel(g_sam, (si.tc0.xy + dirs[3] * offsetScale) * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw, 0.0).rgb
	};
	so.color.rgb = (kernelColors[0] + kernelColors[1] + kernelColors[2] + kernelColors[3]) * 0.25;
	so.color.a = 1.0;
#endif

	return so;
}
#endif

#ifdef _VS
VSO mainMotionVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubBlurVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubBlurVS._tcViewScaleBias.xy + g_ubBlurVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainMotionFS(VSO si)
{
	FSO so;

#ifdef DEF_OFF
	so.color = g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0);
#else
	const float3 rand = Rand(si.pos.xy);
	const float2 ndcPos = ToNdcPos(si.tc0.xy);
	const float offsetScale = 0.6 + 0.1 * rand.x; // Blur 60% - 70% of frame time.

#if _SHADER_QUALITY <= _Q_LOW
	const int sampleCount = 6;
#elif _SHADER_QUALITY <= _Q_MEDIUM
	const int sampleCount = 8;
#elif _SHADER_QUALITY <= _Q_HIGH
	const int sampleCount = 12;
#elif _SHADER_QUALITY <= _Q_ULTRA
	const int sampleCount = 16;
#endif

	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0.zw, 0.0);

	const float originDepthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0.zw, 0.0).r;
	const float originDepth = ToLinearDepth(originDepthSam, g_ubExtraBlurFS._zNearFarEx);
	const float equalize = 1.0 / originDepth;

	float2 tcFrom;
	{
		const float3 posW = DS_GetPosition(originDepthSam, g_ubExtraBlurFS._matInvVP, ndcPos);
		const float4 prevClipSpacePos = mul(float4(posW, 1), g_ubExtraBlurFS._matPrevVP);
		const float2 prevNdcPos = prevClipSpacePos.xy / prevClipSpacePos.w;
		tcFrom = ToTexCoords(prevNdcPos);
	}

	const float2 stride = (si.tc0.xy - tcFrom) * offsetScale / (sampleCount - 1);
	const float2 tcOrigin = lerp(tcFrom, si.tc0.xy, 0.675); // Start from this location up to offsetScale.

	float4 acc = float4(g_tex.SampleLevel(g_sam, si.tc0.zw, 0.0).rgb, 1); // Must have at least one sample.
	[unroll] for (int i = 0; i < sampleCount; ++i)
	{
		if (i == sampleCount / 2)
			continue; // Midpoint already sampled.

		const float2 kernelCoords = (tcOrigin + stride * i) * g_ubBlurFS._tcViewScaleBias.xy + g_ubBlurFS._tcViewScaleBias.zw;

		const float kernelDepthSam = g_texDepth.SampleLevel(g_samDepth, kernelCoords, 0.0).r;
		const float kernelDepth = ToLinearDepth(kernelDepthSam, g_ubExtraBlurFS._zNearFarEx);
		const float kernelDeeper = kernelDepth - originDepth;
		const float allowed = saturate(1.0 + kernelDeeper * equalize) * gBuffer1Sam.a; // Closer points require extra care.
		const float weight = 1.0 + saturate(kernelDeeper); // To fix the seam between foreground and background.

		const float3 kernelColorSam = g_tex.SampleLevel(g_sam, kernelCoords, 0.0).rgb;
		acc += lerp(0.0, float4(kernelColorSam * weight, weight), allowed);
	}
	acc /= acc.a;

	so.color = float4(acc.rgb, 1);
#endif

	return so;
}
#endif

//@main:#U U
//@main:#V V
//@mainBox:#UBox U
//@mainBox:#VBox V
//@mainSsao:#Ssao
//@mainResolveDithering:#ResolveDithering
//@mainSharpen:#Sharpen
//@mainDof:#UDoF U
//@mainDof:#VDoF V
//@mainAntiAliasing:#AntiAliasing
//@mainAntiAliasing:#AntiAliasingOff OFF
//@mainMotion:#Motion
//@mainMotion:#MotionOff OFF
