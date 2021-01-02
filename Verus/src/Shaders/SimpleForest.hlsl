// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "SimpleForest.inc.hlsl"

ConstantBuffer<UB_SimpleForestVS> g_ubSimpleForestVS : register(b0, space0);
ConstantBuffer<UB_SimpleForestFS> g_ubSimpleForestFS : register(b0, space1);

Texture2D              g_texGBuffer0  : register(t1, space1);
SamplerState           g_samGBuffer0  : register(s1, space1);
Texture2D              g_texGBuffer1  : register(t2, space1);
SamplerState           g_samGBuffer1  : register(s2, space1);
Texture2D              g_texGBuffer2  : register(t3, space1);
SamplerState           g_samGBuffer2  : register(s3, space1);
Texture2D              g_texShadowCmp : register(t4, space1);
SamplerComparisonState g_samShadowCmp : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
};

struct VSO
{
	float4 pos         : SV_Position;
	float2 tc0         : TEXCOORD0;
	float2 angles      : TEXCOORD1;
	float3 dirToEye    : TEXCOORD2;
	float4 posW_depth  : TEXCOORD3;
	float4 color       : COLOR0;
	float4 psize       : PSIZE;
	float clipDistance : CLIPDIST;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubSimpleForestVS._eyePos_clipDistanceOffset.xyz;
	const float clipDistanceOffset = g_ubSimpleForestVS._eyePos_clipDistanceOffset.w;

	const float pointSpriteSize = si.tc0.x * (1.f / 500.f);
	const float angle = si.tc0.y * (1.f / 32767.f);

	const float3 toEye = eyePos - si.pos.xyz;
	const float distToScreen = length(g_ubSimpleForestVS._eyePosScreen.xyz - si.pos.xyz);

	const float nearAlpha = saturate((distToScreen - 6.f) * (1.f / 3.f)); // From 6m to 9m.
	const float farAlpha = 1.f - saturate((distToScreen - 900.f) * (1.f / 100.f)); // From 900m to 1000m.

	so.pos = mul(si.pos, g_ubSimpleForestVS._matWVP);
	so.tc0 = 0.f;
	so.dirToEye = toEye;
	so.posW_depth = float4(si.pos.xyz, so.pos.z);
	so.color.rgb = RandomColor(si.pos.xz, 0.3f, 0.2f);
	so.color.a = nearAlpha * farAlpha;
	so.psize = (pointSpriteSize * (g_ubSimpleForestVS._viewportSize.yx * g_ubSimpleForestVS._viewportSize.z)).xyxy;
	so.psize.xy *= g_ubSimpleForestVS._pointSpriteScale.xy * g_ubSimpleForestVS._matP._m11;
	so.clipDistance = clipDistanceOffset;

	float2 param0 = toEye.xy;
	float2 param1 = toEye.zz;
	param0.y = max(0.f, param0.y); // Only upper hemisphere.
	param1.y = length(toEye.xz); // Distance in XZ-plane.
	so.angles.xy = (atan2(param0, param1) + _PI) * (0.5f / _PI); // atan2(x, z) and atan2(max(0.f, y), length(toEye.xz)). From 0 to 1.
	so.angles.y = (so.angles.y - 0.5f) * 4.f; // Choose this quadrant.
	so.angles.xy = saturate(so.angles.xy);
	so.angles.x = frac(so.angles.x - angle + 0.5f); // Turn.

	return so;
}
#endif

#ifdef _GS
[maxvertexcount(4)]
void mainGS(point VSO si[1], inout TriangleStream<VSO> stream)
{
	VSO so;

	so = si[0];
	const float2 center = so.pos.xy;
	const float2 centerW = so.posW_depth.xy;
	for (int i = 0; i < 4; ++i)
	{
		so.pos.xy = center + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.xy;
		so.tc0.xy = _POINT_SPRITE_TEX_COORDS[i];
		so.posW_depth.xy = centerW + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.zw;
		so.clipDistance += so.posW_depth.y;
		stream.Append(so);
	}
}
#endif

float2 ComputeTexCoords(float2 tc, float2 angles)
{
	const float marginBias = 16.f / 512.f;
	const float marginScale = 1.f - marginBias * 2.f;
	const float2 tcMargin = tc * marginScale + marginBias;

	const float2 frameCount = float2(16, 16);
	const float2 frameScale = 1.f / frameCount;
	const float2 frameBias = floor(min(angles * frameCount + 0.5f, float2(256, frameCount.y - 0.5f)));
	return (tcMargin + frameBias) * frameScale;
}

float ComputeMask(float2 tc, float alpha)
{
	const float2 tcCenter = tc - 0.5f;
	const float rad = saturate(dot(tcCenter, tcCenter) * 4.f);
	return saturate(rad + (alpha * 2.f - 1.f));
}

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	const float2 tc = ComputeTexCoords(si.tc0, si.angles);
	const float mask = ComputeMask(si.tc0, si.color.a);

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, tc);
	const float4 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, tc);
	const float4 rawGBuffer2 = g_texGBuffer2.Sample(g_samGBuffer2, tc);

	// GBuffer0:
	const float3 realAlbedo = ToRealAlbedo(rawGBuffer0.rgb);
	const float specMask = rawGBuffer0.a;

	// GBuffer1:
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float3 normal = mul(normalWV, (float3x3)g_ubSimpleForestFS._matInvV);
	const float alpha = rawGBuffer1.a;

	// GBuffer2:
	const float2 lamScaleBias = DS_GetLamScaleBias(rawGBuffer2);
	const float gloss64 = rawGBuffer2.a * 64.f;
	const float gloss4K = gloss64 * gloss64;

	// <Shadow>
	float shadowMask;
	{
		float4 shadowConfig = g_ubSimpleForestFS._shadowConfig;
		const float lamBiasMask = saturate(lamScaleBias.y * shadowConfig.y);
		shadowConfig.y = 1.f - lamBiasMask; // Keep penumbra blurry.
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, g_ubSimpleForestFS._dirToSun.xyz, depth);
		shadowMask = SimpleShadowMapCSM(
			g_texShadowCmp,
			g_samShadowCmp,
			si.posW_depth.xyz,
			posForShadow,
			g_ubSimpleForestFS._matShadow,
			g_ubSimpleForestFS._matShadowCSM1,
			g_ubSimpleForestFS._matShadowCSM2,
			g_ubSimpleForestFS._matShadowCSM3,
			g_ubSimpleForestFS._matScreenCSM,
			g_ubSimpleForestFS._csmSplitRanges,
			shadowConfig);
	}
	// </Shadow>

	const float4 litRet = VerusLit(g_ubSimpleForestFS._dirToSun.xyz, normal, dirToEye,
		gloss4K,
		lamScaleBias,
		float4(0, 0, 1, 0));

	const float3 diffColor = litRet.y * g_ubSimpleForestFS._sunColor.rgb * shadowMask + g_ubSimpleForestFS._ambientColor.rgb;
	const float3 specColor = litRet.z * g_ubSimpleForestFS._sunColor.rgb * shadowMask * specMask;

	so.color.rgb = realAlbedo * diffColor + specColor;
	so.color.a = 1.f;

	const float fog = ComputeFog(depth, g_ubSimpleForestFS._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimpleForestFS._fogColor.rgb, fog);

	so.color.rgb = ToSafeHDR(so.color.rgb);

	clip(alpha * mask * step(0.f, si.clipDistance) - 0.53f);

	return so;
}
#endif

//@main:# (VGF)
