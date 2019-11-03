#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
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
//Texture2D    g_texDetail : register(t3, space1);
//SamplerState g_samDetail : register(s3, space1);

struct VSI
{
	// Binding 0:
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
	// Binding 1:
#ifdef DEF_SKINNED
	VK_LOCATION(12)      int4 bw : TEXCOORD4;
	VK_LOCATION(13)      int4 bi : TEXCOORD5;
#endif
	// Binding 2:
	VK_LOCATION(14)      float4 tan : TEXCOORD6;
	VK_LOCATION(15)      float4 bin : TEXCOORD7;
	// Binding 3:
#if 0
	VK_LOCATION(9)       int4 tc1     : TEXCOORD1;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
#endif

	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos : SV_Position;
	float2 tc0 : TEXCOORD0;
#ifndef DEF_DEPTH
	float4 matTBN0 : TEXCOORD2;
	float4 matTBN1 : TEXCOORD3;
	float4 matTBN2 : TEXCOORD4;
	float4 color0 : COLOR0;
#endif
	float2 depth : TEXCOORD6;
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

	const float3 pos = intactPos;

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

	so.pos = mul(float4(posW, 1), g_ubPerFrame._matVP);
	so.tc0 = intactTc0;
#ifndef DEF_DEPTH
	so.matTBN0 = float4(tanWV, posW.x);
	so.matTBN1 = float4(binWV, posW.y);
	so.matTBN2 = float4(nrmWV, posW.z);
	so.color0 = userColor;
#endif
	so.depth = so.pos.zw;

	return so;
}
#endif

#ifdef _FS
#ifdef DEF_DEPTH
#ifdef DEF_CLIP
void mainFS(VSO si)
{
	const float rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, si.tc0).a;
	clip(rawAlbedo - 1.0 / 16.0);
}
#endif
#else
_DS_FSO mainFS(VSO si)
{
	_DS_FSO so;

#ifdef DEF_SOLID
	DS_Reset(so);
	DS_SetAlbedo(so, si.color0.rgb);
	DS_SetDepth(so, si.depth.x / si.depth.y);
	DS_SetNormal(so, float3(0, 0, 1));
	DS_SetEmission(so, 1.0, 0.0);
	DS_SetMetallicity(so, 1.0, 0.0);
#else
	_TBN_SPACE(
		si.matTBN0.xyz,
		si.matTBN1.xyz,
		si.matTBN2.xyz);
	const float3 rand = Rand(si.pos.xy);

	// <Albedo>
	const float texAlbedoEnable = ceil(g_ubPerMaterialFS._texEnableAlbedo.a);
	float4 rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, si.tc0 * texAlbedoEnable);
	rawAlbedo.rgb = lerp(g_ubPerMaterialFS._texEnableAlbedo.rgb, rawAlbedo.rgb, g_ubPerMaterialFS._texEnableAlbedo.a);
	const float2 alpha_spec = AlphaSwitch(rawAlbedo, si.tc0, g_ubPerMaterialFS._ssb_as.zw);

	const float emitAlpha = PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._emitPick, 16.0);
	const float emitXAlpha = PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._emitXPick, 16.0);
	const float eyeAlpha = PickAlphaRound(g_ubPerMaterialFS._eyePick, si.tc0);
	const float glossXAlpha = PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._glossXPick, 32.0);
	const float hairAlpha = round(PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._hairPick, 16.0));
	const float metalAlpha = PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._metalPick, 16.0);
	const float skinAlpha = PickAlpha(rawAlbedo.rgb, g_ubPerMaterialFS._skinPick, 16.0);
	const float userAlpha = PickAlphaHue(rawAlbedo.rgb, g_ubPerMaterialFS._userPick, 32.0);

	// Add details:
#if 0
	const float3 colorDetail = (g_texDetail.Sample(g_samDetail, frac(si.tc0 * g_ubPerMaterialFS._ds_scale.zw)).rgb - 0.5) *
		g_ubPerMaterialFS._ds_scale.x + 0.5;
	rawAlbedo.rgb = rawAlbedo.rgb * colorDetail * 2.0;
#endif

	const float gray = Grayscale(rawAlbedo.rgb);
	rawAlbedo.rgb = lerp(rawAlbedo.rgb, Overlay(gray, si.color0.rgb), userAlpha * si.color0.a);
	// </Albedo>

	// <Normal>
	const float texNormalEnable = ceil(g_ubPerMaterialFS._texEnableNormal.a);
	float4 rawNormal = g_texNormal.Sample(g_samNormal, si.tc0 * texNormalEnable);
	rawNormal = lerp(g_ubPerMaterialFS._texEnableNormal.rgbr, rawNormal, g_ubPerMaterialFS._texEnableNormal.a);
	const float4 normalAA = NormalMapAA(rawNormal);
	const float3 normalTBN = normalAA.xyz;
	const float3 normalWV = normalize(mul(normalTBN, matFromTBN));

	const float3 anisoWV = normalize(mul(cross(normalTBN, cross(g_ubPerMaterialFS._hairParams.xyz, normalTBN)), matFromTBN));
	// Almost solid hair color is used for picking.
	// Final albedo is calculated using specualar component.
	const float3 hairAlbedo = Overlay(alpha_spec.y, Desaturate(rawAlbedo.rgb, hairAlpha * g_ubPerMaterialFS._hairParams.w));
	// </Normal>

	// <LambertianScaleBias>
	float2 lamScaleBias = g_ubPerMaterialFS._lsb_gloss_lp.xy + float2(0, rawNormal.r * 8.0 * g_ubPerMaterialFS._lsb_gloss_lp.w);
	lamScaleBias = lerp(lamScaleBias, float2(1, 0.45), skinAlpha);
	// </LambertianScaleBias>

	// <Gloss>
	float gloss = lerp(g_ubPerMaterialFS._lsb_gloss_lp.z, g_ubPerMaterialFS._motionBlur_glossX.y, glossXAlpha);
	gloss = lerp(gloss, 4.0 + alpha_spec.y, skinAlpha);
	gloss = lerp(gloss, 0.0, eyeAlpha);
	// </Gloss>

	const float strass = 0.0;

	// <RimAlbedo>
	//const float rimA = lerp(0.3 + alpha_spec.y*0.3, 0.7, skinAlpha);
	//const float3 rimAlbedo = saturate(rawAlbedo.rgb*rimA*2.0);
	//const float rimAlbedoAmount = 1.0 - abs(normalWV.z);
	//rawAlbedo.rgb = lerp(rawAlbedo.rgb, rimAlbedo, rimAlbedoAmount*rimAlbedoAmount);
	// </RimAlbedo>

	DS_Reset(so);
	DS_SetAlbedo(so, lerp(rawAlbedo.rgb, hairAlbedo, hairAlpha));
	DS_SetSpec(so, normalAA.w * max(eyeAlpha, max(strass,
		alpha_spec.y * (1.0 + hairAlpha * 0.75) * g_ubPerMaterialFS._ssb_as.x + g_ubPerMaterialFS._ssb_as.y)));
	DS_SetDepth(so, si.depth.x / si.depth.y);
	DS_SetNormal(so, normalWV + NormalDither(rand));
	DS_SetEmission(so, max(emitAlpha, emitXAlpha) * alpha_spec.y, skinAlpha);
	DS_SetLamScaleBias(so, lamScaleBias, float4(anisoWV, hairAlpha));
	DS_SetMetallicity(so, metalAlpha, hairAlpha);
	DS_SetGloss(so, normalAA.w * gloss);
#endif

	return so;
}
#endif
#endif

//@main:#
//@main:#DepthRobotic DEPTH ROBOTIC (V)
//@main:#DepthSkinned DEPTH SKINNED (V)
//@main:#Instanced    INSTANCED
//@main:#Robotic      ROBOTIC
//@main:#Skinned      SKINNED
