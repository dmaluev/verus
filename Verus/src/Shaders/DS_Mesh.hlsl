// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibTessellation.hlsl"
#include "LibVertex.hlsl"
#include "DS_Mesh.inc.hlsl"

ConstantBuffer<UB_PerFrame>      g_ubPerFrame      : register(b0, space0);
ConstantBuffer<UB_PerMaterialFS> g_ubPerMaterialFS : register(b0, space1);
ConstantBuffer<UB_PerMeshVS>     g_ubPerMeshVS     : register(b0, space2);
ConstantBuffer<UB_SkeletonVS>    g_ubSkeletonVS    : register(b0, space3);
VK_PUSH_CONSTANT
ConstantBuffer<UB_PerObject>     g_ubPerObject     : register(b0, space4);

Texture2D    g_texAlbedo : register(t1, space1);
SamplerState g_samAlbedo : register(s1, space1);
Texture2D    g_texNormal : register(t2, space1);
SamplerState g_samNormal : register(s2, space1);
Texture2D    g_texDetail : register(t3, space1);
SamplerState g_samDetail : register(s3, space1);
Texture2D    g_texStrass : register(t4, space1);
SamplerState g_samStrass : register(s4, space1);

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
	// Binding 2:
	VK_LOCATION_TANGENT  float4 tan : TANGENT;
	VK_LOCATION_BINORMAL float4 bin : BINORMAL;
	// Binding 3:
#if 0
	VK_LOCATION(9)     int4 tc1     : TEXCOORD1;
	VK_LOCATION_COLOR0 float4 color : COLOR0;
#endif
	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos     : SV_Position;
	float2 tc0     : TEXCOORD0;
#if !defined(DEF_DEPTH)
	float4 color0  : COLOR0;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	float4 matTBN0 : TEXCOORD1;
	float4 matTBN1 : TEXCOORD2;
	float4 matTBN2 : TEXCOORD3;
#endif
#endif
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
	const float4 userColor = si.instData;
#else
	const mataff matW = g_ubPerObject._matW;
	const float4 userColor = g_ubPerObject._userColor;
#endif

	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubPerFrame._matV));

	// Dequantize:
	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);
	const float2 intactTc0 = DequantizeUsingDeq2D(si.tc0.xy, g_ubPerMeshVS._tc0DeqScaleBias.xy, g_ubPerMeshVS._tc0DeqScaleBias.zw);
	const float3 intactNrm = si.nrm.xyz;
	const float3 intactTan = si.tan.xyz;
	const float3 intactBin = si.bin.xyz;

#if defined(DEF_SKINNED) || defined(DEF_ROBOTIC)
	const float4 warpMask = float4(
		1,
		si.pos.w,
		si.tan.w,
		si.bin.w) * GetWarpScale();
	const float3 pos = ApplyWarp(intactPos, g_ubSkeletonVS._vWarpZones, warpMask);
#else
	const float3 pos = intactPos;
#endif

	float3 posW;
	float3 nrmWV;
	float3 tanWV;
	float3 binWV;
	{
#ifdef DEF_SKINNED
		const float4 weights = si.bw * (1.0 / 255.0);
		const float4 indices = si.bi;

		float3 posSkinned = 0.0;
		float3 nrmSkinned = 0.0;
		float3 tanSkinned = 0.0;
		float3 binSkinned = 0.0;

		for (int i = 0; i < 4; ++i)
		{
			const mataff matBone = g_ubSkeletonVS._vMatBones[indices[i]];
			posSkinned += mul(float4(pos, 1), matBone).xyz * weights[i];
			nrmSkinned += mul(intactNrm, (float3x3)matBone) * weights[i];
			tanSkinned += mul(intactTan, (float3x3)matBone) * weights[i];
			binSkinned += mul(intactBin, (float3x3)matBone) * weights[i];
		}

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmSkinned, (float3x3)matWV);
		tanWV = mul(tanSkinned, (float3x3)matWV);
		binWV = mul(binSkinned, (float3x3)matWV);
#elif DEF_ROBOTIC
		const int index = si.pos.w;

		const mataff matBone = g_ubSkeletonVS._vMatBones[index];

		const float3 posSkinned = mul(float4(pos, 1), matBone).xyz;
		nrmWV = mul(intactNrm, (float3x3)matBone);
		tanWV = mul(intactTan, (float3x3)matBone);
		binWV = mul(intactBin, (float3x3)matBone);

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmWV, (float3x3)matWV);
		tanWV = mul(tanWV, (float3x3)matWV);
		binWV = mul(binWV, (float3x3)matWV);
#else
		posW = mul(float4(pos, 1), matW).xyz;
		nrmWV = mul(intactNrm, (float3x3)matWV);
		tanWV = mul(intactTan, (float3x3)matWV);
		binWV = mul(intactBin, (float3x3)matWV);
#endif
	}

	so.pos = MulTessPos(float4(posW, 1), g_ubPerFrame._matV, g_ubPerFrame._matVP);
	so.tc0 = intactTc0;
#if !defined(DEF_DEPTH)
	so.color0 = userColor;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	so.matTBN0 = float4(tanWV, posW.x);
	so.matTBN1 = float4(binWV, posW.y);
	so.matTBN2 = float4(nrmWV, posW.z);
#endif
#endif

	return so;
}
#endif

_HSO_STRUCT;

#ifdef _HS
PCFO PatchConstFunc(const OutputPatch<HSO, 3> outputPatch)
{
	PCFO so;
	_HS_PCF_BODY(g_ubPerFrame._matP);
	return so;
}

[domain("tri")]
[maxtessfactor(7.0)]
[outputcontrolpoints(3)]
[outputtopology("triangle_cw")]
[partitioning(_PARTITION_METHOD)]
[patchconstantfunc("PatchConstFunc")]
HSO mainHS(InputPatch<VSO, 3> inputPatch, uint id : SV_OutputControlPointID)
{
	HSO so;

	_HS_PN_BODY(matTBN2, g_ubPerFrame._matP, g_ubPerFrame._viewportSize);

	_HS_COPY(pos);
	_HS_COPY(tc0);
#if !defined(DEF_DEPTH)
	_HS_COPY(color0);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_HS_COPY(matTBN0);
	_HS_COPY(matTBN1);
	_HS_COPY(matTBN2);
#endif
#endif

	return so;
}
#endif

#ifdef _DS
[domain("tri")]
VSO mainDS(_IN_DS)
{
	VSO so;

	_DS_INIT_SMOOTH_POS;

	so.pos = ApplyProjection(smoothPosWV, g_ubPerFrame._matP);
	_DS_COPY(tc0);
#if !defined(DEF_DEPTH)
	_DS_COPY(color0);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_DS_COPY(matTBN0);
	_DS_COPY(matTBN1);
	_DS_COPY(matTBN2);
#endif
#endif

	return so;
}
#endif

#ifdef _FS
#ifdef DEF_DEPTH
void mainFS(VSO si)
{
	const float rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, si.tc0).a;
	clip(rawAlbedo - 1.0 / 16.0);
}
#else
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

#ifdef DEF_SOLID_COLOR
	DS_SolidColor(so, si.color0.rgb);
#else
	_TBN_SPACE(
		si.matTBN0.xyz,
		si.matTBN1.xyz,
		si.matTBN2.xyz);
	const float3 rand = Rand(si.pos.xy);

	// <Material>
	const float2 mm_alphaSwitch = g_ubPerMaterialFS._alphaSwitch_anisoSpecDir.xy;
	const float2 mm_anisoSpecDir = g_ubPerMaterialFS._alphaSwitch_anisoSpecDir.zw;
	const float mm_detail = g_ubPerMaterialFS._detail_emission_gloss_hairDesat.x;
	const float2 mm_detailScale = g_ubPerMaterialFS._detailScale_strassScale.xy;
	const float mm_emission = g_ubPerMaterialFS._detail_emission_gloss_hairDesat.y;
	const float4 mm_emissionPick = g_ubPerMaterialFS._emissionPick;
	const float4 mm_eyePick = g_ubPerMaterialFS._eyePick;
	const float mm_gloss = g_ubPerMaterialFS._detail_emission_gloss_hairDesat.z;
	const float4 mm_glossPick = g_ubPerMaterialFS._glossPick;
	const float2 mm_glossScaleBias = g_ubPerMaterialFS._glossScaleBias_specScaleBias.xy;
	const float mm_hairDesat = g_ubPerMaterialFS._detail_emission_gloss_hairDesat.w;
	const float4 mm_hairPick = g_ubPerMaterialFS._hairPick;
	const float2 mm_lamScaleBias = g_ubPerMaterialFS._lamScaleBias_lightPass_motionBlur.xy;
	const float mm_lightPass = g_ubPerMaterialFS._lamScaleBias_lightPass_motionBlur.z;
	const float4 mm_metalPick = g_ubPerMaterialFS._metalPick;
	const float4 mm_skinPick = g_ubPerMaterialFS._skinPick;
	const float2 mm_specScaleBias = g_ubPerMaterialFS._glossScaleBias_specScaleBias.zw;
	const float2 mm_strassScale = g_ubPerMaterialFS._detailScale_strassScale.zw;
	const float4 mm_tc0ScaleBias = g_ubPerMaterialFS._tc0ScaleBias;
	const float4 mm_texEnableAlbedo = g_ubPerMaterialFS._texEnableAlbedo;
	const float4 mm_texEnableNormal = g_ubPerMaterialFS._texEnableNormal;
	const float4 mm_userColor = g_ubPerMaterialFS._userColor;
	const float4 mm_userPick = g_ubPerMaterialFS._userPick;
	const float motionBlur = g_ubPerMaterialFS._lamScaleBias_lightPass_motionBlur.w;
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

	const float2 alpha_spec = AlphaSwitch(rawAlbedo, tc0, mm_alphaSwitch);
	const float emitAlpha = PickAlpha(rawAlbedo.rgb, mm_emissionPick, 16.0);
	const float eyeAlpha = PickAlphaRound(mm_eyePick, tc0);
	const float glossAlpha = PickAlpha(rawAlbedo.rgb, mm_glossPick, 32.0);
	const float hairAlpha = round(PickAlpha(rawAlbedo.rgb, mm_hairPick, 16.0));
	const float metalAlpha = PickAlpha(rawAlbedo.rgb, mm_metalPick, 16.0);
	const float skinAlpha = PickAlpha(rawAlbedo.rgb, mm_skinPick, 16.0);
	const float userAlpha = PickAlphaHue(rawAlbedo.rgb, mm_userPick, 32.0);

	rawAlbedo.rgb = lerp(rawAlbedo.rgb, Overlay(gray, si.color0.rgb), userAlpha * si.color0.a);
	const float3 hairAlbedo = Overlay(alpha_spec.y, Desaturate(rawAlbedo.rgb, hairAlpha * mm_hairDesat));

	// <Gloss>
	float gloss = lerp(4.0, 16.0, alpha_spec.y) * mm_glossScaleBias.x + mm_glossScaleBias.y;
	gloss = lerp(gloss, mm_gloss, glossAlpha);
	gloss = lerp(gloss, 4.5 + alpha_spec.y, skinAlpha);
	gloss = lerp(gloss, 0.0, eyeAlpha);
	// </Gloss>

	// <Normal>
	float3 normalWV;
	float toksvigFactor;
	float lightPassStrength;
	float3 anisoWV;
	{
		const float texEnableNormalAlpha = ceil(mm_texEnableNormal.a);
		float4 rawNormal = g_texNormal.Sample(g_samNormal, tc0 * texEnableNormalAlpha);
		rawNormal = lerp(mm_texEnableNormal.rgbr, rawNormal, mm_texEnableNormal.a);
		const float4 normalAA = NormalMapAA(rawNormal);
		const float3 normalTBN = normalAA.xyz;
		normalWV = normalize(mul(normalTBN, matFromTBN));
		toksvigFactor = ComputeToksvigFactor(normalAA.a, gloss);
		lightPassStrength = rawNormal.r;
		anisoWV = normalize(mul(cross(normalTBN, cross(float3(mm_anisoSpecDir, 0), normalTBN)), matFromTBN));
	}
	// </Normal>

	// <Detail>
	{
		const float3 rawDetail = g_texDetail.Sample(g_samDetail, tc0 * mm_detailScale).rgb;
		rawAlbedo.rgb = rawAlbedo.rgb * lerp(0.5, rawDetail, mm_detail) * 2.0;
	}
	// </Detail>

	// <Strass>
	float strass;
	{
		const float rawStrass = g_texStrass.Sample(g_samStrass, tc0 * mm_strassScale).r;
		strass = saturate(rawStrass * (0.3 + 0.7 * alpha_spec.y) * 2.0);
	}
	// </Strass>

	// <LambertianScaleBias>
	float2 lamScaleBias = mm_lamScaleBias + float2(0, lightPassStrength * 8.0 * mm_lightPass);
	lamScaleBias += float2(-0.1, -0.3) * alpha_spec.y + float2(0.1, 0.2); // We bring the noise!
	lamScaleBias = lerp(lamScaleBias, float2(1, 0.45), skinAlpha);
	// </LambertianScaleBias>

	// <RimAlbedo>
	{
		const float3 newColor = saturate((rawAlbedo.rgb + gray) * 0.6);
		const float mask = 1.0 - abs(normalWV.z);
		rawAlbedo.rgb = lerp(rawAlbedo.rgb, newColor, mask * mask * (1.0 - alpha_spec.y));
	}
	// </RimAlbedo>

	{
		DS_Reset(so);

		DS_SetAlbedo(so, lerp(rawAlbedo.rgb, hairAlbedo, hairAlpha));
		DS_SetSpec(so, max(eyeAlpha, max(strass,
			alpha_spec.y * (1.0 + hairAlpha * 0.75) * mm_specScaleBias.x + mm_specScaleBias.y)));

		DS_SetNormal(so, normalWV + NormalDither(rand));
		DS_SetEmission(so, emitAlpha * mm_emission, skinAlpha);
		DS_SetMotionBlur(so, motionBlur);

		DS_SetLamScaleBias(so, lamScaleBias, float4(anisoWV, hairAlpha));
		DS_SetMetallicity(so, metalAlpha, hairAlpha);
		DS_SetGloss(so, gloss * lerp(toksvigFactor, 1.0, eyeAlpha));
	}

	clip(alpha_spec.x - 0.5);
#endif

	return so;
}
#endif
#endif

//@main:#
//@main:#Instanced INSTANCED
//@main:#Robotic   ROBOTIC
//@main:#Skinned   SKINNED

//@main:#Depth          DEPTH
//@main:#DepthInstanced DEPTH INSTANCED
//@main:#DepthRobotic   DEPTH ROBOTIC
//@main:#DepthSkinned   DEPTH SKINNED

//@main:#SolidColor          SOLID_COLOR
//@main:#SolidColorInstanced SOLID_COLOR INSTANCED
//@main:#SolidColorRobotic   SOLID_COLOR ROBOTIC
//@main:#SolidColorSkinned   SOLID_COLOR SKINNED

//@main:#Tess          TESS (VHDF)
//@main:#TessInstanced TESS INSTANCED (VHDF)
//@main:#TessRobotic   TESS ROBOTIC (VHDF)
//@main:#TessSkinned   TESS SKINNED (VHDF)
