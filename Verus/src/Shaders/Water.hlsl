// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "Water.inc.hlsl"

ConstantBuffer<UB_WaterVS> g_ubWaterVS : register(b0, space0);
ConstantBuffer<UB_WaterFS> g_ubWaterFS : register(b0, space1);

Texture2D              g_texTerrainHeightmapVS : register(t1, space0);
SamplerState           g_samTerrainHeightmapVS : register(s1, space0);
Texture2D              g_texGenHeightmapVS     : register(t2, space0);
SamplerState           g_samGenHeightmapVS     : register(s2, space0);
Texture2D              g_texFoamVS             : register(t3, space0);
SamplerState           g_samFoamVS             : register(s3, space0);

Texture2D              g_texTerrainHeightmap   : register(t1, space1);
SamplerState           g_samTerrainHeightmap   : register(s1, space1);
Texture2D              g_texGenHeightmap       : register(t2, space1);
SamplerState           g_samGenHeightmap       : register(s2, space1);
Texture2D              g_texGenNormals         : register(t3, space1);
SamplerState           g_samGenNormals         : register(s3, space1);
Texture2D              g_texFoam               : register(t4, space1);
SamplerState           g_samFoam               : register(s4, space1);
Texture2D              g_texReflection         : register(t5, space1);
SamplerState           g_samReflection         : register(s5, space1);
Texture2D              g_texRefraction         : register(t6, space1);
SamplerState           g_samRefraction         : register(s6, space1);
Texture2D              g_texShadowCmp          : register(t7, space1);
SamplerComparisonState g_samShadowCmp          : register(s7, space1);
Texture2D              g_texShadow             : register(t8, space1);
SamplerState           g_samShadow             : register(s8, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos            : SV_Position;
	float2 tc0            : TEXCOORD0;
	float3 tcLand_height  : TEXCOORD1;
	float4 tcScreen       : TEXCOORD2;
	float4 dirToEye_depth : TEXCOORD3;
	float3 posW           : TEXCOORD4;
};

struct FSO
{
	float4 color : SV_Target0;
};

static const float g_waterPatchTexelCenter = 0.5f / 1024.f;
static const float g_beachTexelCenter = 0.5f / 16.f;
static const float g_beachMip = 6.f;
static const float g_beachHeightScale = 3.f;
static const float g_adjustHeightBy = 0.7f;
static const float g_foamScale = 8.f;
static const float g_shadeScale = 1.f / 32.f;
static const float g_wavesZoneMaskCenter = -10.f;
static const float g_wavesZoneMaskContrast = 0.12f;
static const float g_wavesMaskScaleBias = 20.f;

float2 GetWaterHeightAt(
	Texture2D texTerrainHeightmap, SamplerState samTerrainHeightmap,
	Texture2D texGenHeightmap, SamplerState samGenHeightmap,
	float2 pos, float waterScale, float mapSideInv,
	float distToEye, float distToMipScale, float landDistToMipScale)
{
	const float2 tc = pos * waterScale;
	const float2 tcLand = pos * mapSideInv + 0.5f;

	float landHeight;
	{
		const float mip = log2(max(1.f, distToEye * landDistToMipScale));
		const float texelCenter = 0.5f * mapSideInv * exp2(mip);
		landHeight = UnpackTerrainHeight(texTerrainHeightmap.SampleLevel(samTerrainHeightmap, tcLand + texelCenter, mip).r);
	}
	const float landMask = ToLandMask(landHeight);

	float seaWaterHeight;
	{
		const float mip = log2(max(1.f, distToEye * distToMipScale));
		const float texelCenter = g_waterPatchTexelCenter * exp2(mip);
		seaWaterHeight = texGenHeightmap.SampleLevel(samGenHeightmap, tc + texelCenter, mip).r;
	}

	const float beachWaterHeight = texGenHeightmap.SampleLevel(samGenHeightmap, tc + g_beachTexelCenter, g_beachMip).r * g_beachHeightScale;

	const float height = lerp(seaWaterHeight, beachWaterHeight, landMask);

	const float scale = saturate(distToEye); // Too close -> wave can get clipped.

	return float2((height + g_adjustHeightBy) * scale * scale, landHeight);
}

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubWaterVS._eyePos_mapSideInv.xyz;
	const float mapSideInv = g_ubWaterVS._eyePos_mapSideInv.w;
	const float waterScale = g_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.x;
	const float distToMipScale = g_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.y;
	const float landDistToMipScale = g_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.z;
	const float wavePhase = g_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.w;

	float3 posW = mul(float4(si.pos.xyz, 1), g_ubWaterVS._matW).xyz;

	const float3 dirToEye = eyePos - posW;
	const float distToEye = length(dirToEye);

	const float2 height_landHeight = GetWaterHeightAt(
		g_texTerrainHeightmapVS, g_samTerrainHeightmapVS,
		g_texGenHeightmapVS, g_samGenHeightmapVS,
		posW.xz, waterScale, mapSideInv,
		distToEye, distToMipScale, landDistToMipScale);

	const float2 tc0 = posW.xz * waterScale;

	// <Waves>
	float wave;
	{
		const float phaseShift = saturate(g_texFoamVS.SampleLevel(g_samFoamVS, tc0 * g_shadeScale, 4.f).r * 2.f);
		const float wavesZoneMask = saturate(1.f - g_wavesZoneMaskContrast * abs(height_landHeight.y - g_wavesZoneMaskCenter));
		const float2 wavesMaskCenter = frac(wavePhase + phaseShift + float2(0.f, 0.5f)) * g_wavesMaskScaleBias - g_wavesMaskScaleBias;
		const float2 absDeltaAsym = AsymAbs(height_landHeight.y - wavesMaskCenter, -1.f, 3.f);
		const float wavesMask = smoothstep(0.f, 1.f, saturate(1.f - 0.15f * min(absDeltaAsym.x, absDeltaAsym.y)));
		wave = wavesMask * wavesZoneMask;
	}
	// </Waves>

	posW.y = height_landHeight.x + wave * 2.f * saturate(1.f / (distToEye * 0.01f));

	so.pos = mul(float4(posW, 1), g_ubWaterVS._matVP);
	so.tc0 = tc0;
	so.tcLand_height.xy = (posW.xz + 0.5f) * mapSideInv + 0.5f;
	so.tcLand_height.z = height_landHeight.x;
	so.tcScreen = mul(float4(posW, 1), g_ubWaterVS._matScreen);
	so.dirToEye_depth.xyz = dirToEye;
	so.dirToEye_depth.w = so.pos.z;
	so.posW = posW;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float phase = g_ubWaterFS._phase_wavePhase_camScale.x;
	const float wavePhase = g_ubWaterFS._phase_wavePhase_camScale.y;
	const float2 camScale = g_ubWaterFS._phase_wavePhase_camScale.zw;

	const float height = si.tcLand_height.z;
	const float3 dirToEye = normalize(si.dirToEye_depth.xyz);
	const float depth = si.dirToEye_depth.w;

	const float4 tcScreenInv = 1.f / si.tcScreen;
	const float perspectScale = tcScreenInv.z;
	const float2 tcScreen = si.tcScreen.xy * tcScreenInv.w;

	// <Land>
	float beachWaterHeight;
	float landHeight;
	float landMask;
	float landMaskStatic;
	{
		beachWaterHeight = g_texGenHeightmap.SampleLevel(g_samGenHeightmap, si.tc0 + g_beachTexelCenter, g_beachMip).r * g_beachHeightScale + g_adjustHeightBy;
		landHeight = UnpackTerrainHeight(g_texTerrainHeightmap.Sample(g_samTerrainHeightmap, si.tcLand_height.xy).r);
		landMask = ToLandMask(landHeight - beachWaterHeight);
		landMaskStatic = lerp(saturate((ToLandMask(landHeight) - 0.8f) * 5.f), landMask, 0.1f);
	}
	const float alpha = saturate((1.f - landMask) * 20.f);
	// </Land>

	// <Waves>
	float wave;
	float fresnelDimmer; // A trick to make waves look more 3D.
	{
		const float phaseShift = saturate(g_texFoam.SampleLevel(g_samFoam, si.tc0 * g_shadeScale, 4.f).r * 2.f);
		const float wavesZoneMask = saturate(1.f - g_wavesZoneMaskContrast * abs(landHeight - g_wavesZoneMaskCenter));
		const float2 wavesMaskCenter = frac(wavePhase + phaseShift + float2(0.f, 0.5f)) * g_wavesMaskScaleBias - g_wavesMaskScaleBias;
		const float2 delta = landHeight - wavesMaskCenter;
		const float2 absDelta = abs(delta);
		const float2 absDeltaAsym = AsymAbs(delta, -1.f, 3.5f);
		const float wavesMask = saturate(1.f - 0.5f * min(absDeltaAsym.x, absDeltaAsym.y));
		wave = wavesMask * wavesZoneMask;
		const float biggerWavesMask = saturate(1.f - 0.5f * min(absDelta.x, absDelta.y));
		fresnelDimmer = saturate(1.f - biggerWavesMask * wavesZoneMask * 1.5f);
	}
	// </Waves>

	// <Normal>
	float3 normal;
	{
		const float3 rawNormal = g_texGenNormals.Sample(g_samGenNormals, si.tc0).rgb;
		const float3 normalWithLand = lerp(rawNormal, float3(0.5f, 0.5f, 1), saturate(landMask * 0.8f + wave));
		normal = normalize(normalWithLand.xzy * float3(-2, 2, -2) + float3(1, -1, 1));
	}
	// </Normal>

	// <Reflection>
	float3 reflectColor;
	{
		const float fudgeFactor = 32.f;
		const float3 flatReflect = reflect(-dirToEye, float3(0, 1, 0));
		const float3 bumpReflect = reflect(-dirToEye, normal);
		const float3 delta = bumpReflect - flatReflect;
		const float2 viewSpaceOffsetPerMeter = mul(float4(delta, 0), g_ubWaterFS._matV).xy;
		const float2 offset = viewSpaceOffsetPerMeter * saturate(perspectScale) * camScale * fudgeFactor;
		reflectColor = g_texReflection.Sample(g_samReflection, tcScreen + max(offset, float2(-1, 0))).rgb;
		reflectColor = ReflectionDimming(reflectColor, 0.8f);
	}
	// </Reflection>

	// <Refraction>
	float3 refractColor;
	{
		const float fudgeFactor = (0.1f + 0.9f * abs(dirToEye.y) * abs(dirToEye.y)) * 2.5f;
		const float waterDepth = -landHeight;
		const float2 tcScreen2 = tcScreen * 2.f - 1.f;
		const float mask = saturate(dot(tcScreen2, tcScreen2));
		const float3 bumpRefract = refract(-dirToEye, normal, 1.f / 1.33f);
		const float3 delta = bumpRefract + dirToEye;
		const float2 viewSpaceOffsetPerMeter = mul(float4(delta, 0), g_ubWaterFS._matV).xy * (1.f - mask);
		const float2 offset = viewSpaceOffsetPerMeter * waterDepth * saturate(perspectScale) * camScale * fudgeFactor;
		const float2 tcRefractR = tcScreen + offset * 0.9f;
		const float2 tcRefractG = tcScreen + offset * 1.f;
		const float2 tcRefractB = tcScreen + offset * 1.1f;
		refractColor = float3(
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractR, 0.f).r,
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractG, 0.f).g,
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractB, 0.f).b);
	}
	// </Refraction>

	const float shade = g_texFoam.Sample(g_samFoam, si.tc0 * g_shadeScale).r;

	// <Foam>
	float4 foamColor;
	float foamHeightAlpha;
	{
		const float2 tcFoam = si.tc0 * g_foamScale;
		const float2 tcFoamA = tcFoam + phase + float2(0.5f, 0.f);
		const float2 tcFoamB = tcFoam - phase;
		const float3 foamColorA = g_texFoam.Sample(g_samFoam, tcFoamA).rgb;
		const float3 foamColorB = g_texFoam.Sample(g_samFoam, tcFoamB).rgb;
		foamColor.rgb = lerp(foamColorA, foamColorB, 0.5f);
		foamColor.a = saturate(foamColor.b * 2.f);

		const float a = saturate(height - 0.1f);
		const float a2 = a * a;
		const float a4 = a2 * a2;
		foamHeightAlpha = saturate(a4 * a4 + wave * 0.75f);

		const float complement = 1.f - shade;
		const float mask = saturate((landMask - 0.9f) * 10.f * (1.f - shade * 3.f)) * complement * complement;
		const float superfoam = saturate(mask * (1.f - mask) * 3.f);
		foamColor = lerp(foamColor, 1.f, max(superfoam * superfoam, saturate(wave * 0.75f)));
	}
	const float foamAlpha = 0.002f + 0.998f * saturate(landMaskStatic + foamHeightAlpha) * foamColor.a;
	// </Foam>

	// <Plankton>
	float3 planktonColor;
	float refractMask;
	{
		const float diffuseMask = ToWaterDiffuseMask(landHeight);
		planktonColor = lerp(g_ubWaterFS._diffuseColorDeep.rgb, g_ubWaterFS._diffuseColorShallow.rgb, diffuseMask);
		planktonColor *= 0.1f + 0.2f * saturate(shade - wave);
		refractMask = saturate((diffuseMask - 0.75f) * 4.f);
	}
	// </Plankton>

	const float spec = lerp(1.f, 0.1f, foamAlpha);
	const float gloss = lerp(1200.f, 4.f, foamAlpha);

	const float2 lamScaleBias = float2(1, 0.2f);

	// <Shadow>
	float shadowMask;
	{
		float4 shadowConfig = g_ubWaterFS._shadowConfig;
		const float lamBiasMask = saturate(lamScaleBias.y * shadowConfig.y);
		shadowConfig.y = 1.f - lamBiasMask; // Keep penumbra blurry.
		const float3 posForShadow = AdjustPosForShadow(si.posW, normal, g_ubWaterFS._dirToSun.xyz, depth);
		shadowMask = ShadowMapCSM(
			g_texShadowCmp,
			g_samShadowCmp,
			g_texShadow,
			g_samShadow,
			si.posW,
			posForShadow,
			g_ubWaterFS._matShadow,
			g_ubWaterFS._matShadowCSM1,
			g_ubWaterFS._matShadowCSM2,
			g_ubWaterFS._matShadowCSM3,
			g_ubWaterFS._matScreenCSM,
			g_ubWaterFS._csmSplitRanges,
			shadowConfig);
	}
	// </Shadow>

	const float4 litRet = VerusLit(g_ubWaterFS._dirToSun.xyz, normal, dirToEye,
		gloss,
		lamScaleBias,
		float4(0, 0, 1, 0),
		1.f);

	const float fresnelTerm = FresnelSchlick(0.03f, 0.6f * fresnelDimmer, litRet.w);

	const float3 diff = litRet.y * g_ubWaterFS._sunColor.rgb * shadowMask + g_ubWaterFS._ambientColor.rgb;
	const float3 diffColorFoam = foamColor.rgb * diff;
	const float3 diffColorOpaque = planktonColor * diff;
	const float3 diffColor = lerp(lerp(diffColorOpaque, refractColor, refractMask), diffColorFoam, foamAlpha);
	const float3 specColor = max(litRet.z * g_ubWaterFS._sunColor.rgb * shadowMask * spec, reflectColor) * fresnelTerm;

	const float fog = ComputeFog(depth, g_ubWaterFS._fogColor.a);

	so.color.rgb = lerp(diffColor * (1.f - fresnelTerm) + specColor, g_ubWaterFS._fogColor.rgb, fog);
	so.color.a = alpha;

	clip(so.color.a - 0.01f);

	return so;
}
#endif

//@main:#0
////@main:#1 REFLECTION
////@main:#2 REFLECTION DISTORTED_REF
////@main:#3 REFLECTION DISTORTED_REF FOAM_HQ 3D_WATER
////@main:#4 REFLECTION DISTORTED_REF FOAM_HQ 3D_WATER REFRACTION

////@main:#Depth DEPTH
