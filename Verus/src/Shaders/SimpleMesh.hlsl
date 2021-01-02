// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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

Texture2D              g_texAlbedo    : register(t1, space1);
SamplerState           g_samAlbedo    : register(s1, space1);
Texture2D              g_texShadowCmp : register(t2, space1);
SamplerComparisonState g_samShadowCmp : register(s2, space1);

struct VSI
{
	// Binding 0:
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
	// Binding 1:
#ifdef DEF_SKINNED
	VK_LOCATION_BLENDWEIGHTS int4 bw : BLENDWEIGHTS;
	VK_LOCATION_BLENDINDICES int4 bi : BLENDINDICES;
#endif
	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos        : SV_Position;
	float4 color0     : COLOR0;
	float2 tc0        : TEXCOORD0;
	float4 posW_depth : TEXCOORD1;
#if !defined(DEF_TEXTURE)
	float3 nrmW       : TEXCOORD2;
	float3 dirToEye   : TEXCOORD3;
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

	// World matrix, instance data:
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

	// Dequantize:
	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubSimplePerMeshVS._posDeqScale.xyz, g_ubSimplePerMeshVS._posDeqBias.xyz);
	const float2 intactTc0 = DequantizeUsingDeq2D(si.tc0.xy, g_ubSimplePerMeshVS._tc0DeqScaleBias.xy, g_ubSimplePerMeshVS._tc0DeqScaleBias.zw);
	const float3 intactNrm = si.nrm.xyz;

	const float3 pos = intactPos;

	float3 posW;
	float3 nrmW;
	{
#ifdef DEF_SKINNED
		const float4 weights = si.bw * (1.f / 255.f);
		const float4 indices = si.bi;

		float3 posSkinned = 0.f;
		float3 nrmSkinned = 0.f;

		for (int i = 0; i < 4; ++i)
		{
			const mataff matBone = g_ubSimpleSkeletonVS._vMatBones[indices[i]];
			posSkinned += mul(float4(pos, 1), matBone).xyz * weights[i];
			nrmSkinned += mul(intactNrm, (float3x3)matBone) * weights[i];
		}

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmW = mul(nrmSkinned, (float3x3)matW);
#elif DEF_ROBOTIC
		const int index = si.pos.w;

		const mataff matBone = g_ubSimpleSkeletonVS._vMatBones[index];

		const float3 posSkinned = mul(float4(pos, 1), matBone).xyz;
		nrmW = mul(intactNrm, (float3x3)matBone);

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmW = mul(nrmW, (float3x3)matW);
#else
		posW = mul(float4(pos, 1), matW).xyz;
		nrmW = mul(intactNrm, (float3x3)matW);
#endif
	}

	so.pos = mul(float4(posW, 1), g_ubSimplePerFrame._matVP);
	so.posW_depth = float4(posW, so.pos.z);
	so.color0 = userColor;
	so.tc0 = intactTc0;
#if !defined(DEF_TEXTURE)
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

#ifdef DEF_TEXTURE
	const float4 rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, si.tc0);
	so.color.rgb = rawAlbedo.rgb * si.color0.rgb;
	so.color.rgb = ToSafeHDR(so.color.rgb);
	so.color.a = 1.f;
#else
	// <Material>
	const float2 mm_alphaSwitch = g_ubSimplePerMaterialFS._alphaSwitch_anisoSpecDir.xy;
	const float mm_emission = g_ubSimplePerMaterialFS._detail_emission_gloss_hairDesat.y;
	const float4 mm_emissionPick = g_ubSimplePerMaterialFS._emissionPick;
	const float4 mm_eyePick = g_ubSimplePerMaterialFS._eyePick;
	const float mm_gloss = g_ubSimplePerMaterialFS._detail_emission_gloss_hairDesat.z;
	const float4 mm_glossPick = g_ubSimplePerMaterialFS._glossPick;
	const float2 mm_glossScaleBias = g_ubSimplePerMaterialFS._glossScaleBias_specScaleBias.xy;
	const float mm_hairDesat = g_ubSimplePerMaterialFS._detail_emission_gloss_hairDesat.w;
	const float4 mm_hairPick = g_ubSimplePerMaterialFS._hairPick;
	const float2 mm_lamScaleBias = g_ubSimplePerMaterialFS._lamScaleBias_lightPass_motionBlur.xy;
	const float4 mm_skinPick = g_ubSimplePerMaterialFS._skinPick;
	const float2 mm_specScaleBias = g_ubSimplePerMaterialFS._glossScaleBias_specScaleBias.zw;
	const float4 mm_tc0ScaleBias = g_ubSimplePerMaterialFS._tc0ScaleBias;
	const float4 mm_texEnableAlbedo = g_ubSimplePerMaterialFS._texEnableAlbedo;
	const float4 mm_userColor = g_ubSimplePerMaterialFS._userColor;
	const float4 mm_userPick = g_ubSimplePerMaterialFS._userPick;
	// </Material>

	const float2 tc0 = si.tc0 * mm_tc0ScaleBias.xy + mm_tc0ScaleBias.zw;

	// <Albedo>
	float4 rawAlbedo;
	{
		const float texEnableAlbedoAlpha = ceil(mm_texEnableAlbedo.a);
		rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, tc0 * texEnableAlbedoAlpha);
		rawAlbedo.rgb = lerp(mm_texEnableAlbedo.rgb, rawAlbedo.rgb, mm_texEnableAlbedo.a);
	}
	const float gray = Grayscale(rawAlbedo.rgb);
	// </Albedo>

	// <Pick>
	const float2 alpha_spec = AlphaSwitch(rawAlbedo, tc0, mm_alphaSwitch);
	const float emitAlpha = PickAlpha(rawAlbedo.rgb, mm_emissionPick, 16.f);
	const float eyeAlpha = PickAlphaRound(mm_eyePick, tc0);
	const float glossAlpha = PickAlpha(rawAlbedo.rgb, mm_glossPick, 32.f);
	const float hairAlpha = round(PickAlpha(rawAlbedo.rgb, mm_hairPick, 16.f));
	const float skinAlpha = PickAlpha(rawAlbedo.rgb, mm_skinPick, 16.f);
	const float userAlpha = PickAlpha(rawAlbedo.rgb, mm_userPick, 16.f);
	// </Pick>

	rawAlbedo.rgb = lerp(rawAlbedo.rgb, Overlay(gray, si.color0.rgb), userAlpha * si.color0.a);
	const float3 hairAlbedo = Overlay(alpha_spec.y, Desaturate(rawAlbedo.rgb, hairAlpha * mm_hairDesat));
	const float3 realAlbedo = ToRealAlbedo(lerp(rawAlbedo.rgb, hairAlbedo, hairAlpha));
	const float specMask = saturate(alpha_spec.y * mm_specScaleBias.x + mm_specScaleBias.y);

	// <Gloss>
	float gloss = lerp(4.f, 16.f, alpha_spec.y) * mm_glossScaleBias.x + mm_glossScaleBias.y;
	gloss = lerp(gloss, mm_gloss, glossAlpha);
	gloss = lerp(gloss, 4.f + alpha_spec.y, skinAlpha);
	gloss = lerp(gloss, 0.f, eyeAlpha);
	const float gloss4K = gloss * gloss;
	// </Gloss>

	// <LambertianScaleBias>
	const float2 lamScaleBias = lerp(mm_lamScaleBias, float2(1, 0.45f), skinAlpha);
	// </LambertianScaleBias>

	const float3 normal = normalize(si.nrmW);
	const float3 dirToEye = normalize(si.dirToEye.xyz);
	const float depth = si.posW_depth.w;

	// <Shadow>
	float shadowMask;
	{
		float4 shadowConfig = g_ubSimplePerFrame._shadowConfig;
		const float lamBiasMask = saturate(lamScaleBias.y * shadowConfig.y);
		shadowConfig.y = 1.f - lamBiasMask; // Keep penumbra blurry.
		const float3 posForShadow = AdjustPosForShadow(si.posW_depth.xyz, normal, g_ubSimplePerFrame._dirToSun.xyz, depth);
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
			shadowConfig);
	}
	// </Shadow>

	const float4 litRet = VerusLit(g_ubSimplePerFrame._dirToSun.xyz, normal, dirToEye,
		gloss4K,
		lamScaleBias,
		float4(0, 0, 1, 0));

	const float3 diffColor = litRet.y * g_ubSimplePerFrame._sunColor.rgb * shadowMask + g_ubSimplePerFrame._ambientColor.rgb;
	const float3 specColor = litRet.z * g_ubSimplePerFrame._sunColor.rgb * shadowMask * specMask;
	const float emission = emitAlpha * mm_emission;

	so.color.rgb = realAlbedo * (diffColor + emission) + specColor;
	so.color.a = 1.f;

	const float fog = ComputeFog(depth, g_ubSimplePerFrame._fogColor.a, si.posW_depth.y);
	so.color.rgb = lerp(so.color.rgb, g_ubSimplePerFrame._fogColor.rgb, fog);

	so.color.rgb = ToSafeHDR(so.color.rgb);

	clip(alpha_spec.x - 0.5f);
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
