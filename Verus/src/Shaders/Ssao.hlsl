// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "Ssao.inc.hlsl"

ConstantBuffer<UB_SsaoVS> g_ubSsaoVS : register(b0, space0);
ConstantBuffer<UB_SsaoFS> g_ubSsaoFS : register(b0, space1);

Texture2D    g_texRandNormals : register(t1, space1);
SamplerState g_samRandNormals : register(s1, space1);
Texture2D    g_texGBuffer1    : register(t2, space1);
SamplerState g_samGBuffer1    : register(s2, space1);
Texture2D    g_texDepth       : register(t3, space1);
SamplerState g_samDepth       : register(s3, space1);

// Normalized diagonal 1/sqrt(1+1+1):
static const float g_s = 0.57735;
static const float3 g_rays[8] =
{
	float3(+g_s, +g_s, +g_s),
	float3(+g_s, +g_s, -g_s),
	float3(+g_s, -g_s, +g_s),
	float3(+g_s, -g_s, -g_s),
	float3(-g_s, +g_s, +g_s),
	float3(-g_s, +g_s, -g_s),
	float3(-g_s, -g_s, +g_s),
	float3(-g_s, -g_s, -g_s)
};

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos    : SV_Position;
	float2 tc0    : TEXCOORD0;
	float2 tcNorm : TEXCOORD1;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad & wrapped random normal:
	so.pos = float4(mul(si.pos, g_ubSsaoVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubSsaoVS._matV).xy;
	so.tcNorm = mul(si.pos, g_ubSsaoVS._matP).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 randNormalWV = g_texRandNormals.SampleLevel(g_samRandNormals, si.tcNorm, 0.0).xyz * 2.0 - 1.0;

	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);

	const float rawDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
	const float originDepth = ToLinearDepth(rawDepth, g_ubSsaoFS._zNearFarEx);

	const float smallRad = 0.02;
	const float largeRad = 0.06;
	const float perspectScale = max(1.0 / originDepth, 0.05);
	const float4 scale = float4(smallRad, smallRad, largeRad, largeRad) * perspectScale * g_ubSsaoFS._camScale.xyxy;

	float2 acc = _SINGULARITY_FIX;
	[unroll] for (uint i = 0; i < 8; i++)
	{
		const float3 randRayWV = reflect(g_rays[i], randNormalWV);
		const float3 hemiRayWV = randRayWV + normalWV * saturate(-dot(normalWV, randRayWV)) * 2.0;
		const float nDotR = saturate(dot(normalWV, hemiRayWV));

		const float3 smallRay = hemiRayWV * float3(scale.xy, smallRad);
		const float3 largeRay = hemiRayWV * float3(scale.zw, largeRad);

		const float2 rawKernelDepths = float2(
			g_texDepth.SampleLevel(g_samDepth, si.tc0 + smallRay.xy, 0.0).r,
			g_texDepth.SampleLevel(g_samDepth, si.tc0 + largeRay.xy, 0.0).r);
		const float2 kernelDepths = ToLinearDepth(rawKernelDepths, g_ubSsaoFS._zNearFarEx);

		const float2 kernelDeeper = kernelDepths - originDepth;
		const float2 rayCloser = kernelDeeper + float2(smallRay.z, largeRay.z);
		const float2 visible = step(0.0, rayCloser);
		const float weight = saturate(1.2 + 5.0 * rayCloser.y) * nDotR;

		acc += float2(min(visible.x, visible.y) * weight, weight);
	}

	so.color = acc.x / acc.y;

	return so;
}
#endif

//@main:#
