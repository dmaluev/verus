// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibTessellation.hlsl"
#include "DS_Terrain.inc.hlsl"

CBUFFER(0, UB_TerrainVS, g_ubTerrainVS)
CBUFFER(1, UB_TerrainFS, g_ubTerrainFS)

Texture2D      g_texHeightVS : REG(t1, space0, t0);
SamplerState   g_samHeightVS : REG(s1, space0, s0);
Texture2D      g_texNormalVS : REG(t2, space0, t1);
SamplerState   g_samNormalVS : REG(s2, space0, s1);

Texture2D      g_texNormal   : REG(t1, space1, t2);
SamplerState   g_samNormal   : REG(s1, space1, s2);
Texture2D      g_texBlend    : REG(t2, space1, t3);
SamplerState   g_samBlend    : REG(s2, space1, s3);
Texture2DArray g_texLayers   : REG(t3, space1, t4);
SamplerState   g_samLayers   : REG(s3, space1, s4);
Texture2DArray g_texLayersN  : REG(t4, space1, t5);
SamplerState   g_samLayersN  : REG(s4, space1, s5);
Texture2DArray g_texLayersX  : REG(t5, space1, t6);
SamplerState   g_samLayersX  : REG(s5, space1, s6);
Texture2D      g_texDetail   : REG(t6, space1, t7);
SamplerState   g_samDetail   : REG(s6, space1, s7);
Texture2D      g_texDetailN  : REG(t7, space1, t8);
SamplerState   g_samDetailN  : REG(s7, space1, s8);
Texture2D      g_texStrass   : REG(t8, space1, t9);
SamplerState   g_samStrass   : REG(s8, space1, s9);

struct VSI
{
	VK_LOCATION_POSITION int4 pos             : POSITION;
	VK_LOCATION(16)      int4 posPatch        : INSTDATA0;
	VK_LOCATION(17)      int4 layerForChannel : INSTDATA1;
};

struct VSO
{
	float4 pos             : SV_Position;
	float3 nrmWV           : TEXCOORD0;
#if !defined(DEF_DEPTH)
	float4 layerForChannel : TEXCOORD1;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	float3 tcBlend         : TEXCOORD2;
	float4 tcLayer_tcMap   : TEXCOORD3;
#endif
#endif
};

static const float g_detailScale = 8.0;
static const float g_strassScale = 16.0;
static const float g_layerScale = 1.0 / 8.0;

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 headPos = g_ubTerrainVS._headPos_invMapSide.xyz;
	const float invMapSide = g_ubTerrainVS._headPos_invMapSide.w;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz;
	const float2 tcMap = pos.xz * invMapSide + 0.5; // Range [0, 1).
	const float2 posBlend = pos.xz + edgeCorrection * 0.5;

	// <HeightAndNormal>
	float3 inNrm;
	{
		pos.y = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + (8.0 * invMapSide), 4.0).r);
		const float distToHead = distance(pos, headPos);
		const float geomipsLod = log2(clamp(distToHead * (2.0 / 100.0), 1.0, 18.0));
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

	so.pos = MulTessPos(float4(pos, 1), g_ubTerrainVS._matV, g_ubTerrainVS._matVP);
	so.nrmWV = mul(inNrm, (float3x3)g_ubTerrainVS._matWV);
#if !defined(DEF_DEPTH)
	so.layerForChannel = si.layerForChannel;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	so.tcBlend.xy = posBlend * invMapSide + 0.5;
	so.tcBlend.z = pos.y;
	so.tcLayer_tcMap.xy = pos.xz * g_layerScale;
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * invMapSide + 0.5; // Texel's center.
#endif
#endif

	return so;
}
#endif

_HSO_STRUCT;

#ifdef _HS
PCFO PatchConstFunc(const OutputPatch<HSO, 3> outputPatch)
{
	PCFO so;
	_HS_PCF_BODY(g_ubTerrainVS._matP);
	return so;
}

[domain("tri")]
[maxtessfactor(7.0)]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[partitioning(_PARTITION_METHOD)]
[patchconstantfunc("PatchConstFunc")]
HSO mainHS(InputPatch<VSO, 3> inputPatch, uint id : SV_OutputControlPointID)
{
	HSO so;

	_HS_PN_BODY(nrmWV, g_ubTerrainVS._matP, g_ubTerrainVS._viewportSize);

	_HS_COPY(pos);
	_HS_COPY(nrmWV);
#if !defined(DEF_DEPTH)
	_HS_COPY(layerForChannel);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_HS_COPY(tcBlend);
	_HS_COPY(tcLayer_tcMap);
#endif
#endif

	return so;
}
#endif

#ifdef _DS
[domain("tri")]
VSO mainDS(_IN_DS)
{
	VSO so;

	_DS_INIT_FLAT_POS;
	_DS_INIT_SMOOTH_POS;

	so.pos = ApplyProjection(smoothPosWV, g_ubTerrainVS._matP);
	_DS_COPY(nrmWV);
#if !defined(DEF_DEPTH)
	_DS_COPY(layerForChannel);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_DS_COPY(tcBlend);
	_DS_COPY(tcLayer_tcMap);
#endif
#endif

#ifndef DEF_DEPTH
	// Fade to non-tess mesh at 80 meters. LOD 1 starts at 100 meters.
	const float tessStrength = saturate((1.0 - saturate(smoothPosWV.z / -80.0)) * 4.0);
	const float3 posWV = lerp(flatPosWV, smoothPosWV, tessStrength);
	so.pos = ApplyProjection(posWV, g_ubTerrainVS._matP);
#endif

	return so;
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;
	DS_Reset(so);

#ifdef DEF_SOLID_COLOR
	DS_SolidColor(so, si.layerForChannel.rgb);
#else
	// Fix interpolation errors by rounding:
	si.layerForChannel = round(si.layerForChannel);
	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float4 blendSam = g_texBlend.Sample(g_samBlend, si.tcBlend.xy);
	const float4 weights = float4(blendSam.rgb, 1.0 - dot(blendSam.rgb, 1.0));

	const float4 texEnabled = ceil(weights);
	const float3 tcLayerR = float3(tcLayer * texEnabled.r, si.layerForChannel.r);
	const float3 tcLayerG = float3(tcLayer * texEnabled.g, si.layerForChannel.g);
	const float3 tcLayerB = float3(tcLayer * texEnabled.b, si.layerForChannel.b);
	const float3 tcLayerA = float3(tcLayer * texEnabled.a, si.layerForChannel.a);

	// <Basis>
	float3 basisTan, basisBin, basisNrm;
	{
		const float4 basisSam = g_texNormal.Sample(g_samNormal, tcMap);
		const float4 basis = basisSam * 2.0 - 1.0;
		basisNrm = float3(basis.x, 0, basis.y);
		basisTan = float3(0, basis.z, basis.w);
		basisNrm.y = ComputeNormalZ(basisNrm.xz);
		basisTan.x = ComputeNormalZ(basisTan.yz);
		basisBin = cross(basisTan, basisNrm);
	}
	_TBN_SPACE(
		mul(basisTan, (float3x3)g_ubTerrainFS._matWV),
		mul(basisBin, (float3x3)g_ubTerrainFS._matWV),
		mul(basisNrm, (float3x3)g_ubTerrainFS._matWV));
	// </Basis>

	// <Albedo>
	float3 albedo;
	float detailStrength;
	float roughStrength;
	{
		float3 accAlbedo = 0.0;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerR).rgb * weights.r;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerG).rgb * weights.g;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerB).rgb * weights.b;
		accAlbedo += g_texLayers.Sample(g_samLayers, tcLayerA).rgb * weights.a;
		albedo = accAlbedo;

		static float vDetailStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubTerrainFS._vDetailStrength;
		static float vRoughStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubTerrainFS._vRoughStrength;
		const float4 detailStrengthForChannel = float4(
			vDetailStrength[si.layerForChannel.r],
			vDetailStrength[si.layerForChannel.g],
			vDetailStrength[si.layerForChannel.b],
			vDetailStrength[si.layerForChannel.a]);
		const float4 roughStrengthForChannel = float4(
			vRoughStrength[si.layerForChannel.r],
			vRoughStrength[si.layerForChannel.g],
			vRoughStrength[si.layerForChannel.b],
			vRoughStrength[si.layerForChannel.a]);
		detailStrength = dot(detailStrengthForChannel, weights);
		roughStrength = dot(roughStrengthForChannel, weights);
	}
	// </Albedo>

	// <Detail>
	float3 detailSam;
	float3 detailNSam;
	{
		detailSam = g_texDetail.Sample(g_samDetail, tcLayer * g_detailScale).rgb;
		detailNSam = g_texDetailN.Sample(g_samDetailN, tcLayer * g_detailScale).rgb;
	}
	albedo = saturate(albedo * lerp(0.5, detailSam, detailStrength) * 2.0);
	// </Detail>

	// <Water>
	float waterRoughnessScale;
	float waterRoughnessMin;
	{
		const float dryMask = saturate(si.tcBlend.z);
		const float dryMask3 = dryMask * dryMask * dryMask;
		const float wetMask = 1.0 - dryMask;
		const float wetMask3 = wetMask * wetMask * wetMask;
		albedo *= 0.4 + 0.6 * dryMask3;
		waterRoughnessScale = 1.0 - 0.8 * saturate(dryMask * wetMask3 * 16.0);
		waterRoughnessMin = wetMask3;
	}
	// </Water>

	// <Normal>
	float3 normalWV;
	float3 tangentWV;
	{
		float4 accNormal = 0.0;
		accNormal += g_texLayersN.Sample(g_samLayersN, tcLayerR) * weights.r;
		accNormal += g_texLayersN.Sample(g_samLayersN, tcLayerG) * weights.g;
		accNormal += g_texLayersN.Sample(g_samLayersN, tcLayerB) * weights.b;
		accNormal += g_texLayersN.Sample(g_samLayersN, tcLayerA) * weights.a;

		accNormal.rg = saturate(accNormal.rg * lerp(0.5, detailNSam.rg, detailStrength) * 2.0);

		const float3 normalTBN = NormalMapFromBC5(accNormal);
		normalWV = normalize(mul(normalTBN, matFromTBN));
		tangentWV = normalize(mul(cross(normalTBN, cross(float3(0, 1, 0), normalTBN)), matFromTBN));
	}
	// </Normal>

	// <X>
	float occlusion;
	float roughness;
	float metallic;
	float emission;
	float wrapDiffuse;
	float anisoSpec;
	{
		float4 accX = 0.0;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerR) * weights.r;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerG) * weights.g;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerB) * weights.b;
		accX += g_texLayersX.Sample(g_samLayersX, tcLayerA) * weights.a;

		occlusion = accX.r;
		roughness = accX.g;
		const float2 metallic_wrapDiffuse = CleanMutexChannels(SeparateMutexChannels(accX.b));
		const float2 emission_anisoSpec = CleanMutexChannels(SeparateMutexChannels(accX.a));
		metallic = metallic_wrapDiffuse.r;
		emission = emission_anisoSpec.r * 10000.0;
		wrapDiffuse = metallic_wrapDiffuse.g;
		anisoSpec = emission_anisoSpec.g;
	}
	// </X>

	// <Rim>
	{
		const float rim = saturate(1.0 - normalWV.z);
		const float rimSq = rim * rim;
		roughness = saturate(roughness + rimSq * 3.0 * wrapDiffuse);
	}
	// </Rim>

	// <Strass>
	{
		const float strass = g_texStrass.Sample(g_samStrass, tcLayer * g_strassScale).r;
		roughness *= 1.0 - clamp(strass * roughStrength * 4.0, 0.0, 0.99);
	}
	// </Strass>

	roughness = max(roughness * waterRoughnessScale, waterRoughnessMin);

	{
		DS_SetAlbedo(so, albedo);
		DS_SetSSSHue(so, 0.5);

		DS_SetNormal(so, normalWV);
		DS_SetEmission(so, emission);
		DS_SetMotionBlur(so, 1.0);

		DS_SetOcclusion(so, occlusion);
		DS_SetRoughness(so, roughness);
		DS_SetMetallic(so, metallic);
		DS_SetWrapDiffuse(so, wrapDiffuse);

		DS_SetTangent(so, tangentWV);
		DS_SetAnisoSpec(so, anisoSpec);
		DS_SetRoughDiffuse(so, roughStrength);
	}
#endif

	return so;
}
#endif

//@main:#
//@main:#Tess TESS (VHDF)

//@main:#Depth     DEPTH (V)
//@main:#DepthTess DEPTH TESS (VHD)

//@main:#SolidColor SOLID_COLOR
