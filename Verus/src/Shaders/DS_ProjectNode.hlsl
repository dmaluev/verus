// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibVertex.hlsl"
#include "DS_ProjectNode.inc.hlsl"

CBUFFER(0, UB_ProjectNodeView, g_ubView)
CBUFFER(1, UB_ProjectNodeSubpassFS, g_ubSubpassFS)
CBUFFER(2, UB_ProjectNodeMeshVS, g_ubMeshVS)
CBUFFER(3, UB_ProjectNodeTextureFS, g_ubTextureFS)
CBUFFER(4, UB_ProjectNodeObject, g_ubObject)

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, 1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, 2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, 3, space1);
VK_SUBPASS_INPUT(3, g_texGBuffer3, g_samGBuffer3, 4, space1);
VK_SUBPASS_INPUT(4, g_texDepth, g_samDepth, 5, space1);
Texture2D    g_tex : REG(t1, space3, t5);
SamplerState g_sam : REG(s1, space3, s5);

struct VSI
{
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int2 tc0   : TEXCOORD0;
};

struct VSO
{
	float4 pos          : SV_Position;
	float4 clipSpacePos : TEXCOORD0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubMeshVS._posDeqScale.xyz, g_ubMeshVS._posDeqBias.xyz);

	const float3 posW = mul(float4(inPos, 1), g_ubObject._matW);
	so.pos = mul(float4(posW, 1), g_ubView._matVP);
	so.clipSpacePos = so.pos;

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

	const float3 posInBoxSpace = mul(float4(posWV, 1), g_ubObject._matToBoxSpace);

	if (all(abs(posInBoxSpace) < 0.5))
	{
		const float3 color = g_tex.Sample(g_sam, posInBoxSpace.xz + 0.5).rgb;
		so.target0.rgb = color * g_ubObject._levels.x;
		so.target1.rgb = color * g_ubObject._levels.y;
		so.target2.rgb = color * g_ubObject._levels.z;
	}
	else
	{
		discard;
	}

	return so;
}
#endif

//@main:#
