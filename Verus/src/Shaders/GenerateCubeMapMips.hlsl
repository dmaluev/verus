// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "GenerateCubeMapMips.inc.hlsl"

ConstantBuffer<UB_GenerateCubeMapMips> g_ub : register(b0, space0);

TextureCube  g_texSrcMip : register(t1, space0);
SamplerState g_samSrcMip : register(s1, space0);

RWTexture2D<float4> g_uavOutFacePosX : register(u2, space0);
RWTexture2D<float4> g_uavOutFaceNegX : register(u3, space0);
RWTexture2D<float4> g_uavOutFacePosY : register(u4, space0);
RWTexture2D<float4> g_uavOutFaceNegY : register(u5, space0);
RWTexture2D<float4> g_uavOutFacePosZ : register(u6, space0);
RWTexture2D<float4> g_uavOutFaceNegZ : register(u7, space0);

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
	g_uavOutFacePosX[si.dispatchThreadID.xy] = PackColor(colorPosX);

	const float2 tcNegX = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegX = float3(-1, -tcNegX.y, tcNegX.x);
	const float4 colorNegX = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegX, g_ub._srcMipLevel);
	g_uavOutFaceNegX[si.dispatchThreadID.xy] = PackColor(colorNegX);

	const float2 tcPosY = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirPosY = float3(tcPosY.x, +1, tcPosY.y);
	const float4 colorPosY = g_texSrcMip.SampleLevel(g_samSrcMip, dirPosY, g_ub._srcMipLevel);
	g_uavOutFacePosY[si.dispatchThreadID.xy] = PackColor(colorPosY);

	const float2 tcNegY = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegY = float3(+tcNegY.x, -1, tcNegY.y);
	const float4 colorNegY = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegY, g_ub._srcMipLevel);
	g_uavOutFaceNegY[si.dispatchThreadID.xy] = PackColor(colorNegY);

	const float2 tcPosZ = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirPosZ = float3(tcPosZ.x, -tcPosZ.y, +1);
	const float4 colorPosZ = g_texSrcMip.SampleLevel(g_samSrcMip, dirPosZ, g_ub._srcMipLevel);
	g_uavOutFacePosZ[si.dispatchThreadID.xy] = PackColor(colorPosZ);

	const float2 tcNegZ = g_ub._dstTexelSize * (si.dispatchThreadID.xy + 0.5) * 2.0 - 1.0;
	const float3 dirNegZ = float3(-tcNegZ.x, -tcNegZ.y, -1);
	const float4 colorNegZ = g_texSrcMip.SampleLevel(g_samSrcMip, dirNegZ, g_ub._srcMipLevel);
	g_uavOutFaceNegZ[si.dispatchThreadID.xy] = PackColor(colorNegZ);
}
#endif

//@main:# (C)
