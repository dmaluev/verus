// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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
	VK_LOCATION(8)       float2 tc0 : TEXCOORD0;
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
	const float3 mainCameraEyePos = g_ubSimpleForestVS._mainCameraEyePos_pointSpriteFlipY.xyz;
	const float pointSpriteFlipY = g_ubSimpleForestVS._mainCameraEyePos_pointSpriteFlipY.w;

	const float pointSpriteSize = si.tc0.x;
	const float angle = si.tc0.y;

	const float3 toEye = eyePos - si.pos.xyz;
	const float distToMainCamera = distance(mainCameraEyePos, si.pos.xyz);

	const float nearAlpha = ComputeFade(distToMainCamera, 6.0, 9.0);
	const float farAlpha = 1.0 - ComputeFade(distToMainCamera, 900.0, 1000.0);

	so.pos = mul(si.pos, g_ubSimpleForestVS._matWVP);
	so.tc0 = 0.0;
	so.dirToEye = toEye;
	so.posW_depth = float4(si.pos.xyz, so.pos.w);
	so.color.rgb = RandomColor(si.pos.xz, 0.25, 0.15);
	so.color.a = nearAlpha * farAlpha;
	so.psize = (pointSpriteSize * (g_ubSimpleForestVS._viewportSize.yx * g_ubSimpleForestVS._viewportSize.z)).xyxy;
	so.psize.xy *= float2(1, pointSpriteFlipY) * g_ubSimpleForestVS._matP._m11;
	so.clipDistance = clipDistanceOffset;

	float2 paramA = toEye.xy;
	float2 paramB = toEye.zz;
	paramA.y = max(0.0, paramA.y); // Only upper hemisphere.
	paramB.y = length(toEye.xz); // Distance in XZ-plane.
	so.angles = (atan2(paramA, paramB) + _PI) * (0.5 / _PI); // atan2(x, z) and atan2(max(0, y), length(toEye.xz)). From 0 to 1.
	so.angles.y = (so.angles.y - 0.5) * 4.0; // Choose this quadrant.
	so.angles = saturate(so.angles);
	so.angles.x = frac(so.angles.x - angle + 0.5); // Turn.

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
		so.tc0 = _POINT_SPRITE_TEX_COORDS[i];
		so.posW_depth.xy = centerW + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.zw;
		so.clipDistance += so.posW_depth.y;
		stream.Append(so);
	}
}
#endif

float2 ComputeTexCoords(float2 tc, float2 angles)
{
	const float margin = 4.0 / 512.0;
	const float tcScale = 1.0 - margin * 2.0;
	const float2 tcWithMargin = tc * tcScale + margin;

	const float2 frameCount = float2(16, 16);
	const float2 frameScale = 1.0 / frameCount;
	const float2 frameBias = floor(min(angles * frameCount + 0.5, float2(256, frameCount.y - 0.5)));
	return (tcWithMargin + frameBias) * frameScale;
}

float ComputeMask(float2 tc, float alpha)
{
	const float2 tcCenter = tc - 0.5;
	const float grad = 1.0 - saturate(dot(tcCenter, tcCenter) * 4.0);
	return saturate(grad + (alpha * 2.0 - 1.0));
}

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 normalFlip = g_ubSimpleForestFS._normalFlip.xyz;
	const float2 tcFlip = g_ubSimpleForestFS._tcFlip.xy;

	si.tc0 = frac(si.tc0 * tcFlip);

	const float3 dirToLight = g_ubSimpleForestFS._dirToSun.xyz;
	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	const float2 tc = ComputeTexCoords(si.tc0, si.angles);
	const float mask = ComputeMask(si.tc0, si.color.a);

	// <SampleSurfaceData>
	// GBuffer0:
	const float4 gBuffer0Sam = g_texGBuffer0.Sample(g_samGBuffer0, tc);
	const float3 albedo = saturate(gBuffer0Sam.rgb * si.color.rgb * 2.0);

	// GBuffer1:
	const float4 gBuffer1Sam = g_texGBuffer1.Sample(g_samGBuffer1, tc);
	const float3 normalWV = DS_GetNormal(gBuffer1Sam) * normalFlip;
	const float3 normal = mul(normalWV, (float3x3)g_ubSimpleForestFS._matInvV);
	const float alpha = gBuffer1Sam.a;

	// GBuffer2:
	const float4 gBuffer2Sam = g_texGBuffer2.Sample(g_samGBuffer2, tc);
	const float occlusion = gBuffer2Sam.r;
	const float roughness = gBuffer2Sam.g;
	const float metallic = gBuffer2Sam.b;
	// </SampleSurfaceData>

	// <Shadow>
	float shadowMask;
	{
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, dirToLight, depth);
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
			g_ubSimpleForestFS._shadowConfig);
	}
	// </Shadow>

	const float ambientOcclusion = 1.0 - 0.9 * si.tc0.y;

	float3 punctualDiff, punctualSpec;
	VerusSimpleLit(normal, dirToLight, dirToEye,
		albedo,
		roughness, metallic,
		punctualDiff, punctualSpec);

	punctualDiff *= g_ubSimpleForestFS._sunColor.rgb * shadowMask;
	punctualSpec *= g_ubSimpleForestFS._sunColor.rgb * shadowMask;

	so.color.rgb = (albedo * (g_ubSimpleForestFS._ambientColor.rgb * ambientOcclusion + punctualDiff) + punctualSpec) * occlusion;
	so.color.a = 1.0;

	const float fog = ComputeFog(depth, g_ubSimpleForestFS._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimpleForestFS._fogColor.rgb, fog);

	so.color.rgb = SaturateHDR(so.color.rgb);

	clip(alpha * mask * step(0.0, si.clipDistance) - 0.5);

	return so;
}
#endif

//@main:# (VGF)
