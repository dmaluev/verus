#include "Lib.hlsl"
#include "LibVertex.hlsl"
#include "DS_Mesh.inc.hlsl"

ConstantBuffer<UB_PerFrame>    g_perFrame    : register(b0, space0);
ConstantBuffer<UB_PerMaterial> g_perMaterial : register(b0, space1);
ConstantBuffer<UB_PerMesh>     g_perMesh     : register(b0, space2);
ConstantBuffer<UB_Skinning>    g_skinning    : register(b0, space3);
VK_PUSH_CONSTANT
ConstantBuffer<UB_PerObject>   g_perObject   : register(b0, space4);

Texture2D    g_texAlbedo : register(t1, space1);
SamplerState g_samAlbedo : register(s1, space1);
Texture2D    g_texNormal : register(t2, space1);
SamplerState g_samNormal : register(s2, space1);

struct VSI
{
	// Binding 0:
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
	// Binding 1:
#ifdef DEF_SKELETON
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
	float4 matTBN0 : TEXCOORD2;
	float4 matTBN1 : TEXCOORD3;
	float4 matTBN2 : TEXCOORD4;
	float4 color0 : COLOR0;
};

struct FSO
{
	float4 color : SV_Target0;
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
	const mataff matW = g_perObject._matW;
	const float4 userColor = g_perObject._userColor;
#endif

	// Dequantize:
	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_perMesh._posDeqScale.xyz, g_perMesh._posDeqBias.xyz);
	const float2 intactTc0 = DequantizeUsingDeq2D(si.tc0.xy, g_perMesh._tc0DeqScaleBias.xy, g_perMesh._tc0DeqScaleBias.zw);
	const float3 intactNrm = si.nrm.xyz;
	const float3 intactTan = si.tan.xyz;
	const float3 intactBin = si.bin.xyz;

	const float3 pos = intactPos;

	float3 posW;
	float3 nrmWV;
	float3 tanWV;
	float3 binWV;
	{
#ifdef DEF_SKELETON
		const float4 weights = si.bw / 255.0;
		const float4 indices = si.bi;

		float3 posSkinned = 0.0;
		float3 nrmSkinned = 0.0;
		float3 tanSkinned = 0.0;
		float3 binSkinned = 0.0;

		for (int i = 0; i < 4; ++i)
		{
			const mataff matBone = g_skinning._vMatBones[indices[i]];
			posSkinned += mul(float4(pos, 1), matBone).xyz*weights[i];
			nrmSkinned += mul(intactNrm, (float3x3)matBone)*weights[i];
			tanSkinned += mul(intactTan, (float3x3)matBone)*weights[i];
			binSkinned += mul(intactBin, (float3x3)matBone)*weights[i];
		}

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmSkinned, (float3x3)g_perFrame._matVP);
		tanWV = mul(tanSkinned, (float3x3)g_perFrame._matVP);
		binWV = mul(binSkinned, (float3x3)g_perFrame._matVP);
#elif DEF_SKELETON_RIGID
		const int index = si.pos.w;

		const mataff matBone = g_skinning._vMatBones[index];

		const float3 posSkinned = mul(float4(pos, 1), matBone).xyz;
		nrmWV = mul(intactNrm, (float3x3)matBone);
		tanWV = mul(intactTan, (float3x3)matBone);
		binWV = mul(intactBin, (float3x3)matBone);

		posW = mul(float4(posSkinned, 1), matW).xyz;
		nrmWV = mul(nrmWV, (float3x3)g_perFrame._matVP);
		tanWV = mul(tanWV, (float3x3)g_perFrame._matVP);
		binWV = mul(binWV, (float3x3)g_perFrame._matVP);
#else
		posW = mul(float4(pos, 1), matW).xyz;
		nrmWV = mul(intactNrm, (float3x3)g_perFrame._matVP);
		tanWV = mul(intactTan, (float3x3)g_perFrame._matVP);
		binWV = mul(intactBin, (float3x3)g_perFrame._matVP);
#endif
	}

	so.pos = mul(float4(posW, 1), g_perFrame._matVP);
	so.tc0 = intactTc0;
	so.matTBN0 = float4(tanWV, posW.x);
	so.matTBN1 = float4(binWV, posW.y);
	so.matTBN2 = float4(nrmWV, posW.z);
	so.color0 = userColor;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	float4 texAlbedo = g_texAlbedo.Sample(g_samAlbedo, si.tc0);

	float4 rawNormal = g_texNormal.Sample(g_samNormal, si.tc0);
	const float4 normalAA = NormalMapAA(rawNormal);
	const float3 normalTBN = normalAA.xyz;

	so.color = texAlbedo;

	return so;
}
#endif

//@main:T

//@main:TSkeleton SKELETON
//@main:TSkeletonRigid SKELETON_RIGID

//@main:TInstanced INSTANCED
