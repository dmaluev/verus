// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibSurface.hlsl"
#include "Water.inc.hlsl"

ConstantBuffer<UB_WaterVS> g_ubWaterVS : register(b0, space0);
ConstantBuffer<UB_WaterFS> g_ubWaterFS : register(b0, space1);

Texture2D    g_texTerrainHeightmapVS : register(t1, space0);
SamplerState g_samTerrainHeightmapVS : register(s1, space0);
Texture2D    g_texGenHeightmapVS     : register(t2, space0);
SamplerState g_samGenHeightmapVS     : register(s2, space0);

Texture2D    g_texTerrainHeightmap   : register(t1, space1);
SamplerState g_samTerrainHeightmap   : register(s1, space1);
Texture2D    g_texGenHeightmap       : register(t2, space1);
SamplerState g_samGenHeightmap       : register(s2, space1);
Texture2D    g_texGenNormals         : register(t3, space1);
SamplerState g_samGenNormals         : register(s3, space1);
Texture2D    g_texFoam               : register(t4, space1);
SamplerState g_samFoam               : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos      : SV_Position;
	float2 tc0      : TEXCOORD0;
	float2 tcLand   : TEXCOORD1;
	float4 dirToEye : TEXCOORD2;
};

struct FSO
{
	float4 color : SV_Target0;
};

static const float g_bestPrecision = 50.0;
static const float g_waterPatchTexelCenter = 0.5 / 1024.0;
static const float g_beachTexelCenter = 0.5 / 16.0;
static const float g_beachMip = 6.0;
static const float g_beachHeightScale = 2.0;
static const float g_adjustHeightBy = 1.0;
static const float g_foamScale = 8.0;

float ToLandMask(float height)
{
	return saturate(height * 0.2 + 1.0); // [-5 to 0] -> [0 to 1]
}

float ToPlanktonMask(float height)
{
	return saturate(height * 0.02 + 1.0);
}

float GetWaterHeightAt(
	Texture2D texTerrainHeightmap, SamplerState samTerrainHeightmap,
	Texture2D texGenHeightmap, SamplerState samGenHeightmap,
	float2 pos, float waterScale, float mapSideInv,
	float distToEye, float distToMipScale, float landDistToMipScale)
{
	const float2 tc = pos * waterScale;
	const float2 tcLand = pos * mapSideInv + 0.5;

	float landHeight = 0.0;
	{
		const float mip = log2(max(1.0, distToEye * landDistToMipScale));
		const float texelCenter = 0.5 * mapSideInv * exp2(mip);
		landHeight = texTerrainHeightmap.SampleLevel(samTerrainHeightmap, tcLand + texelCenter, mip).r + g_bestPrecision;
	}
	const float landMask = ToLandMask(landHeight);

	float seaWaterHeight = 0.0;
	{
		const float mip = log2(max(1.0, distToEye * distToMipScale));
		const float texelCenter = g_waterPatchTexelCenter * exp2(mip);
		seaWaterHeight = texGenHeightmap.SampleLevel(samGenHeightmap, tc + texelCenter, mip).r;
	}

	const float beachWaterHeight = texGenHeightmap.SampleLevel(samGenHeightmap, tc + g_beachTexelCenter, g_beachMip).r * g_beachHeightScale;

	const float height = lerp(seaWaterHeight, beachWaterHeight, landMask);

	const float scale = saturate(distToEye); // Too close -> wave can get clipped.

	return (height + g_adjustHeightBy) * scale * scale;
}

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubWaterVS._eyePos.xyz;
	const float waterScale = g_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.x;
	const float distToMipScale = g_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.y;
	const float mapSideInv = g_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.z;
	const float landDistToMipScale = g_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.w;

	float3 posW = mul(float4(si.pos.xyz, 1), g_ubWaterVS._matW).xyz;

	const float3 dirToEye = eyePos - posW;
	const float distToEye = length(dirToEye);

	const float height = GetWaterHeightAt(
		g_texTerrainHeightmapVS, g_samTerrainHeightmapVS,
		g_texGenHeightmapVS, g_samGenHeightmapVS,
		posW.xz, waterScale, mapSideInv,
		distToEye, distToMipScale, landDistToMipScale);
	posW.y = height;

	so.pos = mul(float4(posW, 1), g_ubWaterVS._matVP);
	so.tc0 = posW.xz * waterScale;
	so.tcLand = (posW.xz + 0.5) * mapSideInv + 0.5;
	so.dirToEye.xyz = dirToEye;
	so.dirToEye.w = distToEye;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 dirToEye = normalize(si.dirToEye.xyz);

	// <Land>
	float beachWaterHeight = 0.0;
	float landHeight = 0.0;
	float landMask = 0.0;
	float landMaskStatic = 0.0;
	{
		beachWaterHeight = g_texGenHeightmap.SampleLevel(g_samGenHeightmap, si.tc0 + g_beachTexelCenter, g_beachMip).r * g_beachHeightScale + g_adjustHeightBy;
		landHeight = g_texTerrainHeightmap.Sample(g_samTerrainHeightmap, si.tcLand).r + g_bestPrecision;
		landMask = ToLandMask(landHeight - beachWaterHeight);
		landMaskStatic = lerp(saturate((ToLandMask(landHeight) - 0.8) * 5.0), landMask, 0.1);
	}
	const float alpha = saturate((1.0 - landMask) * 20.0);
	// </Land>

	// <Foam>
	float4 foamColor = 0.0;
	{
		const float2 tcFoam = si.tc0 * g_foamScale;
		const float2 tcFoamA = tcFoam + g_ubWaterFS._phase.x + float2(0.5, 0.0);
		const float2 tcFoamB = tcFoam - g_ubWaterFS._phase.x;
		const float3 foamColorA = g_texFoam.Sample(g_samFoam, tcFoamA).rgb;
		const float3 foamColorB = g_texFoam.Sample(g_samFoam, tcFoamB).rgb;
		foamColor.rgb = lerp(foamColorA, foamColorB, 0.5) * 2.0;
		foamColor.a = saturate(foamColor.b * 2.0);
	}
	const float foamAlpha = 0.01 + 0.99 * landMaskStatic * foamColor.a;
	// </Foam>

	// <Plankton>
	float3 planktonColor = 0.0;
	{
		const float3 planktonColorShallow = float3(0.02, 0.42, 0.52);
		const float3 planktonColorDeep = float3(0.01, 0.011, 0.06);
		planktonColor = lerp(planktonColorDeep, planktonColorShallow, ToPlanktonMask(landHeight));
		planktonColor *= 0.5;
	}
	// </Plankton>

	// <Normal>
	float3 normal = 0.0;
	{
		const float3 rawNormal = g_texGenNormals.Sample(g_samGenNormals, si.tc0).rgb;
		const float3 normalWithLand = lerp(rawNormal, float3(0.5, 0.5, 1), landMask * 0.8);
		normal = normalize(normalWithLand.xzy * float3(-2, 2, -2) + float3(1, -1, 1));
	}
	// </Normal>

	const float spec = lerp(1.0, 0.1, foamAlpha);
	const float gloss = lerp(1024.0, 4.0, foamAlpha);

	const float3 waterColor = lerp(planktonColor, foamColor.rgb, foamAlpha);
	const float3 skyColor = float3(0.1, 0.26, 0.67) * 750.0;

	const float4 litRet = VerusLit(g_ubWaterFS._dirToSun.xyz, normal, dirToEye,
		gloss,
		float2(1, 0.3),
		float4(0, 0, 1, 0),
		1.0);

	const float fresnelTerm = FresnelSchlick(0.02, 0.97, litRet.w);

	const float3 diffColor = litRet.y * g_ubWaterFS._sunColor.rgb * waterColor;
	const float3 specColor = litRet.z * g_ubWaterFS._sunColor.rgb * spec + skyColor * fresnelTerm;

	so.color.rgb = diffColor + specColor;
	so.color.a = alpha;

	clip(so.color.a - 0.01);

	return so;
}
#endif

//@main:#0
////@main:#1 REFLECTION
////@main:#2 REFLECTION DISTORTED_REF
////@main:#3 REFLECTION DISTORTED_REF FOAM_HQ 3D_WATER
////@main:#4 REFLECTION DISTORTED_REF FOAM_HQ 3D_WATER REFRACTION

////@main:#Depth DEPTH
