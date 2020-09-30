// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "DS_AO.inc.hlsl"

ConstantBuffer<UB_AOPerFrame>   g_ubAOPerFrame   : register(b0, space0);
ConstantBuffer<UB_AOTexturesFS> g_ubAOTexturesFS : register(b0, space1);
ConstantBuffer<UB_AOPerMeshVS>  g_ubAOPerMeshVS  : register(b0, space2);
VK_PUSH_CONSTANT
ConstantBuffer<UB_AOPerObject>  g_ubAOPerObject  : register(b0, space3);

VK_SUBPASS_INPUT(0, g_texDepth, g_samDepth, t1, s1, space1);
Texture2D    g_texGBuffer1 : register(t2, space1);
SamplerState g_samGBuffer1 : register(s2, space1);

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
	float3 radius_radiusSq_invRadiusSq : TEXCOORD1;
	float3 lightPosWV                  : TEXCOORD2;
	float4 color_coneOut               : TEXCOORD3;
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
	const float3 color = si.instData.rgb;
	const float coneIn = si.instData.a;
#else
	const mataff matW = g_ubAOPerObject._matW;
	const float3 color = g_ubAOPerObject.rgb;
	const float coneIn = g_ubAOPerObject.a;
#endif

	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubAOPerFrame._matV));

	const float3x3 matW33 = (float3x3)matW;
	const float3x3 matV33 = (float3x3)g_ubAOPerFrame._matV;
	const float3x3 matWV33 = (float3x3)matWV;

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubAOPerMeshVS._posDeqScale.xyz, g_ubAOPerMeshVS._posDeqBias.xyz);

	const float3 posW = mul(float4(intactPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_ubAOPerFrame._matVP);
	so.clipSpacePos = so.pos;

	// <MoreAOParams>
	const float3 posUnit = mul(float3(0, 0, 1), matW33); // Need to know the scale.
	so.radius_radiusSq_invRadiusSq.y = dot(posUnit, posUnit);
	so.radius_radiusSq_invRadiusSq.x = sqrt(so.radius_radiusSq_invRadiusSq.y);
	so.radius_radiusSq_invRadiusSq.z = 1.0 / so.radius_radiusSq_invRadiusSq.y;
	const float4 posOrigin = float4(0, 0, 0, 1);
	so.lightPosWV = mul(posOrigin, matWV).xyz;
	so.color_coneOut = float4(color, 1.0);
	// </MoreAOParams>

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = 1.0;

	const float3 ndcPos = si.clipSpacePos.xyz / si.clipSpacePos.w;

	const float2 tc0 = mul(float4(ndcPos.xy, 0, 1), g_ubAOPerFrame._matToUV).xy;

	const float radius = si.radius_radiusSq_invRadiusSq.x;
	const float radiusSq = si.radius_radiusSq_invRadiusSq.y;
	const float invRadiusSq = si.radius_radiusSq_invRadiusSq.z;
	const float3 lightPosWV = si.lightPosWV;

	// GBuffer1:
	const float depth = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, tc0).r;
	const float3 posWV = DS_GetPosition(depth, g_ubAOPerFrame._matInvP, ndcPos.xy);

	if (posWV.z <= lightPosWV.z + radius)
	{
		const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, tc0, 0.0);
		const float3 normalWV = DS_GetNormal(rawGBuffer1);

		const float3 toLightWV = lightPosWV - posWV;
		const float3 dirToLightWV = normalize(toLightWV);
		const float distToLightSq = dot(toLightWV, toLightWV);
		const float lightFalloff = min(0.25, ComputePointLightIntensity(distToLightSq, radiusSq, invRadiusSq));

		const float nDotL = saturate(0.1 + dot(normalWV, dirToLightWV));

		so.color = 1.0 - nDotL * lightFalloff;
	}
	else
	{
		discard;
	}

	return so;
}
#endif

//@main:#InstancedOmni INSTANCED OMNI
