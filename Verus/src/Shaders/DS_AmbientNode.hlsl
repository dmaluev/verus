// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibVertex.hlsl"
#include "DS_AmbientNode.inc.hlsl"

CBUFFER(0, UB_AmbientNodeView, g_ubView)
CBUFFER(1, UB_AmbientNodeSubpassFS, g_ubSubpassFS)
CBUFFER(2, UB_AmbientNodeMeshVS, g_ubMeshVS)
SBUFFER(3, SB_AmbientNodeInstanceData, g_sbInstanceData, t5)
VK_PUSH_CONSTANT
CBUFFER(4, UB_AmbientNodeObject, g_ubObject)

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, 1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, 2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, 3, space1);
VK_SUBPASS_INPUT(3, g_texGBuffer3, g_samGBuffer3, 4, space1);
VK_SUBPASS_INPUT(4, g_texDepth, g_samDepth, 5, space1);

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
	float4 pos           : SV_Position;
	float4 clipSpacePos  : TEXCOORD0;
	float4 color_initial : TEXCOORD1;
	float3 centerPosWV   : TEXCOORD2;
	uint instanceID      : SV_InstanceID;
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
	const float initial = si.pid0_instData.a;
#else
	const mataff matW = g_ubObject._matW;
	const float3 color = g_ubObject._color.rgb;
	const float initial = g_ubObject._color.a;
#endif
	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_ubView._matV));
	// </TheMatrix>

	const float3 posW = mul(float4(inPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_ubView._matVP);
	so.clipSpacePos = so.pos;

	so.color_initial = float4(color, initial);

	so.centerPosWV = mul(float4(0, 0, 0, 1), matWV).xyz;

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

	const float3 ndcPos = si.clipSpacePos.xyz / si.clipSpacePos.w;
	const float2 tc0 = mul(float4(ndcPos.xy, 0, 1), g_ubView._matToUV).xy *
		g_ubView._tcViewScaleBias.xy + g_ubView._tcViewScaleBias.zw;

	// Depth:
	const float depthSam = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, tc0).r;
	const float3 posWV = DS_GetPosition(depthSam, g_ubView._matInvP, ndcPos.xy);

#ifdef _VULKAN
	const uint realInstanceID = si.instanceID;
#else
	const uint realInstanceID = si.instanceID + g_ubSubpassFS._firstInstance.x;
#endif
	const float3 posInBoxSpace = mul(float4(posWV, 1), g_sbInstanceData[realInstanceID]._matToBoxSpace);
	const float wall = g_sbInstanceData[realInstanceID]._wall_wallScale_cylinder_sphere.x;
	const float wallScale = g_sbInstanceData[realInstanceID]._wall_wallScale_cylinder_sphere.y;
	const float cylinder = g_sbInstanceData[realInstanceID]._wall_wallScale_cylinder_sphere.z;
	const float sphere = g_sbInstanceData[realInstanceID]._wall_wallScale_cylinder_sphere.w;

	so.target0.rgb = si.color_initial.a;
	if (all(abs(posInBoxSpace) < 0.5))
	{
		// Normal:
		const float4 gBuffer1Sam = VK_SUBPASS_LOAD(g_texGBuffer1, g_samGBuffer1, tc0);
		const float3 normalWV = DS_GetNormal(gBuffer1Sam);

		const float3 dirToPosWV = normalize(posWV - si.centerPosWV);
		const float dos = saturate(dot(normalWV, dirToPosWV));

		const float3 rawMask = abs(posInBoxSpace) * 2.0;
		const float2 lenSq = float2(dot(rawMask.xz, rawMask.xz), dot(rawMask, rawMask));
		const float2 len = sqrt(lenSq);

		const float3 walledMask = saturate((rawMask - wall) * wallScale);
		const float squareOcclusion = Screen(walledMask.x, walledMask.z);
		const float discOcclusion = saturate((len.x - wall) * wallScale);
		const float sphereOcclusion = saturate((len.y - wall) * wallScale);

		const float occlusion = lerp(
			Screen(lerp(squareOcclusion, discOcclusion, cylinder), walledMask.y),
			sphereOcclusion,
			sphere);
		so.target0.rgb = lerp(si.color_initial.rgb, si.color_initial.a, max(occlusion, dos * saturate(occlusion * 4.0)));
	}
	else
	{
		discard;
	}

	return so;
}
#endif

//@main:#Instanced INSTANCED
