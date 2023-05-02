// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibTessellation.hlsl"
#include "LibVertex.hlsl"
#include "DS_Mesh.inc.hlsl"

CBUFFER(0, UB_PerView, g_ubPerView)
CBUFFER(1, UB_PerMaterialFS, g_ubPerMaterialFS)
CBUFFER(2, UB_PerMeshVS, g_ubPerMeshVS)
CBUFFER(3, UB_SkeletonVS, g_ubSkeletonVS)
VK_PUSH_CONSTANT
CBUFFER(4, UB_PerObject, g_ubPerObject)

Texture2D    g_texA       : REG(t1, space1, t0);
SamplerState g_samA       : REG(s1, space1, s0);
Texture2D    g_texN       : REG(t2, space1, t1);
SamplerState g_samN       : REG(s2, space1, s1);
Texture2D    g_texX       : REG(t3, space1, t2);
SamplerState g_samX       : REG(s3, space1, s2);
Texture2D    g_texDetail  : REG(t4, space1, t3);
SamplerState g_samDetail  : REG(s4, space1, s3);
Texture2D    g_texDetailN : REG(t5, space1, t4);
SamplerState g_samDetailN : REG(s5, space1, s4);
Texture2D    g_texStrass  : REG(t6, space1, t5);
SamplerState g_samStrass  : REG(s6, space1, s5);

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
	// Binding 2:
	VK_LOCATION_TANGENT  float4 tan : TANGENT;
	VK_LOCATION_BINORMAL float4 bin : BINORMAL;
	// Binding 3:
#if 0
	VK_LOCATION(9)     int2 tc1     : TEXCOORD1;
	VK_LOCATION_COLOR0 float4 color : COLOR0;
#endif
	_PER_INSTANCE_DATA_0
};

struct VSO
{
	float4 pos     : SV_Position;
	float2 tc0     : TEXCOORD0;
	float4 matTBN2 : TEXCOORD1;
#if !defined(DEF_DEPTH)
	float4 color0  : COLOR0;
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	float4 matTBN0 : TEXCOORD2;
	float4 matTBN1 : TEXCOORD3;
#endif
#endif
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Dequantize:
	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);
	const float2 inTc0 = DequantizeUsingDeq2D(si.tc0, g_ubPerMeshVS._tc0DeqScaleBias.xy, g_ubPerMeshVS._tc0DeqScaleBias.zw);
	const float3 inNrm = si.nrm.xyz;
	const float3 inTan = si.tan.xyz;
	const float3 inBin = si.bin.xyz;

	// <TheMatrix>
#ifdef DEF_INSTANCED
	mataff matW = GetInstMatrix(
		si.pid0_matPart0,
		si.pid0_matPart1,
		si.pid0_matPart2);
	const float4 userColor = si.pid0_instData;
#else
	mataff matW = g_ubPerObject._matW;
	const float4 userColor = g_ubPerObject._userColor;
#endif

#if defined(DEF_SKINNED) || defined(DEF_ROBOTIC)
	const float4 warpMask = float4(
		1,
		si.pos.w,
		si.tan.w,
		si.bin.w) * GetWarpScale();
	const float3 pos = ApplyWarp(inPos, g_ubSkeletonVS._vWarpZones, warpMask);
#elif defined(DEF_PLANT)
	{
		const float3 normPos = NormalizePosition(si.pos.xyz);
		const float weightY = saturate(normPos.y * normPos.y - 0.01);
		const float3 offsetXYZ = normPos - float3(0.5, 0.8, 0.5);
		const float weightXZ = saturate((dot(offsetXYZ.xz, offsetXYZ.xz) - 0.01) * 0.4);

		const float phaseShiftY = frac(userColor.x + userColor.z);
		const float phaseShiftXZ = frac(phaseShiftY + inPos.x + inPos.z);
		const float2 bending = (float2(0.7, 0.5) + float2(0.3, 0.5) *
			sin((g_ubPerObject._userColor.rg + float2(phaseShiftY, phaseShiftXZ)) * (_PI * 2.0))) * float2(weightY, weightXZ);

		const float3x3 matW33 = (float3x3)matW;
		const float3x3 matBending = (float3x3)g_ubPerObject._matW;
		const float3x3 matBentW33 = mul(matW33, matBending);
		const float3x3 matRandW33 = mul(matBending, matW33);
		float3x3 matNewW33;
		matNewW33[0] = lerp(matW33[0], matBentW33[0], bending.x);
		matNewW33[1] = lerp(matW33[1], matBentW33[1], bending.x);
		matNewW33[2] = lerp(matW33[2], matBentW33[2], bending.x);
		matNewW33[0] = lerp(matNewW33[0], matRandW33[0], bending.y);
		matNewW33[1] = lerp(matNewW33[1], matRandW33[1], bending.y);
		matNewW33[2] = lerp(matNewW33[2], matRandW33[2], bending.y);
		matW = mataff(
			lerp(matW33[0], matNewW33[0], userColor.a),
			lerp(matW33[1], matNewW33[1], userColor.a),
			lerp(matW33[2], matNewW33[2], userColor.a),
			matW[3]);
	}
	const float3 pos = inPos;
#else
	const float3 pos = inPos;
#endif

	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubPerView._matV));
	// </TheMatrix>

	float3 posW;
	float3 nrmWV;
	float3 tanWV;
	float3 binWV;
	{
#ifdef DEF_SKINNED
		const float4 weights = si.bw * (1.0 / 255.0);
		const int4 indices = si.bi;

		float3 posSkinned = 0.0;
		float3 nrmSkinned = 0.0;
		float3 tanSkinned = 0.0;
		float3 binSkinned = 0.0;

		for (int i = 0; i < 4; ++i)
		{
			const mataff matBone = g_ubSkeletonVS._vMatBones[indices[i]];
			posSkinned += mul(float4(pos, 1), matBone).xyz * weights[i];
			nrmSkinned += mul(inNrm, (float3x3)matBone) * weights[i];
			tanSkinned += mul(inTan, (float3x3)matBone) * weights[i];
			binSkinned += mul(inBin, (float3x3)matBone) * weights[i];
		}

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmSkinned, (float3x3)matWV);
		tanWV = mul(tanSkinned, (float3x3)matWV);
		binWV = mul(binSkinned, (float3x3)matWV);
#elif defined(DEF_ROBOTIC)
		const int index = si.pos.w;

		const mataff matBone = g_ubSkeletonVS._vMatBones[index];

		const float3 posSkinned = mul(float4(pos, 1), matBone).xyz;
		nrmWV = mul(inNrm, (float3x3)matBone);
		tanWV = mul(inTan, (float3x3)matBone);
		binWV = mul(inBin, (float3x3)matBone);

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmWV, (float3x3)matWV);
		tanWV = mul(tanWV, (float3x3)matWV);
		binWV = mul(binWV, (float3x3)matWV);
#else
		posW = mul(float4(pos, 1), matW).xyz;
		nrmWV = mul(inNrm, (float3x3)matWV);
		tanWV = mul(inTan, (float3x3)matWV);
		binWV = mul(inBin, (float3x3)matWV);
#endif
	}

	so.pos = MulTessPos(float4(posW, 1), g_ubPerView._matV, g_ubPerView._matVP);
	so.tc0 = inTc0;
	so.matTBN2 = float4(nrmWV, posW.z);
#ifdef DEF_TESS
	so.matTBN2.xyz = normalize(so.matTBN2.xyz);
#endif

#if !defined(DEF_DEPTH)
	so.color0 = userColor;
#ifdef DEF_PLANT
	so.color0.rgb = RandomColor(userColor.xz, 0.25, 0.15);
	so.color0.a = QuadOutEasing(1.0 - ComputeFade(max(-so.pos.z, so.pos.w), 50.0, 100.0));
#endif
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	so.matTBN0 = float4(tanWV, posW.x);
	so.matTBN1 = float4(binWV, posW.y);
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
	_HS_PCF_BODY(g_ubPerView._matP);
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

	_HS_PN_BODY(matTBN2, g_ubPerView._matP, g_ubPerView._viewportSize);

	_HS_COPY(pos);
	_HS_COPY(tc0);
	_HS_COPY(matTBN2);
#if !defined(DEF_DEPTH)
	_HS_COPY(color0);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_HS_COPY(matTBN0);
	_HS_COPY(matTBN1);
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

	_DS_INIT_FLAT_POS;
	_DS_INIT_SMOOTH_POS;

	const float3 toEyeWV = g_ubPerView._eyePosWV_invTessDistSq.xyz - flatPosWV;
	const float distToEyeSq = dot(toEyeWV, toEyeWV);
	const float tessStrength = 1.0 - saturate(distToEyeSq * g_ubPerView._eyePosWV_invTessDistSq.w * 1.1 - 0.1);
	const float3 posWV = lerp(flatPosWV, smoothPosWV, tessStrength);
	so.pos = ApplyProjection(posWV, g_ubPerView._matP);
	_DS_COPY(tc0);
	_DS_COPY(matTBN2);
#if !defined(DEF_DEPTH)
	_DS_COPY(color0);
#if !defined(DEF_DEPTH) && !defined(DEF_SOLID_COLOR)
	_DS_COPY(matTBN0);
	_DS_COPY(matTBN1);
#endif
#endif

	return so;
}
#endif

#ifdef _FS
#ifdef DEF_DEPTH
void mainFS(VSO si)
{
	const float albedoSam = g_texA.Sample(g_samA, si.tc0).a;
	clip(albedoSam - 0.5);
}
#else
DS_FSO mainFS(VSO si)
{
	DS_FSO so;
	DS_Reset(so);

#ifdef DEF_SOLID_COLOR
	DS_SolidColor(so, si.color0.rgb);
#else
	const float dither = Dither2x2(si.pos.xy);
	const float3 rand = Rand(si.pos.xy);
	_TBN_SPACE(
		si.matTBN0.xyz,
		si.matTBN1.xyz,
		si.matTBN2.xyz);

	// <Material>
	const float2 mm_anisoSpecDir = g_ubPerMaterialFS._anisoSpecDir_detail_emission.xy;
	const float mm_detail = g_ubPerMaterialFS._anisoSpecDir_detail_emission.z;
	const float mm_emission = g_ubPerMaterialFS._anisoSpecDir_detail_emission.w;
	const float mm_motionBlur = g_ubPerMaterialFS._motionBlur_nmContrast_roughDiffuse_sssHue.x;
	const float mm_nmContrast = g_ubPerMaterialFS._motionBlur_nmContrast_roughDiffuse_sssHue.y;
	const float mm_roughDiffuse = frac(g_ubPerMaterialFS._motionBlur_nmContrast_roughDiffuse_sssHue.z);
	const float mm_sssHue = g_ubPerMaterialFS._motionBlur_nmContrast_roughDiffuse_sssHue.w;
	const float2 mm_detailScale = g_ubPerMaterialFS._detailScale_strassScale.xy;
	const float2 mm_strassScale = g_ubPerMaterialFS._detailScale_strassScale.zw;
	const float4 mm_solidA = g_ubPerMaterialFS._solidA;
	const float4 mm_solidN = g_ubPerMaterialFS._solidN;
	const float4 mm_solidX = g_ubPerMaterialFS._solidX;
	const float4 mm_sssPick = g_ubPerMaterialFS._sssPick;
	const float4 mm_tc0ScaleBias = g_ubPerMaterialFS._tc0ScaleBias;
	const float4 mm_userPick = g_ubPerMaterialFS._userPick;
	const float2 mm_xAnisoSpecScaleBias = g_ubPerMaterialFS._xAnisoSpecScaleBias_xMetallicScaleBias.xy;
	const float2 mm_xMetallicScaleBias = g_ubPerMaterialFS._xAnisoSpecScaleBias_xMetallicScaleBias.zw;
	const float2 mm_xRoughnessScaleBias = g_ubPerMaterialFS._xRoughnessScaleBias_xWrapDiffuseScaleBias.xy;
	const float2 mm_xWrapDiffuseScaleBias = g_ubPerMaterialFS._xRoughnessScaleBias_xWrapDiffuseScaleBias.zw;
	// </Material>

	const float2 tc0 = si.tc0 * mm_tc0ScaleBias.xy + mm_tc0ScaleBias.zw;
	const float resolveDitheringMaskEnabled = step(g_ubPerMaterialFS._motionBlur_nmContrast_roughDiffuse_sssHue.z, 1.0);

	// <Albedo>
	float4 albedo;
	{
		const float texEnabled = 1.0 - floor(mm_solidA.a);
		float4 albedoSam = float4(
			g_texA.Sample(g_samA, tc0 * texEnabled).rgb,
			g_texA.SampleBias(g_samA, tc0 * texEnabled, 1.4).a);
		albedoSam.rgb = lerp(albedoSam.rgb, mm_solidA.rgb, mm_solidA.a);

		albedo = albedoSam;
	}
	const float gray = Grayscale(albedo.rgb);
	// </Albedo>

	// <Pick>
	const float sssMask = PickAlpha(albedo.rgb, mm_sssPick, 16.0);
	const float userMask = PickAlpha(albedo.rgb, mm_userPick, 16.0);
	// </Pick>

#ifdef DEF_PLANT
	albedo.rgb = saturate(albedo.rgb * si.color0.rgb * 2.0);
	albedo.a *= si.color0.a;
#else
	albedo.rgb = lerp(albedo.rgb, Overlay(gray, si.color0.rgb), userMask * si.color0.a);
#endif

	// <Detail>
	float3 detailSam;
	float3 detailNSam;
	{
		detailSam = g_texDetail.Sample(g_samDetail, tc0 * mm_detailScale).rgb;
		detailNSam = g_texDetailN.Sample(g_samDetailN, tc0 * mm_detailScale).rgb;
	}
	albedo.rgb = saturate(albedo.rgb * lerp(0.5, detailSam, mm_detail) * 2.0);
	// </Detail>

	// <Normal>
	float3 normalWV;
	float3 tangentWV;
	{
		const float texEnabled = 1.0 - floor(mm_solidN.a);
		float4 normalSam = g_texN.Sample(g_samN, tc0 * texEnabled);
		normalSam.rgb = lerp(normalSam.rgb, mm_solidN.rgb, mm_solidN.a);

		normalSam.rg = saturate(normalSam.rg * lerp(0.5, detailNSam.rg, mm_detail) * 2.0);

		const float3 normalTBN = NormalMapFromBC5(normalSam, mm_nmContrast);
		normalWV = normalize(mul(normalTBN, matFromTBN));
		tangentWV = normalize(mul(cross(normalTBN, cross(float3(mm_anisoSpecDir, 0), normalTBN)), matFromTBN));
	}
	// </Normal>

	// <X>
	float occlusion;
	float roughness;
	float metallic;
	float emission;
	float wrapDiffuse;
	float anisoSpec;
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
		wrapDiffuse = metallic_wrapDiffuse.g;
		anisoSpec = emission_anisoSpec.g;

		// Scale & bias:
		roughness = roughness * mm_xRoughnessScaleBias.x + mm_xRoughnessScaleBias.y;
		metallic = metallic * mm_xMetallicScaleBias.x + mm_xMetallicScaleBias.y;
		emission *= mm_emission;
		wrapDiffuse = wrapDiffuse * mm_xWrapDiffuseScaleBias.x + mm_xWrapDiffuseScaleBias.y;
		anisoSpec = anisoSpec * mm_xAnisoSpecScaleBias.x + mm_xAnisoSpecScaleBias.y;
	}
	// </X>

	// <Rim>
	{
		const float rim = saturate(1.0 - normalWV.z);
		const float rimSq = rim * rim;
		albedo.rgb = lerp(albedo.rgb, gray, rim * rimSq * wrapDiffuse);
		roughness = saturate(roughness + rimSq * 3.0 * wrapDiffuse);
	}
	// </Rim>

	// <Strass>
	{
		const float strass = g_texStrass.Sample(g_samStrass, tc0 * mm_strassScale).r;
		roughness *= 1.0 - strass * mm_roughDiffuse;
	}
	// </Strass>

	{
		DS_SetAlbedo(so, albedo.rgb);
		DS_SetSSSHue(so, lerp(0.5, mm_sssHue, sssMask));

		DS_SetNormal(so, normalWV + NormalDither(rand));
		DS_SetEmission(so, emission);
		DS_SetMotionBlur(so, mm_motionBlur);

		DS_SetOcclusion(so, occlusion);
		DS_SetRoughness(so, roughness);
		DS_SetMetallic(so, metallic);
		DS_SetWrapDiffuse(so, wrapDiffuse);

		DS_SetTangent(so, tangentWV);
		DS_SetAnisoSpec(so, anisoSpec);
		DS_SetRoughDiffuse(so, max(mm_roughDiffuse, AlphaToResolveDitheringMask(albedo.a) * resolveDitheringMaskEnabled));
	}

	clip(albedo.a - (dither + (1.0 / 8.0)));
#endif

	return so;
}
#endif
#endif

//@main:#
//@main:#Instanced INSTANCED
//@main:#Robotic   ROBOTIC
//@main:#Skinned   SKINNED
//@main:#Plant     PLANT INSTANCED

//@main:#Tess          TESS (VHDF)
//@main:#TessInstanced TESS INSTANCED (VHDF)
//@main:#TessRobotic   TESS ROBOTIC (VHDF)
//@main:#TessSkinned   TESS SKINNED (VHDF)
//@main:#TessPlant     TESS PLANT INSTANCED (VHDF)

//@main:#Depth          DEPTH
//@main:#DepthInstanced DEPTH INSTANCED
//@main:#DepthRobotic   DEPTH ROBOTIC
//@main:#DepthSkinned   DEPTH SKINNED
//@main:#DepthPlant     DEPTH PLANT INSTANCED

//@main:#DepthTess          DEPTH TESS (VHDF)
//@main:#DepthTessInstanced DEPTH TESS INSTANCED (VHDF)
//@main:#DepthTessRobotic   DEPTH TESS ROBOTIC (VHDF)
//@main:#DepthTessSkinned   DEPTH TESS SKINNED (VHDF)
//@main:#DepthTessPlant     DEPTH TESS PLANT INSTANCED (VHDF)

//@main:#SolidColor          SOLID_COLOR
//@main:#SolidColorInstanced SOLID_COLOR INSTANCED
//@main:#SolidColorRobotic   SOLID_COLOR ROBOTIC
//@main:#SolidColorSkinned   SOLID_COLOR SKINNED
