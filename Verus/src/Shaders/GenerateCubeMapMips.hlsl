// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "GenerateCubeMapMips.inc.hlsl"

CBUFFER(0, UB_GenerateCubeMapMips, g_ub)

TextureCube  g_texSrcMip : REG(t1, space0, t0);
SamplerState g_samSrcMip : REG(s1, space0, s0);

RWTexture2DArray<float4> g_uavOutFacePosX : REG(u2, space0, u0);
RWTexture2DArray<float4> g_uavOutFaceNegX : REG(u3, space0, u1);
RWTexture2DArray<float4> g_uavOutFacePosY : REG(u4, space0, u2);
RWTexture2DArray<float4> g_uavOutFaceNegY : REG(u5, space0, u3);
RWTexture2DArray<float4> g_uavOutFacePosZ : REG(u6, space0, u4);
RWTexture2DArray<float4> g_uavOutFaceNegZ : REG(u7, space0, u5);

struct CSI
{
	uint3 dispatchThreadID : SV_DispatchThreadID;
	uint  groupIndex       : SV_GroupIndex;
};

#define THREAD_GROUP_SIZE 8

float4 PackColor(float4 x)
{
	return g_ub._srgb ? ColorToSRGB(x) : x;
}

#ifdef _CS
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void mainCS(CSI si)
{
	const float2 tcPosX = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirPosX = float3(+1, -tcPosX.y, -tcPosX.x);
	const float4 colorPosX = g_texSrcMip.SampleLevel(g_samSrcMip, dirPosX, g_ub._srcMipLevel);
	g_uavOutFacePosX[si.dispatchThreadID] = PackColor(colorPosX);

	const float2 tcNegX = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegX = float3(-1, -tcNegX.y, tcNegX.x);
	const float4 colorNegX = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegX, g_ub._srcMipLevel);
	g_uavOutFaceNegX[si.dispatchThreadID] = PackColor(colorNegX);

	const float2 tcPosY = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirPosY = float3(tcPosY.x, +1, tcPosY.y);
	const float4 colorPosY = g_texSrcMip.SampleLevel(g_samSrcMip, dirPosY, g_ub._srcMipLevel);
	g_uavOutFacePosY[si.dispatchThreadID] = PackColor(colorPosY);

	const float2 tcNegY = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegY = float3(+tcNegY.x, -1, tcNegY.y);
	const float4 colorNegY = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegY, g_ub._srcMipLevel);
	g_uavOutFaceNegY[si.dispatchThreadID] = PackColor(colorNegY);

	const float2 tcPosZ = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirPosZ = float3(tcPosZ.x, -tcPosZ.y, +1);
	const float4 colorPosZ = g_texSrcMip.SampleLevel(g_samSrcMip, dirPosZ, g_ub._srcMipLevel);
	g_uavOutFacePosZ[si.dispatchThreadID] = PackColor(colorPosZ);

	const float2 tcNegZ = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegZ = float3(-tcNegZ.x, -tcNegZ.y, -1);
	const float4 colorNegZ = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegZ, g_ub._srcMipLevel);
	g_uavOutFaceNegZ[si.dispatchThreadID] = PackColor(colorNegZ);
}
#endif

//@main:# (C)
