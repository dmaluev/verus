// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "GenerateMips.inc.hlsl"

ConstantBuffer<UB_GenerateMips> g_ub : register(b0, space0);

Texture2D    g_texSrcMip : register(t1, space0);
SamplerState g_samSrcMip : register(s1, space0);

RWTexture2D<float4> g_uavOutMip1 : register(u2, space0);
RWTexture2D<float4> g_uavOutMip2 : register(u3, space0);
RWTexture2D<float4> g_uavOutMip3 : register(u4, space0);
RWTexture2D<float4> g_uavOutMip4 : register(u5, space0);

struct CSI
{
	uint3 dispatchThreadID : SV_DispatchThreadID;
	uint  groupIndex       : SV_GroupIndex;
};

#define THREAD_GROUP_SIZE 8

#define DIM_CASE_WE_HE 0
#define DIM_CASE_WO_HE 1
#define DIM_CASE_WE_HO 2
#define DIM_CASE_WO_HO 3

groupshared float gs_R[64];
groupshared float gs_G[64];
groupshared float gs_B[64];
groupshared float gs_A[64];

void StoreColor(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
	gs_A[index] = color.a;
}

float4 LoadColor(uint index)
{
	return float4(gs_R[index], gs_G[index], gs_B[index], gs_A[index]);
}

float4 PackColor(float4 x)
{
	if (g_ub._srgb)
	{
		return ColorToSRGB(x);
	}
	else
	{
		return x;
	}
}

#ifdef _CS
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void mainCS(CSI si)
{
	float4 srcColor1 = 0.0;
	float2 tc = 0.0;

	switch (g_ub._srcDimensionCase)
	{
	case DIM_CASE_WE_HE:
	{
		tc = g_ub._texelSize * (si.dispatchThreadID.xy + 0.5);

		srcColor1 = g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel);
	}
	break;
	case DIM_CASE_WO_HE:
	{
		tc = g_ub._texelSize * (si.dispatchThreadID.xy + float2(0.25, 0.5));
		const float2 offset = g_ub._texelSize * float2(0.5, 0.0);

		srcColor1 = lerp(
			g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
			g_texSrcMip.SampleLevel(g_samSrcMip, tc + offset, g_ub._srcMipLevel),
			0.5);
	}
	break;
	case DIM_CASE_WE_HO:
	{
		tc = g_ub._texelSize * (si.dispatchThreadID.xy + float2(0.5, 0.25));
		const float2 offset = g_ub._texelSize * float2(0.0, 0.5);

		srcColor1 = lerp(
			g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
			g_texSrcMip.SampleLevel(g_samSrcMip, tc + offset, g_ub._srcMipLevel),
			0.5);
	}
	break;
	case DIM_CASE_WO_HO:
	{
		tc = g_ub._texelSize * (si.dispatchThreadID.xy + float2(0.25, 0.25));
		const float2 offset = g_ub._texelSize * 0.5;

		srcColor1 = lerp(
			lerp(
				g_texSrcMip.SampleLevel(g_samSrcMip, tc, g_ub._srcMipLevel),
				g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(offset.x, 0.0), g_ub._srcMipLevel),
				0.5),
			lerp(g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(0.0, offset.y), g_ub._srcMipLevel),
				g_texSrcMip.SampleLevel(g_samSrcMip, tc + float2(offset.x, offset.y), g_ub._srcMipLevel),
				0.5),
			0.5);
	}
	break;
	}

#ifdef DEF_EXPOSURE
	if (0 == g_ub._srcMipLevel)
	{
		const float2 delta = 0.5 - tc;
		const float2 centerWeighted = saturate((dot(delta, delta) - float2(0.1, 0.01)) * float2(4.0, 200.0));
		const float gray = Grayscale(srcColor1.rgb);
		const float2 mask = saturate((float2(-1, 1) * gray + float2(0.1, -0.99)) * float2(10, 100));
		const float filter = max(mask.x, mask.y) * centerWeighted.y;
		const float alpha = max(centerWeighted.x, filter);
		srcColor1.rgb = lerp(srcColor1.rgb, 0.5, alpha);
		srcColor1.a = 1.0 - alpha;
	}
#endif

	g_uavOutMip1[si.dispatchThreadID.xy] = PackColor(srcColor1);

	if (1 == g_ub._mipLevelCount)
		return;

	StoreColor(si.groupIndex, srcColor1);

	GroupMemoryBarrierWithGroupSync();

	if ((si.groupIndex & 0x9) == 0) // 16 threads:
	{
		const float4 srcColor2 = LoadColor(si.groupIndex + 0x01); // {+0, +1}
		const float4 srcColor3 = LoadColor(si.groupIndex + 0x08); // {+1, +0}
		const float4 srcColor4 = LoadColor(si.groupIndex + 0x09); // {+1, +1}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip2[si.dispatchThreadID.xy >> 1] = PackColor(srcColor1);
		StoreColor(si.groupIndex, srcColor1);
	}

	if (2 == g_ub._mipLevelCount)
		return;

	GroupMemoryBarrierWithGroupSync();

	if ((si.groupIndex & 0x1B) == 0) // 4 threads:
	{
		const float4 srcColor2 = LoadColor(si.groupIndex + 0x02); // {+0, +2}
		const float4 srcColor3 = LoadColor(si.groupIndex + 0x10); // {+2, +0}
		const float4 srcColor4 = LoadColor(si.groupIndex + 0x12); // {+2, +2}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip3[si.dispatchThreadID.xy >> 2] = PackColor(srcColor1);
		StoreColor(si.groupIndex, srcColor1);
	}

	if (3 == g_ub._mipLevelCount)
		return;

	GroupMemoryBarrierWithGroupSync();

	if (si.groupIndex == 0) // 1 thread:
	{
		const float4 srcColor2 = LoadColor(si.groupIndex + 0x04); // {+0, +4}
		const float4 srcColor3 = LoadColor(si.groupIndex + 0x20); // {+4, +0}
		const float4 srcColor4 = LoadColor(si.groupIndex + 0x24); // {+4, +4}
		srcColor1 = 0.25 * (srcColor1 + srcColor2 + srcColor3 + srcColor4);

		g_uavOutMip4[si.dispatchThreadID.xy >> 3] = PackColor(srcColor1);
	}
}
#endif

//@main:# (C)
//@main:#Exposure EXPOSURE (C)
