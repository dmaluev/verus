// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibSurface.hlsl"
#include "SimpleTerrain.inc.hlsl"

CBUFFER(0, UB_SimpleTerrainVS, g_ubSimpleTerrainVS)
CBUFFER(1, UB_SimpleTerrainFS, g_ubSimpleTerrainFS)

Texture2D              g_texHeightVS  : REG(t1, space0, t0);
SamplerState           g_samHeightVS  : REG(s1, space0, s0);
Texture2D              g_texNormalVS  : REG(t2, space0, t1);
SamplerState           g_samNormalVS  : REG(s2, space0, s1);

Texture2D              g_texBlend     : REG(t1, space1, t2);
SamplerState           g_samBlend     : REG(s1, space1, s2);
Texture2DArray         g_texLayers    : REG(t2, space1, t3);
SamplerState           g_samLayers    : REG(s2, space1, s3);
Texture2DArray         g_texLayersX   : REG(t3, space1, t4);
SamplerState           g_samLayersX   : REG(s3, space1, s4);
Texture2D              g_texShadowCmp : REG(t4, space1, t5);
SamplerComparisonState g_samShadowCmp : REG(s4, space1, s5);

struct VSI
{
	VK_LOCATION_POSITION int4 pos             : POSITION;
	VK_LOCATION(16)      int4 posPatch        : INSTDATA0;
	VK_LOCATION(17)      int4 layerForChannel : INSTDATA1;
};

struct VSO
{
	float4 pos             : SV_Position;
	float4 posW_depth      : TEXCOORD0;
	float3 nrmW            : TEXCOORD1;
	float4 layerForChannel : TEXCOORD2;
	float2 tcBlend         : TEXCOORD3;
	float4 tcLayer_tcMap   : TEXCOORD4;
	float3 dirToEye        : TEXCOORD5;
	float clipDistance : SV_ClipDistance;
};

struct FSO
{
	float4 color : SV_Target0;
};

static const float g_layerScale = 1.0 / 8.0;

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubSimpleTerrainVS._eyePos.xyz;
	const float invMapSide = g_ubSimpleTerrainVS._invMapSide_clipDistanceOffset.x;
	const float clipDistanceOffset = g_ubSimpleTerrainVS._invMapSide_clipDistanceOffset.y;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz;
	const float2 tcMap = pos.xz * invMapSide + 0.5; // Range [0, 1).
	const float2 posBlend = pos.xz + edgeCorrection * 0.5;

	// <HeightAndNormal>
	float3 inNrm;
	{
		pos.y = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + (8.0 * invMapSide), 4.0).r);
		const float distToEye = distance(pos, eyePos);
		const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
		const float geomipsLodFrac = frac(geomipsLod);
		const float geomipsLodBase = floor(geomipsLod);
		const float geomipsLodNext = geomipsLodBase + 1.0;
		const float2 texelCenterAB = (0.5 * invMapSide) * exp2(float2(geomipsLodBase, geomipsLodNext));
		const float yA = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.xx, geomipsLodBase).r);
		const float yB = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.yy, geomipsLodNext).r);
		pos.y = lerp(yA, yB, geomipsLodFrac);

		const float4 normalSam = g_texNormalVS.SampleLevel(g_samNormalVS, tcMap + texelCenterAB.xx, geomipsLodBase);
		inNrm = float3(normalSam.x, 0, normalSam.y) * 2.0 - 1.0;
		inNrm.y = ComputeNormalZ(inNrm.xz);
	}
	// </HeightAndNormal>

	so.pos = mul(float4(pos, 1), g_ubSimpleTerrainVS._matVP);
	so.posW_depth = float4(pos, so.pos.w);
	so.nrmW = mul(inNrm, (float3x3)g_ubSimpleTerrainVS._matW);
	so.layerForChannel = si.layerForChannel;
	so.tcBlend = posBlend * invMapSide + 0.5;
	so.tcLayer_tcMap.xy = pos.xz * g_layerScale;
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * invMapSide + 0.5; // Texel's center.
	so.dirToEye = eyePos - pos;
	so.clipDistance = pos.y + clipDistanceOffset;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	// Fix interpolation errors by rounding:
	si.layerForChannel = round(si.layerForChannel);
	const float2 tcLayer = si.tcLayer_tcMap.xy;

	const float4 blendSam = g_texBlend.Sample(g_samBlend, si.tcBlend);
	const float4 weights = float4(blendSam.rgb, 1.0 - dot(blendSam.rgb, 1.0));

	const float4 texEnabled = ceil(weights);
	const float3 tcLayerR = float3(tcLayer * texEnabled.r, si.layerForChannel.r);
	const float3 tcLayerG = float3(tcLayer * texEnabled.g, si.layerForChannel.g);
	const float3 tcLayerB = float3(tcLayer * texEnabled.b, si.layerForChannel.b);
	const float3 tcLayerA = float3(tcLayer * texEnabled.a, si.layerForChannel.a);

	// <Albedo>
	float3 albedo;
	{
		float3 accAlbedo = 0.0;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerR).rgb * weights.r;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerG).rgb * weights.g;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerB).rgb * weights.b;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerA).rgb * weights.a;
		albedo = accAlbedo;
	}
	// </Albedo>

	// <Water>
	float waterRoughnessScale;
	float waterRoughnessMin;
	{
		const float dryMask = saturate(si.posW_depth.y);
		const float dryMask3 = dryMask * dryMask * dryMask;
		const float wetMask = 1.0 - dryMask;
		const float wetMask3 = wetMask * wetMask * wetMask;
		albedo *= 0.4 + 0.6 * dryMask3;
		waterRoughnessScale = 1.0 - 0.8 * saturate(dryMask * wetMask3 * 16.0);
		waterRoughnessMin = wetMask3;
	}
	// </Water>

	// <X>
	float occlusion;
	float roughness;
	float metallic;
	{
		float3 accX = 0.0;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerR).rgb * weights.r;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerG).rgb * weights.g;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerB).rgb * weights.b;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerA).rgb * weights.a;

		occlusion = accX.r;
		roughness = accX.g;
		const float2 metallic_wrapDiffuse = CleanMutexChannels(SeparateMutexChannels(accX.b));
		metallic = metallic_wrapDiffuse.r;
	}
	// </X>

	const float3 normal = normalize(si.nrmW);
	const float3 dirToLight = g_ubSimpleTerrainFS._dirToSun.xyz;
	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	// <Shadow>
	float shadowMask;
	{
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, dirToLight, depth);
		shadowMask = SimpleShadowMapCSM(
			g_texShadowCmp,
			g_samShadowCmp,
			si.posW_depth.xyz,
			posForShadow,
			g_ubSimpleTerrainFS._matShadow,
			g_ubSimpleTerrainFS._matShadowCSM1,
			g_ubSimpleTerrainFS._matShadowCSM2,
			g_ubSimpleTerrainFS._matShadowCSM3,
			g_ubSimpleTerrainFS._matScreenCSM,
			g_ubSimpleTerrainFS._csmSplitRanges,
			g_ubSimpleTerrainFS._shadowConfig);
	}
	// </Shadow>

	float3 punctualDiff, punctualSpec;
	VerusSimpleLit(normal, dirToLight, dirToEye,
		albedo,
		roughness, metallic,
		punctualDiff, punctualSpec);

	punctualDiff *= g_ubSimpleTerrainFS._sunColor.rgb * shadowMask;
	punctualSpec *= g_ubSimpleTerrainFS._sunColor.rgb * shadowMask;

	so.color.rgb = (albedo * (g_ubSimpleTerrainFS._ambientColor.rgb * blendSam.a + punctualDiff) + punctualSpec) * occlusion;
	so.color.a = 1.0;

	const float fog = ComputeFog(depth, g_ubSimpleTerrainFS._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimpleTerrainFS._fogColor.rgb, fog);

	so.color.rgb = SaturateHDR(so.color.rgb);

	return so;
}
#endif

//@main:#
