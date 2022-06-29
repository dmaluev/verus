// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "DS.inc.hlsl"

CBUFFER(0, UB_PerFrame, g_ubPerFrame)
CBUFFER(1, UB_TexturesFS, g_ubTexturesFS)
CBUFFER(2, UB_PerMeshVS, g_ubPerMeshVS)
CBUFFER(3, UB_ShadowFS, g_ubShadowFS)
VK_PUSH_CONSTANT
CBUFFER(4, UB_PerObject, g_ubPerObject)

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, 1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, 2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, 3, space1);
VK_SUBPASS_INPUT(3, g_texGBuffer3, g_samGBuffer3, 4, space1);
VK_SUBPASS_INPUT(4, g_texDepth, g_samDepth, 5, space1);
Texture2D              g_texShadowCmp : REG(t6, space1, t5);
SamplerComparisonState g_samShadowCmp : REG(s6, space1, s5);
Texture2D              g_texShadow    : REG(t7, space1, t6);
SamplerState           g_samShadow    : REG(s7, space1, s6);

struct VSI
{
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int2 tc0   : TEXCOORD0;
	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos                         : SV_Position;
	float4 clipSpacePos                : TEXCOORD0;
#if defined(DEF_DIR) || defined(DEF_SPOT) // Direction and cone shape for spot.
	float4 lightDirWV_invConeDelta     : TEXCOORD1;
#endif
#if defined(DEF_OMNI) || defined(DEF_SPOT) // Omni and spot have position and radius.
	float3 lightPosWV                  : TEXCOORD2;
	float3 radius_radiusSq_invRadiusSq : TEXCOORD3;
#endif
	float4 color_coneOut               : TEXCOORD4;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);

	// <TheMatrix>
#ifdef DEF_INSTANCED
	const mataff matW = GetInstMatrix(
		si.matPart0,
		si.matPart1,
		si.matPart2);
	const float3 color = si.instData.rgb;
	const float coneIn = si.instData.a;
#else
	const mataff matW = g_ubPerObject._matW;
	const float3 color = g_ubPerObject._color.rgb;
	const float coneIn = g_ubPerObject._color.a;
#endif
	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubPerFrame._matV));
	const float3x3 matWV33 = (float3x3)matWV;
	// </TheMatrix>

#ifdef DEF_DIR // Fullscreen quad?
	so.pos = float4(inPos, 1);
#else
	const float3 posW = mul(float4(inPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_ubPerFrame._matVP);
#endif
	so.clipSpacePos = so.pos;

	// <MoreLightParams>
	so.color_coneOut = float4(color, 0);
	const float3 scaledFrontDir = mul(float3(0, 0, 1), matWV33);
#ifdef DEF_DIR
	{
		so.lightDirWV_invConeDelta = float4(scaledFrontDir, 1); // Assume not scaled.
	}
#elif defined(DEF_SPOT)
	{
		const float3 frontDir = normalize(scaledFrontDir);
		so.lightDirWV_invConeDelta.xyz = frontDir;
		const float3 scaledConeDir = mul(float3(0, 1, 1), matWV33);
		const float3 coneDir = normalize(scaledConeDir);
		const float coneOut = dot(frontDir, coneDir);
		const float invConeDelta = 1.0 / (coneIn - coneOut);
		so.lightDirWV_invConeDelta.w = invConeDelta;
		so.color_coneOut.a = coneOut;
	}
#endif
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	{
		so.lightPosWV = mul(float4(0, 0, 0, 1), matWV).xyz;
		so.radius_radiusSq_invRadiusSq.y = dot(scaledFrontDir, scaledFrontDir);
		so.radius_radiusSq_invRadiusSq.x = sqrt(so.radius_radiusSq_invRadiusSq.y);
		so.radius_radiusSq_invRadiusSq.z = 1.0 / so.radius_radiusSq_invRadiusSq.y;
	}
#endif
	// </MoreLightParams>

	return so;
}
#endif

#ifdef _FS
DS_ACC_FSO mainFS(VSO si)
{
	DS_ACC_FSO so;
	DS_Reset(so);

#ifdef DEF_DIR
	const float3 ndcPos = si.clipSpacePos.xyz;
#else
	const float3 ndcPos = si.clipSpacePos.xyz / si.clipSpacePos.w;
#endif
	const float2 tc0 = mul(float4(ndcPos.xy, 0, 1), g_ubPerFrame._matToUV).xy;

	// Direction and cone shape for spot:
#ifdef DEF_DIR
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
#elif defined(DEF_SPOT)
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
	const float invConeDelta = si.lightDirWV_invConeDelta.w;
	const float coneOut = si.color_coneOut.a;
#endif

	// Omni and spot have position and radius:
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float3 lightPosWV = si.lightPosWV;
	const float radius = si.radius_radiusSq_invRadiusSq.x;
	const float radiusSq = si.radius_radiusSq_invRadiusSq.y;
	const float invRadiusSq = si.radius_radiusSq_invRadiusSq.z;
#endif

	// Depth:
	const float depthSam = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, tc0).r;
	const float3 posWV = DS_GetPosition(depthSam, g_ubPerFrame._matInvP, ndcPos.xy);

#if defined(DEF_OMNI) || defined(DEF_SPOT)
	if (posWV.z <= lightPosWV.z + radius)
#endif
	{
		// <SampleSurfaceData>
		// GBuffer0 {Albedo.rgb, SSSHue}:
		const float4 gBuffer0Sam = VK_SUBPASS_LOAD(g_texGBuffer0, g_samGBuffer0, tc0);
		const float3 sssColor = SSSHueToColor(gBuffer0Sam.a);

		// GBuffer1 {Normal.xy, Emission, MotionBlur}:
		const float4 gBuffer1Sam = VK_SUBPASS_LOAD(g_texGBuffer1, g_samGBuffer1, tc0);
		const float3 normalWV = DS_GetNormal(gBuffer1Sam);

		// GBuffer2 {Occlusion, Roughness, Metallic, WrapDiffuse}:
		const float4 gBuffer2Sam = VK_SUBPASS_LOAD(g_texGBuffer2, g_samGBuffer2, tc0);
		const float roughness = gBuffer2Sam.g;
		const float metallic = gBuffer2Sam.b;
		const float wrapDiffuse = gBuffer2Sam.a;

		// GBuffer3 {Tangent.xy, AnisoSpec, RoughDiffuse}:
		const float4 gBuffer3Sam = VK_SUBPASS_LOAD(g_texGBuffer3, g_samGBuffer3, tc0);
		const float3 tangentWV = DS_GetTangent(gBuffer3Sam);
		const float anisoSpec = gBuffer3Sam.b;
		const float roughDiffuse = frac(gBuffer3Sam.a);
		// </SampleSurfaceData>

		// <LightData>
#ifdef DEF_DIR
		const float3 dirToLightWV = -lightDirWV;
		const float lightFalloff = 1.0; // No falloff.
#else
		const float3 toLightWV = lightPosWV - posWV;
		const float3 dirToLightWV = normalize(toLightWV);
		const float distToLightSq = dot(toLightWV, toLightWV);
		const float lightFalloff = ComputePointLightIntensity(distToLightSq, radiusSq, invRadiusSq);
#endif
#ifdef DEF_SPOT // Extra step for spot light:
		const float coneIntensity = ComputeSpotLightConeIntensity(dirToLightWV, lightDirWV, coneOut, invConeDelta);
#else
		const float coneIntensity = 1.0; // No cone.
#endif
		const float lightFalloffWithCone = lightFalloff * coneIntensity;
		const float3 dirToEyeWV = normalize(-posWV);
		const float3 lightColor = si.color_coneOut.rgb;
		// </LightData>

		// <Shadow>
		float shadowMask = 1.0;
		{
#ifdef DEF_DIR
			float4 shadowConfig = g_ubShadowFS._shadowConfig;
			shadowConfig.y = 1.0 - saturate(wrapDiffuse * shadowConfig.y);
			const float3 posForShadow = AdjustPosForShadow(posWV, normalWV, dirToLightWV, -posWV.z);
			shadowMask = ShadowMapCSM(
				g_texShadowCmp,
				g_samShadowCmp,
				g_texShadow,
				g_samShadow,
				posWV,
				posForShadow,
				g_ubShadowFS._matShadow,
				g_ubShadowFS._matShadowCSM1,
				g_ubShadowFS._matShadowCSM2,
				g_ubShadowFS._matShadowCSM3,
				g_ubShadowFS._matScreenCSM,
				g_ubShadowFS._csmSplitRanges,
				shadowConfig);
#endif
		}
		// </Shadow>

		const float lightMinRoughness = 0.015;
		const float lightRoughness = lightMinRoughness + roughness * (1.0 / (1.0 - lightMinRoughness));

		float3 punctualDiff, punctualSpec;
		VerusLit(normalWV, dirToLightWV, dirToEyeWV, tangentWV,
			gBuffer0Sam.rgb, sssColor,
			lightRoughness, metallic, roughDiffuse, wrapDiffuse, anisoSpec,
			punctualDiff, punctualSpec);

		so.target1.rgb = punctualDiff * lightColor * shadowMask * lightFalloffWithCone;
		so.target2.rgb = punctualSpec * lightColor * shadowMask * saturate(lightFalloffWithCone * 4.0);

		so.target1.rgb = SaturateHDR(so.target1.rgb);
		so.target2.rgb = SaturateHDR(so.target2.rgb);

#if defined(DEF_OMNI) || defined(DEF_SPOT)
		const float assumedAlbedo = 0.1;
		so.target0.rgb = assumedAlbedo * lightColor * lightFalloffWithCone;
		so.target0.rgb = SaturateHDR(so.target0.rgb);
#endif
	}
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	else
	{
		discard;
	}
#endif

	return so;
}
#endif

//@main:#InstancedDir  INSTANCED DIR
//@main:#InstancedOmni INSTANCED OMNI
//@main:#InstancedSpot INSTANCED SPOT
