// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibLighting.hlsl"
#include "LibSurface.hlsl"
#include "LibTessellation.hlsl"
#include "SimpleTerrain.inc.hlsl"

ConstantBuffer<UB_SimpleTerrainVS> g_ubSimpleTerrainVS : register(b0, space0);
ConstantBuffer<UB_SimpleTerrainFS> g_ubSimpleTerrainFS : register(b0, space1);

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

struct VSI
{
	VK_LOCATION_POSITION int4 pos             : POSITION;
	VK_LOCATION(16)      int4 posPatch        : INSTDATA0;
	VK_LOCATION(17)      int4 layerForChannel : INSTDATA1;
};

struct VSO
{
	float4 pos             : SV_Position;
	float3 nrmW            : TEXCOORD0;
	float4 layerForChannel : TEXCOORD1;
	float3 tcBlend         : TEXCOORD2;
	float4 tcLayer_tcMap   : TEXCOORD3;
	float3 dirToEye        : TEXCOORD4;
};

struct FSO
{
	float4 color : SV_Target0;
};

static const float g_bestPrecision = 50.0;
static const float g_layerScale = 1.0 / 8.0;

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubSimpleTerrainVS._eyePos_mapSideInv.xyz;
	const float mapSideInv = g_ubSimpleTerrainVS._eyePos_mapSideInv.w;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz;
	const float2 tcMap = pos.xz * mapSideInv + 0.5; // Range [0, 1).
	const float2 posBlend = pos.xz + edgeCorrection * 0.5;

	// <HeightAndNormal>
	float3 intactNrm;
	{
		const float approxHeight = g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + (0.5 * mapSideInv) * 16.0, 4.0).r + g_bestPrecision;
		pos.y = approxHeight;

		const float distToEye = distance(pos, eyePos);
		const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
		const float geomipsLodFrac = frac(geomipsLod);
		const float geomipsLodBase = floor(geomipsLod);
		const float geomipsLodNext = geomipsLodBase + 1.0;
		const float2 texelCenterAB = (0.5 * mapSideInv) * exp2(float2(geomipsLodBase, geomipsLodNext));
		const float yA = g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.xx, geomipsLodBase).r + g_bestPrecision;
		const float yB = g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + texelCenterAB.yy, geomipsLodNext).r + g_bestPrecision;
		pos.y = lerp(yA, yB, geomipsLodFrac);

		const float4 rawNormal = g_texNormalVS.SampleLevel(g_samNormalVS, tcMap + texelCenterAB.xx, geomipsLodBase);
		intactNrm = float3(rawNormal.x, 0, rawNormal.y) * 2.0 - 1.0;
		intactNrm.y = ComputeNormalZ(intactNrm.xz);
	}
	// </HeightAndNormal>

	so.pos = mul(float4(pos, 1), g_ubSimpleTerrainVS._matVP);
	so.nrmW = mul(intactNrm, (float3x3)g_ubSimpleTerrainVS._matW);
	so.layerForChannel = si.layerForChannel;
	so.tcBlend.xy = posBlend * mapSideInv + 0.5;
	so.tcBlend.z = pos.y;
	so.tcLayer_tcMap.xy = pos.xz * g_layerScale;
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * mapSideInv + 0.5; // Texel's center.
	so.dirToEye = eyePos - pos;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 layerForChannel = round(si.layerForChannel);
	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float4 rawBlend = g_texBlend.Sample(g_samBlend, si.tcBlend.xy);
	float4 weights = float4(rawBlend.rgb, 1.0 - dot(rawBlend.rgb, float3(1, 1, 1)));

	// <Albedo>
	float4 albedo;
	float specStrength;
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
		specStrength = dot(specStrengthForChannel, weights) * albedo.a;
	}
	// </Albedo>

	// <Water>
	float waterGlossBoost = 0.0;
	{
		const float dryMask = saturate(si.tcBlend.z);
		const float dryMask3 = dryMask * dryMask * dryMask;
		const float wetMask = 1.0 - dryMask;
		const float wetMask3 = wetMask * wetMask * wetMask;
		albedo.rgb *= dryMask3 * 0.5 + 0.5;
		specStrength = dryMask * saturate(specStrength + wetMask3 * wetMask3);
		waterGlossBoost = min(32.0, dryMask * wetMask3 * 128.0);
	}
	// </Water>

	const float gloss = lerp(3.3, 15.0, specStrength) + waterGlossBoost;

	const float3 normal = normalize(si.nrmW);
	const float3 dirToEye = normalize(si.dirToEye);

	const float4 litRet = VerusLit(g_ubSimpleTerrainFS._dirToSun.xyz, normal, dirToEye,
		gloss,
		g_ubSimpleTerrainFS._lamScaleBias.xy,
		float4(0, 0, 1, 0));

	const float3 diffColor = litRet.y * g_ubSimpleTerrainFS._sunColor.rgb * albedo.rgb;
	const float3 specColor = litRet.z * g_ubSimpleTerrainFS._sunColor.rgb * specStrength;

	so.color.rgb = diffColor + specColor;
	so.color.a = 1.0;

	return so;
}
#endif

//@main:#
