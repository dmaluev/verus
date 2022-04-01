// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "SimpleMesh.inc.hlsl"

ConstantBuffer<UB_SimplePerFrame>      g_ubSimplePerFrame      : register(b0, space0);
ConstantBuffer<UB_SimplePerMaterialFS> g_ubSimplePerMaterialFS : register(b0, space1);
ConstantBuffer<UB_SimplePerMeshVS>     g_ubSimplePerMeshVS     : register(b0, space2);
ConstantBuffer<UB_SimpleSkeletonVS>    g_ubSimpleSkeletonVS    : register(b0, space3);
VK_PUSH_CONSTANT
ConstantBuffer<UB_SimplePerObject>     g_ubSimplePerObject     : register(b0, space4);

Texture2D              g_texA         : register(t1, space1);
SamplerState           g_samA         : register(s1, space1);
Texture2D              g_texX         : register(t2, space1);
SamplerState           g_samX         : register(s2, space1);
Texture2D              g_texShadowCmp : register(t3, space1);
SamplerComparisonState g_samShadowCmp : register(s3, space1);

struct VSI
{
	// Binding 0:
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int2 tc0   : TEXCOORD0;
	// Binding 1:
#ifdef DEF_SKINNED
	VK_LOCATION_BLENDWEIGHTS int4 bw : BLENDWEIGHTS;
	VK_LOCATION_BLENDINDICES int4 bi : BLENDINDICES;
#endif
	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos         : SV_Position;
	float4 color0      : COLOR0;
	float2 tc0         : TEXCOORD0;
	float4 posW_depth  : TEXCOORD1;
#if !defined(DEF_TEXTURE) && !defined(DEF_BAKE_AO)
	float3 nrmW        : TEXCOORD2;
	float3 dirToEye    : TEXCOORD3;
#endif
	float clipDistance : SV_ClipDistance;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 eyePos = g_ubSimplePerFrame._eyePos_clipDistanceOffset.xyz;
	const float clipDistanceOffset = g_ubSimplePerFrame._eyePos_clipDistanceOffset.w;

	// Dequantize:
	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubSimplePerMeshVS._posDeqScale.xyz, g_ubSimplePerMeshVS._posDeqBias.xyz);
	const float2 inTc0 = DequantizeUsingDeq2D(si.tc0, g_ubSimplePerMeshVS._tc0DeqScaleBias.xy, g_ubSimplePerMeshVS._tc0DeqScaleBias.zw);
	const float3 inNrm = si.nrm.xyz;

	// <TheMatrix>
#ifdef DEF_INSTANCED
	mataff matW = GetInstMatrix(
		si.matPart0,
		si.matPart1,
		si.matPart2);
	const float4 userColor = si.instData;
#else
	mataff matW = g_ubSimplePerObject._matW;
	const float4 userColor = g_ubSimplePerObject._userColor;
#endif

	const float3 pos = inPos;
	// </TheMatrix>

	float3 posW;
	float3 nrmW;
	{
#if defined(DEF_SKINNED)
		const float4 weights = si.bw * (1.0 / 255.0);
		const int4 indices = si.bi;

		float3 posSkinned = 0.0;
		float3 nrmSkinned = 0.0;

		for (int i = 0; i < 4; ++i)
		{
			const mataff matBone = g_ubSimpleSkeletonVS._vMatBones[indices[i]];
			posSkinned += mul(float4(pos, 1), matBone).xyz * weights[i];
			nrmSkinned += mul(inNrm, (float3x3)matBone) * weights[i];
		}

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmW = mul(nrmSkinned, (float3x3)matW);
#elif defined(DEF_ROBOTIC)
		const int index = si.pos.w;

		const mataff matBone = g_ubSimpleSkeletonVS._vMatBones[index];

		const float3 posSkinned = mul(float4(pos, 1), matBone).xyz;
		nrmW = mul(inNrm, (float3x3)matBone);

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmW = mul(nrmW, (float3x3)matW);
#else
		posW = mul(float4(pos, 1), matW).xyz;
		nrmW = mul(inNrm, (float3x3)matW);
#endif
	}

	so.pos = mul(float4(posW, 1), g_ubSimplePerFrame._matVP);
	so.posW_depth = float4(posW, so.pos.w);
	so.color0 = userColor;
	so.tc0 = inTc0;
#if !defined(DEF_TEXTURE) && !defined(DEF_BAKE_AO)
	so.nrmW = nrmW;
	so.dirToEye = eyePos - posW;
#endif
	so.clipDistance = posW.y + clipDistanceOffset;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#if defined(DEF_TEXTURE)
	const float4 albedo = g_texA.Sample(g_samA, si.tc0);
	so.color.rgb = albedo.rgb * si.color0.rgb;
	so.color.rgb = SaturateHDR(so.color.rgb);
	so.color.a = 1.0;
#elif defined(DEF_BAKE_AO)
	// <Material>
	const float4 mm_tc0ScaleBias = g_ubSimplePerMaterialFS._tc0ScaleBias;
	// </Material>

	const float2 tc0 = si.tc0 * mm_tc0ScaleBias.xy + mm_tc0ScaleBias.zw;

	// <Albedo>
	const float4 albedo = g_texA.Sample(g_samA, tc0);
	// </Albedo>

	so.color.rgb = albedo.rgb * si.color0.rgb;

	const float fog = saturate(si.posW_depth.w * g_ubSimplePerFrame._fogColor.a);
	so.color.rgb = lerp(so.color.rgb, g_ubSimplePerFrame._fogColor.rgb, fog);

	so.color.rgb = SaturateHDR(so.color.rgb);
	so.color.a = 1.0;

	clip(albedo.a - 0.5);
#else
	// <Material>
	const float mm_emission = g_ubSimplePerMaterialFS._anisoSpecDir_detail_emission.w;
	const float4 mm_solidA = g_ubSimplePerMaterialFS._solidA;
	const float4 mm_solidX = g_ubSimplePerMaterialFS._solidX;
	const float4 mm_tc0ScaleBias = g_ubSimplePerMaterialFS._tc0ScaleBias;
	const float4 mm_userPick = g_ubSimplePerMaterialFS._userPick;
	const float2 mm_xMetallicScaleBias = g_ubSimplePerMaterialFS._xAnisoSpecScaleBias_xMetallicScaleBias.zw;
	const float2 mm_xRoughnessScaleBias = g_ubSimplePerMaterialFS._xRoughnessScaleBias_xWrapDiffuseScaleBias.xy;
	// </Material>

	const float2 tc0 = si.tc0 * mm_tc0ScaleBias.xy + mm_tc0ScaleBias.zw;

	// <Albedo>
	float4 albedo;
	{
		const float texEnabled = 1.0 - floor(mm_solidA.a);
		float4 albedoSam = g_texA.Sample(g_samA, tc0 * texEnabled);
		albedoSam.rgb = lerp(albedoSam.rgb, mm_solidA.rgb, mm_solidA.a);

		albedo = albedoSam;
	}
	const float gray = Grayscale(albedo.rgb);
	// </Albedo>

	// <Pick>
	const float userMask = PickAlpha(albedo.rgb, mm_userPick, 16.0);
	// </Pick>

	albedo.rgb = lerp(albedo.rgb, Overlay(gray, si.color0.rgb), userMask * si.color0.a);

	// <X>
	float occlusion;
	float roughness;
	float metallic;
	float emission;
	{
		const float texEnabled = 1.0 - floor(mm_solidX.a);
		float4 xSam = g_texX.Sample(g_samX, tc0 * texEnabled);
		xSam.rgb = lerp(xSam.rgb, mm_solidX.rgb, mm_solidX.a);

		occlusion = xSam.r;
		roughness = xSam.g;
		const float2 metallic_wrapDiffuse = CleanMutexChannels(SeparateMutexChannels(xSam.b));
		const float2 emission_anisoSpec = CleanMutexChannels(SeparateMutexChannels(xSam.a));
		metallic = metallic_wrapDiffuse.r;
		emission = emission_anisoSpec.r;

		// Scale & bias:
		roughness = roughness * mm_xRoughnessScaleBias.x + mm_xRoughnessScaleBias.y;
		metallic = metallic * mm_xMetallicScaleBias.x + mm_xMetallicScaleBias.y;
		emission *= mm_emission;
	}
	// </X>

	const float3 normal = normalize(si.nrmW);
	const float3 dirToLight = g_ubSimplePerFrame._dirToSun.xyz;
	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	// <Shadow>
	float shadowMask;
	{
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, dirToLight, depth);
		shadowMask = SimpleShadowMapCSM(
			g_texShadowCmp,
			g_samShadowCmp,
			si.posW_depth.xyz,
			posForShadow,
			g_ubSimplePerFrame._matShadow,
			g_ubSimplePerFrame._matShadowCSM1,
			g_ubSimplePerFrame._matShadowCSM2,
			g_ubSimplePerFrame._matShadowCSM3,
			g_ubSimplePerFrame._matScreenCSM,
			g_ubSimplePerFrame._csmSplitRanges,
			g_ubSimplePerFrame._shadowConfig);
	}
	// </Shadow>

	float3 punctualDiff, punctualSpec;
	VerusSimpleLit(normal, dirToLight, dirToEye,
		albedo.rgb,
		roughness, metallic,
		punctualDiff, punctualSpec);

	punctualDiff *= g_ubSimplePerFrame._sunColor.rgb * shadowMask;
	punctualSpec *= g_ubSimplePerFrame._sunColor.rgb * shadowMask;

	so.color.rgb = albedo.rgb * (g_ubSimplePerFrame._ambientColor.rgb * occlusion + punctualDiff + emission) + punctualSpec;
	so.color.a = 1.0;

	const float fog = ComputeFog(depth, g_ubSimplePerFrame._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimplePerFrame._fogColor.rgb, fog);

	so.color.rgb = SaturateHDR(so.color.rgb);

	clip(albedo.a - 0.5);
#endif

	return so;
}
#endif

//@main:#
//@main:#Instanced INSTANCED
//@main:#Robotic   ROBOTIC
//@main:#Skinned   SKINNED

//@main:#Texture          TEXTURE
//@main:#TextureInstanced TEXTURE INSTANCED
//@main:#TextureRobotic   TEXTURE ROBOTIC
//@main:#TextureSkinned   TEXTURE SKINNED

//@main:#BakeAO          BAKE_AO
//@main:#BakeAOInstanced BAKE_AO INSTANCED
//@main:#BakeAORobotic   BAKE_AO ROBOTIC
//@main:#BakeAOSkinned   BAKE_AO SKINNED
