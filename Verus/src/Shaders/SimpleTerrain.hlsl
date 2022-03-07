// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibSurface.hlsl"
#include "SimpleTerrain.inc.hlsl"

ConstantBuffer<UB_SimpleTerrainVS> g_ubSimpleTerrainVS : register(b0, space0);
ConstantBuffer<UB_SimpleTerrainFS> g_ubSimpleTerrainFS : register(b0, space1);

Texture2D              g_texHeightVS  : register(t1, space0);
SamplerState           g_samHeightVS  : register(s1, space0);
Texture2D              g_texNormalVS  : register(t2, space0);
SamplerState           g_samNormalVS  : register(s2, space0);

Texture2D              g_texNormal    : register(t1, space1);
SamplerState           g_samNormal    : register(s1, space1);
Texture2D              g_texBlend     : register(t2, space1);
SamplerState           g_samBlend     : register(s2, space1);
Texture2DArray         g_texLayers    : register(t3, space1);
SamplerState           g_samLayers    : register(s3, space1);
Texture2D              g_texShadowCmp : register(t4, space1);
SamplerComparisonState g_samShadowCmp : register(s4, space1);

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
	const float mapSideInv = g_ubSimpleTerrainVS._mapSideInv_clipDistanceOffset.x;
	const float clipDistanceOffset = g_ubSimpleTerrainVS._mapSideInv_clipDistanceOffset.y;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz;
	const float2 tcMap = pos.xz * mapSideInv + 0.5; // Range [0, 1).
	const float2 posBlend = pos.xz + edgeCorrection * 0.5;

	// <HeightAndNormal>
	float3 inNrm;
	{
		const float approxHeight = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + (0.5 * mapSideInv) * 16.0, 4.0).r);
		pos.y = approxHeight;

		const float distToEye = distance(pos, eyePos);
		const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
		const float geomipsLodFrac = frac(geomipsLod);
		const float geomipsLodBase = floor(geomipsLod);
		const float geomipsLodNext = geomipsLodBase + 1.0;
		const float2 texelCenterAB = (0.5 * mapSideInv) * exp2(float2(geomipsLodBase, geomipsLodNext));
		const float yA = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.xx, geomipsLodBase).r);
		const float yB = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.yy, geomipsLodNext).r);
		pos.y = lerp(yA, yB, geomipsLodFrac);

		const float4 rawNormal = g_texNormalVS.SampleLevel(g_samNormalVS, tcMap + texelCenterAB.xx, geomipsLodBase);
		inNrm = float3(rawNormal.x, 0, rawNormal.y) * 2.0 - 1.0;
		inNrm.y = ComputeNormalZ(inNrm.xz);
	}
	// </HeightAndNormal>

	so.pos = mul(float4(pos, 1), g_ubSimpleTerrainVS._matVP);
	so.posW_depth = float4(pos, so.pos.z);
	so.nrmW = mul(inNrm, (float3x3)g_ubSimpleTerrainVS._matW);
	so.layerForChannel = si.layerForChannel;
	so.tcBlend = posBlend * mapSideInv + 0.5;
	so.tcLayer_tcMap.xy = pos.xz * g_layerScale;
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * mapSideInv + 0.5; // Texel's center.
	so.dirToEye = eyePos - pos;
	so.clipDistance = pos.y + clipDistanceOffset;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 layerForChannel = round(si.layerForChannel);
	const float2 tcLayer = si.tcLayer_tcMap.xy;

	const float4 rawBlend = g_texBlend.Sample(g_samBlend, si.tcBlend);
	float4 weights = float4(rawBlend.rgb, 1.0 - dot(rawBlend.rgb, float3(1, 1, 1)));

	// <Albedo>
	float4 albedo;
	float specMask;
	{
		const float4 rawAlbedos[4] =
		{
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.r)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.g)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.b)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.a))
		};
		float4 accAlbedo = 0.0;
		accAlbedo += rawAlbedos[0] * weights.r;
		accAlbedo += rawAlbedos[1] * weights.g;
		accAlbedo += rawAlbedos[2] * weights.b;
		accAlbedo += rawAlbedos[3] * weights.a;
		albedo = accAlbedo;

		static float vSpecStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubSimpleTerrainFS._vSpecStrength;
		const float4 specStrengthForChannel = float4(
			vSpecStrength[layerForChannel.r],
			vSpecStrength[layerForChannel.g],
			vSpecStrength[layerForChannel.b],
			vSpecStrength[layerForChannel.a]);
		specMask = albedo.a * dot(specStrengthForChannel, weights);
	}
	float3 realAlbedo = ToRealAlbedo(albedo.rgb);
	// </Albedo>

	// <Water>
	float waterGlossBoost;
	{
		const float dryMask = saturate(si.posW_depth.y);
		const float dryMask3 = dryMask * dryMask * dryMask;
		const float wetMask = 1.0 - dryMask;
		const float wetMask3 = wetMask * wetMask * wetMask;
		realAlbedo *= dryMask3 * 0.5 + 0.5;
		specMask = dryMask * saturate(specMask + wetMask3 * wetMask3 * 0.1);
		waterGlossBoost = min(32.0, dryMask * wetMask3 * 100.0);
	}
	// </Water>

	const float gloss64 = lerp(3.3, 15.0, specMask) + waterGlossBoost;
	const float gloss4K = gloss64 * gloss64;

	const float3 normal = normalize(si.nrmW);
	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	// <Shadow>
	float shadowMask;
	{
		float4 shadowConfig = g_ubSimpleTerrainFS._shadowConfig;
		const float lamBiasMask = saturate(g_ubSimpleTerrainFS._lamScaleBias.y * shadowConfig.y);
		shadowConfig.y = 1.0 - lamBiasMask; // Keep penumbra blurry.
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, g_ubSimpleTerrainFS._dirToSun.xyz, depth);
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
			shadowConfig);
	}
	// </Shadow>

	const float4 litRet = VerusLit(g_ubSimpleTerrainFS._dirToSun.xyz, normal, dirToEye,
		gloss4K,
		g_ubSimpleTerrainFS._lamScaleBias.xy,
		float4(0, 0, 1, 0));

	const float3 diffColor = litRet.y * g_ubSimpleTerrainFS._sunColor.rgb * shadowMask + g_ubSimpleTerrainFS._ambientColor.rgb;
	const float3 specColor = litRet.z * g_ubSimpleTerrainFS._sunColor.rgb * shadowMask * specMask;

	so.color.rgb = realAlbedo * diffColor + specColor;
	so.color.a = 1.0;

	const float fog = ComputeFog(depth, g_ubSimpleTerrainFS._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimpleTerrainFS._fogColor.rgb, fog);

	so.color.rgb = ToSafeHDR(so.color.rgb);

	return so;
}
#endif

//@main:#
