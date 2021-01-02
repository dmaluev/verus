// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "DS.inc.hlsl"

ConstantBuffer<UB_PerFrame>   g_ubPerFrame   : register(b0, space0);
ConstantBuffer<UB_TexturesFS> g_ubTexturesFS : register(b0, space1);
ConstantBuffer<UB_PerMeshVS>  g_ubPerMeshVS  : register(b0, space2);
ConstantBuffer<UB_ShadowFS>   g_ubShadowFS   : register(b0, space3);
VK_PUSH_CONSTANT
ConstantBuffer<UB_PerObject>  g_ubPerObject  : register(b0, space4);

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, t1, s1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, t2, s2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, t3, s3, space1);
VK_SUBPASS_INPUT(3, g_texDepth, g_samDepth, t4, s4, space1);
Texture2D              g_texShadowCmp : register(t5, space1);
SamplerComparisonState g_samShadowCmp : register(s5, space1);
Texture2D              g_texShadow    : register(t6, space1);
SamplerState           g_samShadow    : register(s6, space1);

struct VSI
{
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos                         : SV_Position;
	float4 clipSpacePos                : TEXCOORD0;
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	float3 radius_radiusSq_invRadiusSq : TEXCOORD1;
	float3 lightPosWV                  : TEXCOORD2;
#endif
#if defined(DEF_DIR) || defined(DEF_SPOT)
	float4 lightDirWV_invConeDelta     : TEXCOORD3;
#endif
	float4 color_coneOut               : TEXCOORD4;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// World matrix, instance data:
#ifdef DEF_INSTANCED
	const mataff matW = GetInstMatrix(
		si.matPart0,
		si.matPart1,
		si.matPart2);
	const float3 color = si.instData.rgb;
	const float coneIn = si.instData.a;
#else
	const mataff matW = g_ubPerObject._matW;
	const float3 color = g_ubPerObject.rgb;
	const float coneIn = g_ubPerObject.a;
#endif

	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubPerFrame._matV));

	const float3x3 matW33 = (float3x3)matW;
	const float3x3 matWV33 = (float3x3)matWV;

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);

#ifdef DEF_DIR
	so.pos = float4(mul(float4(intactPos, 1), g_ubPerFrame._matQuad), 1);
	so.clipSpacePos = float4(intactPos, 1);
#else
	const float3 posW = mul(float4(intactPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_ubPerFrame._matVP);
	so.clipSpacePos = so.pos;
#endif

	// <MoreLightParams>
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float3 posUnit = mul(float3(0, 0, 1), matW33); // Need to know the scale.
	so.radius_radiusSq_invRadiusSq.y = dot(posUnit, posUnit);
	so.radius_radiusSq_invRadiusSq.x = sqrt(so.radius_radiusSq_invRadiusSq.y);
	so.radius_radiusSq_invRadiusSq.z = 1.f / so.radius_radiusSq_invRadiusSq.y;
	const float4 posOrigin = float4(0, 0, 0, 1);
	so.lightPosWV = mul(posOrigin, matWV).xyz;
#endif
	so.color_coneOut = float4(color, 0);
#ifdef DEF_DIR
	const float3 posUnit = mul(float3(0, 0, 1), matWV33); // Assume no scale in matWV33.
	so.lightDirWV_invConeDelta = float4(posUnit, 1);
#elif DEF_SPOT
	so.lightDirWV_invConeDelta.xyz = normalize(mul(float3(0, 0, 1), matWV33));
	const float3 posCone = mul(float3(0, 1, 1), matW33);
	const float3 dirCone = normalize(posCone);
	const float3 dirUnit = normalize(posUnit);
	const float coneOut = dot(dirUnit, dirCone);
	const float invConeDelta = 1.f / (coneIn - coneOut);
	so.lightDirWV_invConeDelta.w = invConeDelta;
	so.color_coneOut.a = coneOut;
#endif
	// </MoreLightParams>

	return so;
}
#endif

#ifdef _FS
DS_ACC_FSO mainFS(VSO si)
{
	DS_ACC_FSO so;

#ifdef DEF_DIR
	const float3 ndcPos = si.clipSpacePos.xyz;
#else
	const float3 ndcPos = si.clipSpacePos.xyz / si.clipSpacePos.w;
#endif

	const float2 tc0 = mul(float4(ndcPos.xy, 0, 1), g_ubPerFrame._matToUV).xy;

	// For Omni & Spot: light's radius and position:
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float radius = si.radius_radiusSq_invRadiusSq.x;
	const float radiusSq = si.radius_radiusSq_invRadiusSq.y;
	const float invRadiusSq = si.radius_radiusSq_invRadiusSq.z;
	const float3 lightPosWV = si.lightPosWV;
#endif

	// For Dir & Spot: light's direction & cone:
#ifdef DEF_DIR
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
#elif DEF_SPOT
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
	const float invConeDelta = si.lightDirWV_invConeDelta.w;
	const float coneOut = si.color_coneOut.a;
#endif

	// GBuffer1:
	const float depth = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, tc0).r;
	const float3 posWV = DS_GetPosition(depth, g_ubPerFrame._matInvP, ndcPos.xy);

	so.target0 = 0.f;
	so.target1 = 0.f;

#if defined(DEF_OMNI) || defined(DEF_SPOT)
	if (posWV.z <= lightPosWV.z + radius)
#endif
	{
		// Light's diffuse & specular color:
		const float3 lightColor = si.color_coneOut.rgb;

		// GBuffer0 {albedo, specMask}:
		const float4 rawGBuffer0 = VK_SUBPASS_LOAD(g_texGBuffer0, g_samGBuffer0, tc0);
		const float specMask = rawGBuffer0.a;
		const float3 dirToEyeWV = normalize(-posWV);

		// GBuffer2 {normal, emission|skinMask, motionBlurMask}:
		const float4 rawGBuffer1 = VK_SUBPASS_LOAD(g_texGBuffer1, g_samGBuffer1, tc0);
		const float3 normalWV = DS_GetNormal(rawGBuffer1);
		const float2 emission = DS_GetEmission(rawGBuffer1);

		// GBuffer3 {lamScaleBias, metalMask|hairMask, gloss}:
		const float4 rawGBuffer2 = VK_SUBPASS_LOAD(g_texGBuffer2, g_samGBuffer2, tc0);
		const float3 anisoWV = DS_GetAnisoSpec(rawGBuffer2);
		const float2 lamScaleBias = DS_GetLamScaleBias(rawGBuffer2);
		const float2 metalMask = DS_GetMetallicity(rawGBuffer2);
		const float gloss64 = rawGBuffer2.a * 64.f;

		// Special:
		const float skinMask = emission.y;
		const float hairMask = metalMask.y;
		const float eyeMask = saturate(1.f - gloss64);
		const float eyeGloss = 48.f;
		const float2 lamScaleBiasWithHair = lerp(lamScaleBias, float2(1, 0.4f), hairMask);

		const float gloss4K = lerp(gloss64 * gloss64, eyeGloss * eyeGloss, eyeMask);

#ifdef DEF_DIR
		const float3 dirToLightWV = -lightDirWV;
		const float lightFalloff = 1.f;
#else
		const float3 toLightWV = lightPosWV - posWV;
		const float3 dirToLightWV = normalize(toLightWV);
		const float distToLightSq = dot(toLightWV, toLightWV);
		const float lightFalloff = ComputePointLightIntensity(distToLightSq, radiusSq, invRadiusSq);
#endif
#ifdef DEF_SPOT // Extra step for spot light:
		const float coneIntensity = ComputeSpotLightConeIntensity(dirToLightWV, lightDirWV, coneOut, invConeDelta);
#else
		const float coneIntensity = 1.f;
#endif

		const float4 litRet = VerusLit(dirToLightWV, normalWV, dirToEyeWV,
			lerp(gloss4K * (2.f - lightFalloff), 12.f, hairMask),
			lamScaleBiasWithHair,
			float4(anisoWV, hairMask));

		// <Shadow>
		float shadowMask = 1.f;
		{
#ifdef DEF_DIR
			const float lightPassOffset = saturate((lamScaleBiasWithHair.y - 0.5f) * 5.f) * 2.f;
			float4 shadowConfig = g_ubShadowFS._shadowConfig;
			const float lamBiasMask = saturate(lamScaleBiasWithHair.y * shadowConfig.y);
			shadowConfig.y = 1.f - lamBiasMask; // Keep penumbra blurry.
			const float3 posForShadow = AdjustPosForShadow(posWV, normalWV, dirToLightWV, -posWV.z, lightPassOffset);
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

		const float lightFalloffWithCone = lightFalloff * coneIntensity;
		const float diffLightMask = lightFalloffWithCone * shadowMask;
		const float specLightMask = saturate(lightFalloffWithCone * 2.f) * shadowMask;

		// Subsurface scattering effect for front & back faces:
		const float3 frontSkinSSSColor = float3(1.6f, 1.2f, 0.3f);
		const float3 backSkinSSSColor = float3(1.6f, 0.8f, 0.5f);
		const float frontSkinSSSMask = 1.f - litRet.y;
		const float3 lightFaceColor = lightColor * lerp(1.f, frontSkinSSSColor, skinMask * frontSkinSSSMask * frontSkinSSSMask);
		const float3 lightColorSSS = lerp(lightFaceColor, lightColor * backSkinSSSColor, skinMask * litRet.x);

		const float3 metalColor = ToMetalColor(rawGBuffer0.rgb);
		const float3 specColor = saturate(lerp(1.f, metalColor, saturate(metalMask.x + (hairMask - eyeMask) * 0.4f)) + float3(-0.03f, 0.f, 0.05f) * skinMask);

		const float3 maxDiff = lightColorSSS * diffLightMask;
		const float3 maxSpec = lightColorSSS * specLightMask * specColor * specMask;

		so.target0.rgb = ToSafeHDR(maxDiff * litRet.y);
		so.target1.rgb = ToSafeHDR(maxSpec * litRet.z);
		so.target0.a = 1.f;
		so.target1.a = 1.f;
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
