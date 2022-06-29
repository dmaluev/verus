// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "Water.inc.hlsl"

CBUFFER(0, UB_WaterVS, g_ubWaterVS)
CBUFFER(1, UB_WaterFS, g_ubWaterFS)

Texture2D              g_texTerrainHeightmapVS : REG(t1, space0, t0);
SamplerState           g_samTerrainHeightmapVS : REG(s1, space0, s0);
Texture2D              g_texGenHeightmapVS     : REG(t2, space0, t1);
SamplerState           g_samGenHeightmapVS     : REG(s2, space0, s1);
Texture2D              g_texFoamVS             : REG(t3, space0, t2);
SamplerState           g_samFoamVS             : REG(s3, space0, s2);

Texture2D              g_texTerrainHeightmap   : REG(t1, space1, t3);
SamplerState           g_samTerrainHeightmap   : REG(s1, space1, s3);
Texture2D              g_texGenHeightmap       : REG(t2, space1, t4);
SamplerState           g_samGenHeightmap       : REG(s2, space1, s4);
Texture2D              g_texGenNormals         : REG(t3, space1, t5);
SamplerState           g_samGenNormals         : REG(s3, space1, s5);
Texture2D              g_texFoam               : REG(t4, space1, t6);
SamplerState           g_samFoam               : REG(s4, space1, s6);
Texture2D              g_texReflection         : REG(t5, space1, t7);
SamplerState           g_samReflection         : REG(s5, space1, s7);
Texture2D              g_texRefraction         : REG(t6, space1, t8);
SamplerState           g_samRefraction         : REG(s6, space1, s8);
Texture2D              g_texGBuffer3           : REG(t7, space1, t9);
SamplerState           g_samGBuffer3           : REG(s7, space1, s9);
Texture2D              g_texShadowCmp          : REG(t8, space1, t10);
SamplerComparisonState g_samShadowCmp          : REG(s8, space1, s10);
Texture2D              g_texShadow             : REG(t9, space1, t11);
SamplerState           g_samShadow             : REG(s9, space1, s11);

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

static const float g_waterPatchTexelCenter = 0.5 / 1024.0;
static const float g_beachTexelCenter = 0.5 / 16.0;
static const float g_beachMip = 6.0;
static const float g_beachHeightScale = 3.0;
static const float g_adjustHeightBy = 0.7;
static const float g_foamScale = 8.0;
static const float g_shadeScale = 1.0 / 32.0;
static const float g_wavesZoneMaskCenter = -10.0;
static const float g_wavesZoneMaskContrast = 0.12;
static const float g_wavesMaskScaleBias = 20.0;

float2 GetWaterHeightAt(
	Texture2D texTerrainHeightmap, SamplerState samTerrainHeightmap,
	Texture2D texGenHeightmap, SamplerState samGenHeightmap,
	float2 pos, float waterScale, float invMapSide,
	float distToEye, float distToMipScale, float landDistToMipScale)
{
	const float2 tc = pos * waterScale;
	const float2 tcLand = pos * invMapSide + 0.5;

	float landHeight;
	{
		const float mip = log2(max(1.0, distToEye * landDistToMipScale));
		const float texelCenter = 0.5 * invMapSide * exp2(mip);
		landHeight = UnpackTerrainHeight(texTerrainHeightmap.SampleLevel(samTerrainHeightmap, tcLand + texelCenter, mip).r);
	}
	const float landMask = ToLandMask(landHeight);

	float seaWaterHeight;
	{
		const float mip = log2(max(1.0, distToEye * distToMipScale));
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

	const float3 eyePos = g_ubWaterVS._eyePos_invMapSide.xyz;
	const float invMapSide = g_ubWaterVS._eyePos_invMapSide.w;
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
		posW.xz, waterScale, invMapSide,
		distToEye, distToMipScale, landDistToMipScale);

	const float2 tc0 = posW.xz * waterScale;

	// <Waves>
	float wave;
	{
		const float phaseShift = saturate(g_texFoamVS.SampleLevel(g_samFoamVS, tc0 * g_shadeScale, 4.0).r * 2.0);
		const float wavesZoneMask = saturate(1.0 - g_wavesZoneMaskContrast * abs(height_landHeight.y - g_wavesZoneMaskCenter));
		const float2 wavesMaskCenter = frac(wavePhase + phaseShift + float2(0.0, 0.5)) * g_wavesMaskScaleBias - g_wavesMaskScaleBias;
		const float2 absDeltaAsym = AsymAbs(height_landHeight.y - wavesMaskCenter, -1.0, 3.0);
		const float wavesMask = smoothstep(0.0, 1.0, saturate(1.0 - 0.15 * min(absDeltaAsym.x, absDeltaAsym.y)));
		wave = wavesMask * wavesZoneMask;
	}
	// </Waves>

	posW.y = height_landHeight.x + wave * 2.0 * saturate(1.0 / (distToEye * 0.01));

	so.pos = mul(float4(posW, 1), g_ubWaterVS._matVP);
	so.tc0 = tc0;
	so.tcLand_height.xy = (posW.xz + 0.5) * invMapSide + 0.5;
	so.tcLand_height.z = height_landHeight.x;
	so.tcScreen = mul(float4(posW, 1), g_ubWaterVS._matScreen);
	so.dirToEye_depth.xyz = dirToEye;
	so.dirToEye_depth.w = so.pos.w;
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
	const float depthFactor = sqrt(depth);

	const float4 tcScreenInv = 1.0 / si.tcScreen;
	const float perspectScale = tcScreenInv.z;
	const float2 tcScreen = si.tcScreen.xy * tcScreenInv.w;

	// <Land>
	float landHeight;
	float landMask;
	float landMaskStillWater;
	{
		const float beachWaterHeight = g_texGenHeightmap.SampleLevel(g_samGenHeightmap, si.tc0 + g_beachTexelCenter, g_beachMip).r * g_beachHeightScale + g_adjustHeightBy;
		landHeight = UnpackTerrainHeight(g_texTerrainHeightmap.SampleLevel(g_samTerrainHeightmap, si.tcLand_height.xy, 0.0).r);
		landMask = ToLandMask(landHeight - beachWaterHeight);
		landMaskStillWater = lerp(saturate((ToLandMask(landHeight) - 0.8) * 5.0), landMask, 0.1);
	}
	const float alpha = saturate((1.0 - landMask) * 20.0);
	// </Land>

	// <Waves>
	float wave;
	float fresnelWaveFactor; // A trick to make waves look more 3D.
	{
		const float phaseShift = saturate(g_texFoam.SampleLevel(g_samFoam, si.tc0 * g_shadeScale, 4.0).r * 2.0);
		const float wavesZoneMask = saturate(1.0 - g_wavesZoneMaskContrast * abs(landHeight - g_wavesZoneMaskCenter));
		const float2 wavesMaskCenter = frac(wavePhase + phaseShift + float2(0.0, 0.5)) * g_wavesMaskScaleBias - g_wavesMaskScaleBias;
		const float2 delta = landHeight - wavesMaskCenter;
		const float2 absDelta = abs(delta);
		const float2 absDeltaAsym = AsymAbs(delta, -1.0, 3.5);
		const float wavesMask = saturate(1.0 - 0.5 * min(absDeltaAsym.x, absDeltaAsym.y));
		wave = wavesMask * wavesZoneMask;
		const float biggerWavesMask = saturate(1.0 - 0.5 * min(absDelta.x, absDelta.y));
		fresnelWaveFactor = saturate(1.0 - biggerWavesMask * wavesZoneMask * 1.5);
	}
	// </Waves>

	// <Normal>
	float3 normal;
	{
		const float3 normalSam = g_texGenNormals.Sample(g_samGenNormals, si.tc0).rgb;
		const float3 normalWithLand = lerp(normalSam, float3(0.5, 0.5, 1), saturate(landMask * 0.8 + wave));
		normal = normalize(normalWithLand.xzy * float3(-2, 2, -2) + float3(1, -1, 1));
	}
	// </Normal>

	// <Reflection>
	float3 reflectColor;
	{
		const float fudgeFactor = 100.0;
		const float3 flatReflect = reflect(-dirToEye, float3(0, 1, 0));
		const float3 bumpReflect = reflect(-dirToEye, normal);
		const float3 delta = max(bumpReflect - flatReflect, float3(-1, 0, -1)); // Don't look under the terrain.
		const float2 viewSpaceOffsetPerMeter = mul(float4(delta, 0), g_ubWaterFS._matV).xy;
		const float2 offset = viewSpaceOffsetPerMeter * saturate(perspectScale) * camScale * fudgeFactor;
		const float2 blurScale = float2(1.0, max(1.0, depthFactor));
		const float2 tc = tcScreen + float2(offset.x, 0.005 + max(0.0, offset.y));
		const float2 tcddx = ddx(tc) * blurScale;
		const float2 tcddy = ddy(tc) * blurScale;
		reflectColor = g_texReflection.SampleGrad(g_samReflection, tc, tcddx, tcddy).rgb;
	}
	// </Reflection>

	// <Refraction>
	float3 refractColor;
	{
		const float fudgeFactor = (0.1 + 0.9 * abs(dirToEye.y) * abs(dirToEye.y)) * 3.0;
		const float2 tcScreen2 = 1.0 - abs(tcScreen * 2.0 - 1.0);
		const float mask = tcScreen2.x * tcScreen2.y;
		const float3 bumpRefract = CheapRefract(-dirToEye, normal);
		const float3 delta = bumpRefract + dirToEye;
		const float2 viewSpaceOffsetPerMeter = mul(float4(delta, 0), g_ubWaterFS._matV).xy * mask;
		const float2 offset = viewSpaceOffsetPerMeter * saturate(perspectScale) * camScale * fudgeFactor * abs(landHeight);
		const float underwaterMask = g_texGBuffer3.SampleLevel(g_samGBuffer3, tcScreen + offset, 0.0).b;
		const float2 finalOffset = offset * underwaterMask;
		const float2 tcRefractR = tcScreen + finalOffset * 0.9;
		const float2 tcRefractG = tcScreen + finalOffset * 1.0;
		const float2 tcRefractB = tcScreen + finalOffset * 1.1;
		refractColor = float3(
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractR, 0.0).r,
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractG, 0.0).g,
			g_texRefraction.SampleLevel(g_samRefraction, tcRefractB, 0.0).b);
	}
	// </Refraction>

	const float shade = g_texFoam.Sample(g_samFoam, si.tc0 * g_shadeScale).r;

	// <Plankton>
	float3 planktonColor;
	float refractMask;
	{
		const float planktonMask = ToWaterPlanktonMask(landHeight);
		planktonColor = lerp(g_ubWaterFS._diffuseColorDeep.rgb, g_ubWaterFS._diffuseColorShallow.rgb, planktonMask);
		planktonColor *= 0.4 + 0.6 * saturate(shade - wave);
		refractMask = saturate((planktonMask - 0.75) * 4.0);
	}
	// </Plankton>

	// <Foam>
	float4 foamColor;
	{
		const float2 tcFoam = si.tc0 * g_foamScale;
		const float2 tcFoamA = tcFoam + phase + float2(0.5, 0.0);
		const float2 tcFoamB = tcFoam - phase;
		const float3 foamColorA = g_texFoam.Sample(g_samFoam, tcFoamA).rgb;
		const float3 foamColorB = g_texFoam.Sample(g_samFoam, tcFoamB).rgb;
		foamColor.rgb = lerp(foamColorA, foamColorB, 0.5);
		foamColor.a = saturate(foamColor.b * 3.0); // Tweak.

		const float solidMask = saturate((landMask - 0.8) * 5.0) * shade;
		foamColor = lerp(foamColor, 1.0, max(solidMask, saturate(wave * 0.75)));

		const float a = saturate(height - 0.25);
		const float a2 = a * a;
		const float a4 = a2 * a2;
		const float heightBasedAlpha = saturate(a4 * a4 + wave * 0.75);

		foamColor.rgb = saturate(foamColor.rgb + 0.5); // Tweak.
		foamColor.a = 0.002 + 0.998 * foamColor.a * saturate(landMaskStillWater + heightBasedAlpha);
	}
	// </Foam>

	const float wrapDiffuse = 1.0;

	// <Shadow>
	float shadowMask;
	{
		float4 shadowConfig = g_ubWaterFS._shadowConfig;
		shadowConfig.y = 0.5;
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

	const float3 albedo = lerp(planktonColor, foamColor.rgb, foamColor.a);
	const float roughness = clamp(lerp(0.05, 0.75, foamColor.a) * depthFactor * 0.1, 0.05, 1.0);

	float3 punctualDiff, punctualSpec, fresnel;
	VerusWaterLit(normal, g_ubWaterFS._dirToSun.xyz, dirToEye,
		albedo,
		roughness, wrapDiffuse,
		punctualDiff, punctualSpec, fresnel);

	punctualDiff *= g_ubWaterFS._sunColor.rgb * shadowMask;
	punctualSpec *= g_ubWaterFS._sunColor.rgb * shadowMask;

	const float3 diff = lerp(albedo * (punctualDiff + g_ubWaterFS._ambientColor.rgb), refractColor, refractMask * (1.0 - foamColor.a));
	const float3 spec = max(punctualSpec, reflectColor);

	const float3 color = lerp(diff, spec, fresnel * fresnelWaveFactor);

	const float fog = ComputeFog(depth, g_ubWaterFS._fogColor.a);

	so.color.rgb = lerp(color, g_ubWaterFS._fogColor.rgb, fog);
	so.color.a = alpha;

	so.color.rgb = SaturateHDR(so.color.rgb);

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
