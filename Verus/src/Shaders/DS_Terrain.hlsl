// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_Terrain.inc.hlsl"

#define HEIGHT_SCALE 0.01

ConstantBuffer<UB_DrawDepth>     g_ubDrawDepth     : register(b0, space0);
ConstantBuffer<UB_PerMaterialFS> g_ubPerMaterialFS : register(b0, space1);

Texture2D      g_texHeight    : register(t1, space0);
SamplerState   g_samHeight    : register(s1, space0);
Texture2D      g_texNormal    : register(t1, space1);
SamplerState   g_samNormal    : register(s1, space1);
Texture2D      g_texBlend     : register(t2, space1);
SamplerState   g_samBlend     : register(s2, space1);
Texture2DArray g_texLayers    : register(t3, space1);
SamplerState   g_samLayers    : register(s3, space1);
Texture2DArray g_texLayersNM  : register(t4, space1);
SamplerState   g_samLayersNM  : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION int4 pos : POSITION;

	VK_LOCATION(16)      int4 posPatch        : TEXCOORD8;
	VK_LOCATION(17)      int4 layerForChannel : TEXCOORD9;
};

struct VSO
{
	float4 pos             : SV_Position;
	float4 layerForChannel : TEXCOORD0;
#ifndef DEF_DEPTH
	float3 tcBlend         : TEXCOORD1;
	float4 tcLayer_tcMap   : TEXCOORD2;
#endif
	float2 depth           : TEXCOORD3;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 posEye = g_ubDrawDepth._posEye_mapSideInv.xyz;
	const float mapSideInv = g_ubDrawDepth._posEye_mapSideInv.w;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz * float3(1, HEIGHT_SCALE, 1);
	const float2 posBlend = pos.xz + edgeCorrection * 0.5;

	const float bestPrecision = 50.0;
	const float2 tcMap = pos.xz * mapSideInv + 0.5; // Range [0, 1).
	const float distToEye = distance(pos, posEye);
	const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
	const float geomipsLodFrac = frac(geomipsLod);
	const float geomipsLodBase = floor(geomipsLod);
	const float geomipsLodNext = geomipsLodBase + 1.0;
	const float2 halfTexelAB = (0.5 * mapSideInv) * exp2(float2(geomipsLodBase, geomipsLodNext));
	const float yA = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.xx, geomipsLodBase).r + bestPrecision;
	const float yB = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.yy, geomipsLodNext).r + bestPrecision;
	pos.y = lerp(yA, yB, geomipsLodFrac);

	so.pos = mul(float4(pos, 1), g_ubDrawDepth._matVP);
	so.layerForChannel = si.layerForChannel;
#ifndef DEF_DEPTH
	so.tcBlend.xy = posBlend * mapSideInv + 0.5;
	so.tcBlend.z = pos.y + 0.75;
	so.tcLayer_tcMap.xy = pos.xz * (1.0 / 8.0);
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * mapSideInv + 0.5; // Texel's center.
#endif
	so.depth = so.pos.zw;

	return so;
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float3 rand = Rand(si.pos.xy);

	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float gloss = 4.0;

	const float4 layerForChannel = round(si.layerForChannel);

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
		mul(basisTan, (float3x3)g_ubDrawDepth._matWV),
		mul(basisBin, (float3x3)g_ubDrawDepth._matWV),
		mul(basisNrm, (float3x3)g_ubDrawDepth._matWV));
	// </Basis>

	// <Albedo>
	float4 albedo;
	float specStrength;
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

		static float vSpecStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubPerMaterialFS._vSpecStrength;
		static float vDetailStrength[_MAX_TERRAIN_LAYERS] = (float[_MAX_TERRAIN_LAYERS])g_ubPerMaterialFS._vDetailStrength;
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
		specStrength = dot(specStrengthForChannel, weights) * albedo.a;
		detailStrength = dot(detailStrengthForChannel, weights);
	}
	// </Albedo>

	// <Normal>
	float3 normalWV;
	float4 normalAA;
	{
		float4 accNormal = 0.0;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.r)) * weights.r;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.g)) * weights.g;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.b)) * weights.b;
		accNormal += g_texLayersNM.Sample(g_samLayersNM, float3(tcLayer, layerForChannel.a)) * weights.a;
		normalAA = NormalMapAA(accNormal);
		normalWV = normalize(mul(normalAA.xyz, matFromTBN));
	}
	// </Normal>

	DS_Test(so, 0.0, 0.5, 16.0);
	DS_SetAlbedo(so, albedo.rgb);
	DS_SetDepth(so, si.depth.x / si.depth.y);
	DS_SetNormal(so, normalWV + NormalDither(rand));
	DS_SetSpec(so, normalAA.w * specStrength);
	DS_SetGloss(so, normalAA.w * gloss);

	return so;
}
#endif

//@main:#
//@main:#Depth DEPTH (V)
