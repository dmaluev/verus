// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "DS_Compose.inc.hlsl"

ConstantBuffer<UB_ComposeVS> g_ubComposeVS : register(b0, space0);
ConstantBuffer<UB_ComposeFS> g_ubComposeFS : register(b0, space1);

Texture2D    g_texGBuffer0 : register(t1, space1);
SamplerState g_samGBuffer0 : register(s1, space1);
Texture2D    g_texGBuffer1 : register(t2, space1);
SamplerState g_samGBuffer1 : register(s2, space1);
Texture2D    g_texGBuffer2 : register(t3, space1);
SamplerState g_samGBuffer2 : register(s3, space1);
Texture2D    g_texDepth    : register(t4, space1);
SamplerState g_samDepth    : register(s4, space1);
Texture2D    g_texAccDiff  : register(t5, space1);
SamplerState g_samAccDiff  : register(s5, space1);
Texture2D    g_texAccSpec  : register(t6, space1);
SamplerState g_samAccSpec  : register(s6, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos          : SV_Position;
	float2 tc0          : TEXCOORD0;
	float2 clipSpacePos : TEXCOORD1;
};

struct FSO
{
	float4 color : SV_Target0;
};

struct FSO2
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubComposeVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubComposeVS._matV).xy;
	so.clipSpacePos = so.pos.xy;

	return so;
}
#endif

#ifdef _FS
#ifdef DEF_COMPOSE
FSO2 mainFS(VSO si)
{
	FSO2 so;

	const float3 ambientColor = g_ubComposeFS._ambientColor_exposure.rgb;

	const float2 ndcPos = si.clipSpacePos.xy;

	// <Sample>
	const float4 rawGBuffer0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.f);
	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f);
	const float4 rawGBuffer2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.f);
	const float rawDepth = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f).r;
	const float4 rawAccDiff = g_texAccDiff.SampleLevel(g_samAccDiff, si.tc0, 0.f);
	const float4 rawAccSpec = g_texAccSpec.SampleLevel(g_samAccSpec, si.tc0, 0.f);
	// </Sample>

	// White color means 50% albedo, black is not really black:
	const float3 realAlbedo = ToRealAlbedo(rawGBuffer0.rgb);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float2 emission = DS_GetEmission(rawGBuffer1);
	const float3 posW = DS_GetPosition(rawDepth, g_ubComposeFS._matInvVP, ndcPos);
	const float depth = ToLinearDepth(rawDepth, g_ubComposeFS._zNearFarEx);

	const float ssaoDiff = 0.5f + 0.5f * rawGBuffer2.r;
	const float ssaoSpec = ssaoDiff;
	const float ssaoAmb = rawGBuffer2.r;

	const float3 normalW = mul(normalWV, (float3x3)g_ubComposeFS._matInvV);
	const float grayAmbient = Grayscale(ambientColor);
	const float3 finalAmbientColor = lerp(grayAmbient * 0.25f, ambientColor, normalW.y * 0.5f + 0.5f);

	const float3 color = realAlbedo * (rawAccDiff.rgb * ssaoDiff + finalAmbientColor * ssaoAmb + emission.x) + rawAccSpec.rgb * ssaoSpec;

	// <Underwater>
	float3 underwaterColor;
	{
		const float diffuseMask = ToWaterDiffuseMask(posW.y);
		const float refractMask = saturate((diffuseMask - 0.5f) * 2.f);
		const float3 waterDiffColorShallowAmbient = g_ubComposeFS._waterDiffColorShallow.rgb * ambientColor;
		const float3 waterDiffColorDeepAmbient = g_ubComposeFS._waterDiffColorDeep.rgb * ambientColor;
		const float deepAmbientColor = 0.5f + 0.5f * refractMask;
		const float3 planktonColor = lerp(
			waterDiffColorDeepAmbient * deepAmbientColor,
			waterDiffColorShallowAmbient,
			diffuseMask);
		underwaterColor = lerp(realAlbedo * planktonColor * 10.f, color, refractMask);
		const float fog = ComputeFog(depth, g_ubComposeFS._waterDiffColorShallow.a);
		underwaterColor = lerp(underwaterColor, planktonColor * 0.1f, fog * saturate(1.f - refractMask + g_ubComposeFS._waterDiffColorDeep.a));
	}
	// </Underwater>

	// <Fog>
	float3 colorWithFog;
	{
		const float fog = ComputeFog(depth, g_ubComposeFS._fogColor.a, posW.y);
		colorWithFog = lerp(underwaterColor, g_ubComposeFS._fogColor.rgb, fog);
	}
	// </Fog>

	so.target0.rgb = lerp(colorWithFog, realAlbedo, floor(rawDepth));
	so.target0.rgb = ToSafeHDR(so.target0.rgb);
	so.target0.a = 1.f;

	// <BackgroundColor>
	const float2 normalWasSet = ceil(rawGBuffer1.rg);
	const float bg = 1.f - saturate(normalWasSet.r + normalWasSet.g);
	so.target0.rgb = lerp(so.target0.rgb, g_ubComposeFS._backgroundColor.rgb, bg * g_ubComposeFS._backgroundColor.a);
	// </BackgroundColor>

	so.target1 = so.target0;

#if 0
	const float lightGlossScale = g_ubComposeFS._lightGlossScale.x;
	const float3 posWV = mul(float4(posW, 1), g_ubComposeFS._matV);
	const float3 toPosDirWV = normalize(posWV);
	const float dp = -toPosDirWV.z;
	const float smoothSpec = pow(saturate(dp), 4096.f * lightGlossScale);
	const float spec = saturate((smoothSpec - 0.5f) * 3.f + 0.5f);
	so.target0 = max(so.target0, spec * 30000.f);
#endif

	return so;
}
#endif
#ifdef DEF_TONE_MAPPING
FSO mainFS(VSO si)
{
	FSO so;

	// <Sample>
	const float4 rawGBuffer0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.f);
	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.f);
	const float4 rawGBuffer2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.f);
	float4 rawComposed = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.f);
	const float4 rawAccDiff = g_texAccDiff.SampleLevel(g_samAccDiff, si.tc0, 0.f);
	const float4 rawAccSpec = g_texAccSpec.SampleLevel(g_samAccSpec, si.tc0, 0.f);
	// </Sample>

	// Add reflection:
	const float specMask = rawGBuffer0.a;
	const float3 metalColor = ToMetalColor(rawGBuffer0.rgb);
	const float2 metalMask = DS_GetMetallicity(rawGBuffer2);
	rawComposed.rgb += rawAccSpec.rgb * lerp(1.f, metalColor, metalMask.x) * specMask;

	const float gray = Grayscale(rawComposed.rgb);
	const float3 exposedComposed = Desaturate(rawComposed.rgb, saturate(1.f - gray * 0.03f)) * g_ubComposeFS._ambientColor_exposure.a;
	so.color.rgb = VerusToneMapping(exposedComposed, 0.5f);
	so.color.a = 1.f;

	// Vignette:
	const float2 tcOffset = si.tc0 * 1.4f - 0.7f;
	const float lamp = saturate(dot(tcOffset, tcOffset));
	so.color.rgb = lerp(so.color.rgb, so.color.rgb * float3(0.64f, 0.48f, 0.36f), lamp);

	// SolidColor (using special value 1.f for emission):
	so.color.rgb = lerp(so.color.rgb, rawGBuffer0.rgb, floor(rawGBuffer1.b));

	const float3 bloom = rawAccDiff.rgb;
	so.color.rgb += bloom * (1.f - so.color.rgb);

	if (false)
	{
		const float gray = dot(rawComposed.rgb, 1.f / 3.f);
		so.color.r = saturate((gray - 2000.f) * 0.001f);
		so.color.gb *= 0.5f;
	}

	return so;
}
#endif
#endif

//@main:#Compose COMPOSE
//@main:#ToneMapping TONE_MAPPING
