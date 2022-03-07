// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibTessellation.hlsl"
#include "DS_Terrain.inc.hlsl"

#define DETAIL_TC_SCALE 8.0

ConstantBuffer<UB_TerrainVS> g_ubTerrainVS : register(b0, space0);
ConstantBuffer<UB_TerrainFS> g_ubTerrainFS : register(b0, space1);

Texture2D      g_texHeightVS  : register(t1, space0);
SamplerState   g_samHeightVS  : register(s1, space0);
Texture2D      g_texNormalVS  : register(t2, space0);
SamplerState   g_samNormalVS  : register(s2, space0);

Texture2D      g_texNormal    : register(t1, space1);
SamplerState   g_samNormal    : register(s1, space1);
Texture2D      g_texBlend     : register(t2, space1);
SamplerState   g_samBlend     : register(s2, space1);
Texture2DArray g_texLayers    : register(t3, space1);
SamplerState   g_samLayers    : register(s3, space1);
Texture2DArray g_texLayersNM  : register(t4, space1);
SamplerState   g_samLayersNM  : register(s4, space1);
Texture2D      g_texDetail    : register(t5, space1);
SamplerState   g_samDetail    : register(s5, space1);

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

static const float g_layerScale = 1.0 / 8.0;

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubTerrainVS._eyePos_mapSideInv.xyz;
	const float mapSideInv = g_ubTerrainVS._eyePos_mapSideInv.w;

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

	so.pos = MulTessPos(float4(pos, 1), g_ubTerrainVS._matV, g_ubTerrainVS._matVP);
	so.nrmWV = mul(inNrm, (float3x3)g_ubTerrainVS._matWV);
#if !defined(DEF_DEPTH)
	so.layerForChannel = si.layerForChannel;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	so.tcBlend.xy = posBlend * mapSideInv + 0.5;
	so.tcBlend.z = pos.y;
	so.tcLayer_tcMap.xy = pos.xz * g_layerScale;
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * mapSideInv + 0.5; // Texel's center.
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

#ifdef DEF_SOLID_COLOR
	DS_SolidColor(so, si.layerForChannel.rgb);
#else
	const float3 rand = Rand(si.pos.xy);

	const float4 layerForChannel = round(si.layerForChannel);
	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float4 rawBlend = g_texBlend.Sample(g_samBlend, si.tcBlend.xy);
	float4 weights = float4(rawBlend.rgb, 1.0 - dot(rawBlend.rgb, float3(1, 1, 1)));

	// <Basis>
	float3 basisTan, basisBin, basisNrm;
	{
		const float4 rawBasis = g_texNormal.Sample(g_samNormal, tcMap);
		const float4 basis = rawBasis * 2.0 - 1.0;
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
	float4 albedo;
	float specMask;
	float detailStrength;
	{
		const float4 rawAlbedos[4] =
		{
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.r)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.g)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.b)),
			g_texLayers.Sample(g_samLayers, float3(tcLayer, layerForChannel.a))
		};
		const float4 mask = saturate((float4(
			Grayscale(rawAlbedos[0].rgb),
			Grayscale(rawAlbedos[1].rgb),
			Grayscale(rawAlbedos[2].rgb),
			Grayscale(rawAlbedos[3].rgb)) - 0.25) * 8.0 + 0.25);
		weights = saturate(weights + weights * mask);
		const float weightsSum = dot(weights, 1.0);
		weights /= weightsSum;
		float4 accAlbedo = 0.0;
		accAlbedo += rawAlbedos[0] * weights.r;
		accAlbedo += rawAlbedos[1] * weights.g;
		accAlbedo += rawAlbedos[2] * weights.b;
		accAlbedo += rawAlbedos[3] * weights.a;
		albedo = accAlbedo;

		static float vSpecStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubTerrainFS._vSpecStrength;
		static float vDetailStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubTerrainFS._vDetailStrength;
		const float4 specStrengthForChannel = float4(
			vSpecStrength[layerForChannel.r],
			vSpecStrength[layerForChannel.g],
			vSpecStrength[layerForChannel.b],
			vSpecStrength[layerForChannel.a]);
		const float4 detailStrengthForChannel = float4(
			vDetailStrength[layerForChannel.r],
			vDetailStrength[layerForChannel.g],
			vDetailStrength[layerForChannel.b],
			vDetailStrength[layerForChannel.a]);
		specMask = albedo.a * dot(specStrengthForChannel, weights);
		detailStrength = dot(detailStrengthForChannel, weights);
	}
	// </Albedo>

	// <Water>
	float waterGlossBoost;
	{
		const float dryMask = saturate(si.tcBlend.z);
		const float dryMask3 = dryMask * dryMask * dryMask;
		const float wetMask = 1.0 - dryMask;
		const float wetMask3 = wetMask * wetMask * wetMask;
		albedo.rgb *= dryMask3 * 0.5 + 0.5;
		specMask = dryMask * saturate(specMask + wetMask3 * wetMask3 * 0.1);
		waterGlossBoost = min(32.0, dryMask * wetMask3 * 100.0);
	}
	// </Water>

	const float gloss = lerp(3.3, 15.0, specMask) + waterGlossBoost;

	// <Normal>
	float3 normalWV;
	float toksvigFactor;
	{
		float4 accNormal = 0.0;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.r)) * weights.r;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.g)) * weights.g;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.b)) * weights.b;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.a)) * weights.a;
		accNormal = lerp(accNormal, float4(0, 0.5, 0.5, 0.5), 0.5);
		const float4 normalAA = NormalMapAA(accNormal);
		normalWV = normalize(mul(normalAA.xyz, matFromTBN));
		toksvigFactor = ComputeToksvigFactor(normalAA.a, gloss);
	}
	// </Normal>

	// <Detail>
	{
		const float3 rawDetail = g_texDetail.Sample(g_samDetail, tcLayer * DETAIL_TC_SCALE).rgb;
		albedo.rgb = albedo.rgb * lerp(0.5, rawDetail, detailStrength) * 2.0;
	}
	// </Detail>

	// <SpecFresnel>
	float specMaskWithFresnel;
	{
		const float fresnelMask = pow(saturate(1.0 - normalWV.z), 5.0);
		const float maxSpecAdd = (1.0 - specMask) * (0.04 + 0.2 * specMask);
		specMaskWithFresnel = FresnelSchlick(specMask, maxSpecAdd, fresnelMask);
	}
	// </SpecFresnel>

	{
		DS_Reset(so);

		DS_SetAlbedo(so, albedo.rgb);
		DS_SetSpecMask(so, specMaskWithFresnel);

		DS_SetNormal(so, normalWV + NormalDither(rand));
		DS_SetEmission(so, 0.0, 0.0);
		DS_SetMotionBlurMask(so, 1.0);

		DS_SetLamScaleBias(so, g_ubTerrainFS._lamScaleBias.xy, float4(0, 0, 1, 0));
		DS_SetMetallicity(so, 0.05, 0.0);
		DS_SetGloss(so, gloss * toksvigFactor);
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
