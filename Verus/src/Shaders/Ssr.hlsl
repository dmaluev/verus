// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "LibLighting.hlsl"
#include "Ssr.inc.hlsl"

CBUFFER(0, UB_SsrVS, g_ubSsrVS)
CBUFFER(1, UB_SsrFS, g_ubSsrFS)

Texture2D    g_texGBuffer0 : REG(t1, space1, t0);
SamplerState g_samGBuffer0 : REG(s1, space1, s0);
Texture2D    g_texGBuffer1 : REG(t2, space1, t1);
SamplerState g_samGBuffer1 : REG(s2, space1, s1);
Texture2D    g_texGBuffer2 : REG(t3, space1, t2);
SamplerState g_samGBuffer2 : REG(s3, space1, s2);
Texture2D    g_texGBuffer3 : REG(t4, space1, t3);
SamplerState g_samGBuffer3 : REG(s4, space1, s3);
Texture2D    g_texScene    : REG(t5, space1, t4);
SamplerState g_samScene    : REG(s5, space1, s4);
Texture2D    g_texDepth    : REG(t6, space1, t5);
SamplerState g_samDepth    : REG(s6, space1, s5);
TextureCube  g_texImage    : REG(t7, space1, t6);
SamplerState g_samImage    : REG(s7, space1, s6);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos : SV_Position;
	float2 tc0 : TEXCOORD0;
};

struct FSO
{
	float4 color : SV_Target0;
};

float SoftBorderMask(float2 tc)
{
	const float2 mask = saturate((1.0 - abs(0.5 - tc)) * 10.0 - 5.0);
	return mask.x * mask.y;
}

float AdjustFade(float fade)
{
	const float x = 1.0 - fade;
	return 1.0 - x * x;
}

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubSsrVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubSsrVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 rand = Rand(si.pos.xy);
	const float2 ndcPos = ToNdcPos(si.tc0);

	const float radius = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.x;
	const float depthBias = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.y;
	const float thickness = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.z;
	const float equalizeDist = g_ubSsrFS._radius_depthBias_thickness_equalizeDist.w;

	// <Sample>
	const float4 gBuffer0Sam = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0);
	const float3 albedo = gBuffer0Sam.rgb;

	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float3 normalWV = DS_GetNormal(gBuffer1Sam);

	const float4 gBuffer2Sam = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0);
	const float occlusion = gBuffer2Sam.r;
	const float roughness = gBuffer2Sam.g;
	const float metallic = gBuffer2Sam.b;

	const float underwaterMask = g_texGBuffer3.SampleLevel(g_samGBuffer3, si.tc0, 0.0).b;

	const float depthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
	const float3 posWV = DS_GetPosition(depthSam, g_ubSsrFS._matInvP, ndcPos);
	// </Sample>

	const float originDepth = -posWV.z;
	const float3 rayFromEye = normalize(posWV);
	const float3 reflectedDir = reflect(rayFromEye, normalWV);
	const float3 reflectedDirW = mul(reflectedDir, (float3x3)g_ubSsrFS._matInvV);

#ifdef DEF_DEBUG_CUBE_MAP
	const float3 rayFromEyeW = mul(rayFromEye, (float3x3)g_ubSsrFS._matInvV);
	so.color = g_texImage.SampleLevel(g_samImage, rayFromEyeW, 0.0);
	return so;
#endif

	// <ImageBasedLighting>
	const float maxMipLevel = 9.0; // Assume 512 x 512.
	const float mip = RoughnessToMip(roughness, maxMipLevel);
	const float3 imageLo = g_texImage.SampleLevel(g_samImage, reflectedDirW, clamp(mip * maxMipLevel + 0.5, 0.0, maxMipLevel)).rgb;
	const float3 imageHi = g_texImage.SampleLevel(g_samImage, reflectedDirW, clamp(mip * maxMipLevel - 0.5, 0.0, maxMipLevel)).rgb;
	const float3 envColor = lerp(imageLo, imageHi, 1.0 / 3.0); // Smoother image.
	// </ImageBasedLighting>

	float4 color = float4(envColor * occlusion, 0);

#ifdef DEF_SSR
	const float nDosV = saturate(dot(normalWV, -rayFromEye));
	const float rim = 1.0 - nDosV;
	const float fresnel = rim * rim;

	const float mipFactor = 1.0 - 0.9 * mip;

	const float2 equalize = float2(equalizeDist, mipFactor) / float2(originDepth, max(0.01, length(reflectedDir.xy)));

	const float deepRadius = equalize.y * radius;
	const float deepThickness = equalize.y * thickness;

	const float invRadius = 1.0 / deepRadius;
#if _SHADER_QUALITY <= _Q_LOW
	const int maxSampleCount = 4;
#elif _SHADER_QUALITY <= _Q_MEDIUM
	const int maxSampleCount = 6;
#elif _SHADER_QUALITY <= _Q_HIGH
	const int maxSampleCount = 9;
#elif _SHADER_QUALITY <= _Q_ULTRA
	const int maxSampleCount = 12;
#endif
	const int sampleCount = clamp(maxSampleCount * equalize.x, 3, maxSampleCount);
	const float stride = deepRadius / sampleCount;
	const float3 rayStride = reflectedDir * stride;

	float3 kernelPosWV = posWV + normalWV * depthBias + rayStride * rand.x;
	float2 prevTcRaySample = si.tc0;
	float prevRayDeeper = stride * rand.x * 0.5; // Assumption.
	for (int i = 0; i < sampleCount; ++i)
	{
		float4 tcRaySample = mul(float4(kernelPosWV, 1), g_ubSsrFS._matPTex);
		tcRaySample /= tcRaySample.w;
		const float kernelDepthSam = g_texDepth.SampleLevel(g_samDepth, tcRaySample.xy, 0.0).r;

		const float2 rayDepth_texDepth = ToLinearDepth(float2(tcRaySample.z, kernelDepthSam), g_ubSsrFS._zNearFarEx);
		const float rayDeeper = rayDepth_texDepth.x - rayDepth_texDepth.y;

		if (rayDeeper > 0.0 && rayDeeper < deepThickness) // Ray deeper, but not too deep?
		{
			const float ratio = prevRayDeeper / (prevRayDeeper + rayDeeper);
			const float2 tcFinal = lerp(prevTcRaySample, tcRaySample.xy, ratio);

			const float tcMask = SoftBorderMask(tcFinal);

			const float fade = 1.0 - ((i + ratio) * stride * invRadius);

			const float alpha = tcMask * AdjustFade(fade) * fresnel * mipFactor;

			color.rgb = lerp(color.rgb, g_texScene.SampleLevel(g_samScene, tcFinal, 0.0).rgb, alpha);
			color.a = alpha;
			break;
		}

		prevTcRaySample = tcRaySample.xy;
		prevRayDeeper = abs(rayDeeper);
		kernelPosWV += rayStride;
	}
#endif

	color.rgb = VerusIBL(normalWV, -rayFromEye,
		albedo, color.rgb,
		roughness, metallic);
	const float cavity = ComputeCavity(albedo, roughness);

	const float fog = ComputeFog(originDepth, g_ubSsrFS._fogColor.a);

	so.color.rgb = color.rgb * cavity * (1.0 - underwaterMask) * (1.0 - fog);
	so.color.a = color.a;

	return so;
}
#endif

//@main:#
//@main:#DebugCubeMap DEBUG_CUBE_MAP
