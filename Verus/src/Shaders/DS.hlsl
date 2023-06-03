// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "DS.inc.hlsl"

CBUFFER(0, UB_View, g_ubView)
CBUFFER(1, UB_SubpassFS, g_ubSubpassFS)
CBUFFER(2, UB_ShadowFS, g_ubShadowFS)
CBUFFER(3, UB_MeshVS, g_ubMeshVS)
SBUFFER(4, SB_InstanceData, g_sbInstanceData, t7)
VK_PUSH_CONSTANT
CBUFFER(5, UB_Object, g_ubObject)

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, 1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, 2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, 3, space1);
VK_SUBPASS_INPUT(3, g_texGBuffer3, g_samGBuffer3, 4, space1);
VK_SUBPASS_INPUT(4, g_texDepth, g_samDepth, 5, space1);
Texture2D              g_texShadowCmp : REG(t1, space2, t5);
SamplerComparisonState g_samShadowCmp : REG(s1, space2, s5);
Texture2D              g_texShadow    : REG(t2, space2, t6);
SamplerState           g_samShadow    : REG(s2, space2, s6);

struct VSI
{
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int2 tc0   : TEXCOORD0;
	uint instanceID                 : SV_InstanceID;
	_PER_INSTANCE_DATA_0
};

struct VSO
{
	float4 pos                                       : SV_Position;
	float4 clipSpacePos                              : TEXCOORD0;
	float4 lightDirWV_invConeDelta                   : TEXCOORD1;
#if defined(DEF_OMNI) || defined(DEF_SPOT) // Omni and spot have position and radius.
	float4 lightPosWV_lampRadius                     : TEXCOORD2;
	float4 radius_radiusSq_invRadiusSq_invLampRadius : TEXCOORD3;
#endif
	float4 color_coneOut                             : TEXCOORD4;
	uint instanceID                                  : SV_InstanceID;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubMeshVS._posDeqScale.xyz, g_ubMeshVS._posDeqBias.xyz);

	// <TheMatrix>
#ifdef DEF_INSTANCED
	const mataff matW = GetInstMatrix(
		si.pid0_matPart0,
		si.pid0_matPart1,
		si.pid0_matPart2);
	const float3 color = si.pid0_instData.rgb;
	const float coneIn = si.pid0_instData.a;
#else
	const mataff matW = g_ubObject._matW;
	const float3 color = g_ubObject._color.rgb;
	const float coneIn = g_ubObject._color.a;
#endif
	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubView._matV));
	const float3x3 matWV33 = (float3x3)matWV;
	// </TheMatrix>

#ifdef DEF_DIR // Fullscreen quad?
	so.pos = float4(inPos, 1);
#else
	const float3 posW = mul(float4(inPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_ubView._matVP);
#endif
	so.clipSpacePos = so.pos;

	// <MoreLightParams>
	so.color_coneOut = float4(color, 0);
	const float3 scaledFrontDir = mul(float3(0, 0, 1), matWV33);
#ifdef DEF_DIR
	{
		so.lightDirWV_invConeDelta = float4(scaledFrontDir, 1); // Assume not scaled.
	}
#elif defined(DEF_OMNI)
	{
		so.lightDirWV_invConeDelta = float4(normalize(scaledFrontDir), 1); // Assume scaled.
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
		so.lightPosWV_lampRadius.xyz = mul(float4(0, 0, 0, 1), matWV).xyz;
		so.radius_radiusSq_invRadiusSq_invLampRadius.y = dot(scaledFrontDir, scaledFrontDir);
		so.radius_radiusSq_invRadiusSq_invLampRadius.x = sqrt(so.radius_radiusSq_invRadiusSq_invLampRadius.y);
		so.radius_radiusSq_invRadiusSq_invLampRadius.z = 1.0 / so.radius_radiusSq_invRadiusSq_invLampRadius.y;

		const float lampRadius = ComputeLampRadius(so.radius_radiusSq_invRadiusSq_invLampRadius.x, color);
		so.lightPosWV_lampRadius.w = lampRadius;
		so.radius_radiusSq_invRadiusSq_invLampRadius.w = 1.0 / lampRadius;
	}
#endif
	// </MoreLightParams>

	so.instanceID = si.instanceID;

	return so;
}
#endif

#ifdef _FS
[earlydepthstencil]
DS_ACC_FSO mainFS(VSO si)
{
	DS_ACC_FSO so;
	DS_Reset(so);

#ifdef DEF_DIR
	const float3 ndcPos = si.clipSpacePos.xyz;
#else
	const float3 ndcPos = si.clipSpacePos.xyz / si.clipSpacePos.w;
#endif
	const float2 tc0 = mul(float4(ndcPos.xy, 0, 1), g_ubView._matToUV).xy *
		g_ubView._tcViewScaleBias.xy + g_ubView._tcViewScaleBias.zw;

	// Direction and cone shape for spot:
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
#ifdef DEF_SPOT
	const float invConeDelta = si.lightDirWV_invConeDelta.w;
	const float coneOut = si.color_coneOut.a;
#endif

	// Omni and spot have position, radius and size:
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float3 lightPosWV = si.lightPosWV_lampRadius.xyz;
	const float radius = si.radius_radiusSq_invRadiusSq_invLampRadius.x;
	const float radiusSq = si.radius_radiusSq_invRadiusSq_invLampRadius.y;
	const float invRadiusSq = si.radius_radiusSq_invRadiusSq_invLampRadius.z;
	const float lampRadius = si.lightPosWV_lampRadius.w;
	const float invLampRadius = si.radius_radiusSq_invRadiusSq_invLampRadius.w;
#endif

	// Depth:
	const float depthSam = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, tc0).r;
	const float3 posWV = DS_GetPosition(depthSam, g_ubView._matInvP, ndcPos.xy);

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
		const float distToLight = length(toLightWV);
		const float distToLightSq = distToLight * distToLight;
		const float3 dirToLightWV = toLightWV / distToLight;
		const float lightFalloff = ComputePointLightIntensity(distToLightSq, radiusSq, invRadiusSq);
#endif
#ifdef DEF_SPOT // Extra step for spot light:
		const float coneIntensity = ComputeSpotLightConeIntensity(dirToLightWV, lightDirWV, coneOut, invConeDelta) *
			saturate(distToLight * invLampRadius * 10.0 - 10.0);
#else
		const float coneIntensity = 1.0; // No cone.
#endif
		const float lightFalloffWithCone = lightFalloff * coneIntensity;
		const float distToEye = length(posWV);
		const float3 dirToEyeWV = -posWV / distToEye;
		const float3 lightColor = (si.color_coneOut.r >= 0.0) ? si.color_coneOut.rgb : g_ubView._ambientColor.rgb;
		// </LightData>

		// <Shadow>
		float shadowMask = 1.0;
		{
			float4 shadowConfig = g_ubShadowFS._shadowConfig;
			shadowConfig.y = 1.0 - saturate(wrapDiffuse * shadowConfig.y);
			const float3 biasedPosWV = ApplyShadowBiasing(posWV, normalWV, dirToLightWV, -posWV.z, shadowConfig.z);
#ifdef DEF_DIR
			shadowMask = ShadowMapCSM(
				g_texShadowCmp,
				g_samShadowCmp,
				g_texShadow,
				g_samShadow,
				posWV,
				biasedPosWV,
				g_ubShadowFS._matShadow,
				g_ubShadowFS._matShadowCSM1,
				g_ubShadowFS._matShadowCSM2,
				g_ubShadowFS._matShadowCSM3,
				g_ubShadowFS._matScreenCSM,
				g_ubShadowFS._csmSliceBounds,
				shadowConfig);
#else
			const float dp = dot(lightDirWV, -dirToLightWV);
#ifdef DEF_OMNI
			if (dp > 0.5)
#elif DEF_SPOT
			if (dp > coneOut)
#endif
			{

#ifdef _VULKAN
				const uint realInstanceID = si.instanceID;
#else
				const uint realInstanceID = si.instanceID + g_ubShadowFS._shadowConfig.x;
#endif
				shadowMask = ShadowMapVSM(
					g_texShadow,
					g_samShadow,
					biasedPosWV,
					g_sbInstanceData[realInstanceID]._matShadow);
				shadowMask = max(shadowMask, g_ubShadowFS._shadowConfig.y);
#ifdef DEF_OMNI
				shadowMask = max(shadowMask, saturate(1.0 - (dp - 0.5) * (1.0 / 0.2071))); // Fade edges.
#endif
			}
#endif
		}
		// </Shadow>

#ifdef DEF_DIR
		const float lightMinRoughness = 0.015; // The sun.
#else
		const float distToReflected = distToLight + distToEye;
		const float lampRadiusFactor = lampRadius / (1.0 + distToReflected * distToReflected);
		const float lightMinRoughness = clamp(lampRadiusFactor * 64.0, 0.01, 0.6);
#endif
		const float lightRoughness = lightMinRoughness + (1.0 - lightMinRoughness) * roughness;

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
