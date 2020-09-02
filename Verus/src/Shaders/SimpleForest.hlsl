// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

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

	const float pointSpriteSize = si.tc0.x / 500.0;
	const float angle = si.tc0.y / 32767.0;

	const float3 toEye = g_ubSimpleForestVS._eyePos.xyz - si.pos.xyz;
	const float distToScreen = length(g_ubSimpleForestVS._eyePosScreen.xyz - si.pos.xyz);

	const float farAlpha = 1.0 - saturate((distToScreen - 900.0) * (1.0 / 100.0));

	so.pos = mul(si.pos, g_ubSimpleForestVS._matWVP);
	so.tc0 = 0.0;
	so.dirToEye = toEye;
	so.posW_depth = float4(si.pos.xyz, so.pos.z);
	so.color.rgb = RandomColor(si.pos.xz, 0.3, 0.2);
	so.color.a = farAlpha;
	so.psize = (pointSpriteSize * (g_ubSimpleForestVS._viewportSize.yx * g_ubSimpleForestVS._viewportSize.z)).xyxy;
	so.psize.xy *= g_ubSimpleForestVS._pointSpriteScale.xy * g_ubSimpleForestVS._matP._m11;
	so.clipDistance = 0.0;

	float2 param0 = toEye.xy;
	float2 param1 = toEye.zz;
	param0.y = max(0.0, param0.y); // Only upper hemisphere.
	param1.y = length(toEye.xz); // Distance in XZ-plane.
	so.angles.xy = (atan2(param0, param1) + _PI) * (0.5 / _PI); // atan2(x, z) and atan2(max(0.0, y), length(toEye.xz)). From 0 to 1.
	so.angles.y = (so.angles.y - 0.5) * 4.0; // Choose this quadrant.
	so.angles.xy = saturate(so.angles.xy);
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
		so.tc0.xy = _POINT_SPRITE_TEX_COORDS[i];
		so.posW_depth.xy = centerW + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.zw;
		so.clipDistance = so.posW_depth.y;
		stream.Append(so);
	}
}
#endif

float2 ComputeTexCoords(float2 tc, float2 angles)
{
	const float marginBias = 16.0 / 512.0;
	const float marginScale = 1.0 - marginBias * 2.0;
	const float2 tcMargin = tc * marginScale + marginBias;

	const float2 frameCount = float2(16, 16);
	const float2 frameScale = 1.0 / frameCount;
	const float2 frameBias = floor(min(angles * frameCount + 0.5, float2(256, frameCount.y - 0.5)));
	return (tcMargin + frameBias) * frameScale;
}

float ComputeMask(float2 tc, float alpha)
{
	const float2 tcCenter = tc - 0.5;
	const float rad = saturate(dot(tcCenter, tcCenter) * 4.0);
	return saturate(rad + (alpha * 2.0 - 1.0));
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
	const float3 albedo = rawGBuffer0.rgb * 0.5;
	const float spec = rawGBuffer0.a;

	// GBuffer1:
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float3 normal = mul(normalWV, (float3x3)g_ubSimpleForestFS._matInvV);
	const float alpha = rawGBuffer1.a;

	// GBuffer2:
	const float2 lamScaleBias = DS_GetLamScaleBias(rawGBuffer2);
	const float gloss64 = rawGBuffer2.a * 64.0;
	const float gloss = gloss64 * gloss64;

	// <Shadow>
	float shadowMask;
	{
		float4 config = g_ubSimpleForestFS._shadowConfig;
		const float lamBiasMask = saturate(lamScaleBias.y * config.y);
		config.y = 1.0 - lamBiasMask; // Keep penumbra blurry.
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, g_ubSimpleForestFS._dirToSun.xyz, depth);
		const float4 tcShadow = ShadowCoords(float4(posForShadow, 1), g_ubSimpleForestFS._matSunShadow, depth);
		shadowMask = SimpleShadowMapCSM(
			g_texShadowCmp,
			g_samShadowCmp,
			tcShadow,
			config,
			g_ubSimpleForestFS._splitRanges,
			g_ubSimpleForestFS._matSunShadow,
			g_ubSimpleForestFS._matSunShadowCSM1,
			g_ubSimpleForestFS._matSunShadowCSM2,
			g_ubSimpleForestFS._matSunShadowCSM3);
	}
	// </Shadow>

	const float4 litRet = VerusLit(g_ubSimpleForestFS._dirToSun.xyz, normal, dirToEye,
		gloss,
		lamScaleBias,
		float4(0, 0, 1, 0));

	const float3 diffColor = litRet.y * g_ubSimpleForestFS._sunColor.rgb * shadowMask + g_ubSimpleForestFS._ambientColor.rgb;
	const float3 specColor = litRet.z * g_ubSimpleForestFS._sunColor.rgb * shadowMask * spec;

	so.color.rgb = albedo * diffColor + specColor;
	so.color.a = 1.0;

	const float fog = ComputeFog(depth, g_ubSimpleForestFS._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimpleForestFS._fogColor.rgb, fog);

	clip(alpha * mask * step(0.0, si.clipDistance) - 0.53);

	return so;
}
#endif

//@main:# (VGF)
