// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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
Texture2D    g_texAccAmb   : register(t5, space1);
SamplerState g_samAccAmb   : register(s5, space1);
Texture2D    g_texAccDiff  : register(t6, space1);
SamplerState g_samAccDiff  : register(s6, space1);
Texture2D    g_texAccSpec  : register(t7, space1);
SamplerState g_samAccSpec  : register(s7, space1);

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

struct FSO3
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
	float4 target2 : SV_Target2;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubComposeVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubComposeVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
#ifdef DEF_COMPOSE
FSO3 mainFS(VSO si)
{
	FSO3 so;
	so.target0 = 0.0;
	so.target1 = 0.0;
	so.target2 = 0.0;

	const float2 ndcPos = ToNdcPos(si.tc0);

	// <Sample>
	const float4 gBuffer0Sam = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0);
	const float3 albedo = gBuffer0Sam.rgb;

	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float emission = DS_GetEmission(gBuffer1Sam);

	const float4 gBuffer2Sam = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0);
	const float occlusion = gBuffer2Sam.r;

	const float depthSam = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).r;
	const float3 posW = DS_GetPosition(depthSam, g_ubComposeFS._matInvVP, ndcPos);
	const float depth = ToLinearDepth(depthSam, g_ubComposeFS._zNearFarEx);

	const float4 accAmbSam = g_texAccAmb.SampleLevel(g_samAccAmb, si.tc0, 0.0);
	const float4 accDiffSam = g_texAccDiff.SampleLevel(g_samAccDiff, si.tc0, 0.0);
	const float4 accSpecSam = g_texAccSpec.SampleLevel(g_samAccSpec, si.tc0, 0.0);
	const float3 ambientColor = accAmbSam.rgb * occlusion;
	// </Sample>

	const float3 color = albedo * (ambientColor + accDiffSam.rgb + emission) + accSpecSam.rgb;

	// <Underwater>
	float3 underwaterColor;
	{
		const float planktonMask = ToWaterPlanktonMask(posW.y);
		const float refractMask = saturate((planktonMask - 0.5) * 2.0);
		const float3 waterDiffColorShallowAmbient = g_ubComposeFS._waterDiffColorShallow.rgb * ambientColor;
		const float3 waterDiffColorDeepAmbient = g_ubComposeFS._waterDiffColorDeep.rgb * ambientColor;
		const float deepAmbientColor = 0.5 + 0.5 * refractMask;
		const float3 planktonColor = lerp(
			waterDiffColorDeepAmbient * deepAmbientColor,
			waterDiffColorShallowAmbient,
			planktonMask);
		underwaterColor = lerp(albedo * planktonColor * 10.0, color, refractMask);
		const float fog = ComputeFog(depth, g_ubComposeFS._waterDiffColorShallow.a);
		underwaterColor = lerp(underwaterColor, planktonColor * 0.1, fog * saturate(1.0 - refractMask + g_ubComposeFS._waterDiffColorDeep.a));
	}
	// </Underwater>

	// <Fog>
	float3 colorWithFog;
	{
		const float fog = ComputeFog(depth, g_ubComposeFS._fogColor.a, posW.y);
		colorWithFog = lerp(underwaterColor, g_ubComposeFS._fogColor.rgb, fog);
	}
	// </Fog>

	so.target0.rgb = lerp(colorWithFog, albedo, floor(depthSam));

	// <BackgroundColor>
	const float2 normalWasSet = ceil(gBuffer1Sam.rg);
	const float backgroundMask = 1.0 - saturate(normalWasSet.r + normalWasSet.g);
	so.target0.rgb = lerp(so.target0.rgb, g_ubComposeFS._backgroundColor.rgb, backgroundMask * g_ubComposeFS._backgroundColor.a);
	// </BackgroundColor>

	so.target1 = so.target0; // Second copy for things like water refraction.
	so.target2.b = (1.0 - saturate(posW.y * 4.0)) * g_ubComposeFS._exposure_underwaterMask.y;

	return so;
}
#endif
#ifdef DEF_TONE_MAPPING
FSO mainFS(VSO si)
{
	FSO so;

	// <Sample>
	const float4 gBuffer0Sam = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0);
	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	float3 composed;
	{
#ifdef DEF_CINEMA
		// Chromatic aberration:
		const float2 offset = (0.5 - si.tc0) * 0.0015;
		const float2 tcR = si.tc0 + offset;
		const float2 tcG = si.tc0;
		const float2 tcB = si.tc0 - offset;
		composed.r = g_texDepth.SampleLevel(g_samDepth, tcR, 0.0).r;
		composed.g = g_texDepth.SampleLevel(g_samDepth, tcG, 0.0).g;
		composed.b = g_texDepth.SampleLevel(g_samDepth, tcB, 0.0).b;
#else
		composed = g_texDepth.SampleLevel(g_samDepth, si.tc0, 0.0).rgb;
#endif
	}
#ifdef DEF_BLOOM
	const float4 gBuffer2Sam = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0);
#endif
	// </Sample>

	const float gray = Grayscale(composed);
	const float3 exposedComposed = Desaturate(composed, saturate(1.0 - gray * 0.05)) * g_ubComposeFS._exposure_underwaterMask.x;
	so.color.rgb = VerusToneMapping(exposedComposed, 0.5);
	so.color.a = 1.0;

	// SolidColor (using special value 1 for emission):
	so.color.rgb = lerp(so.color.rgb, gBuffer0Sam.rgb, floor(gBuffer1Sam.b));

#ifdef DEF_BLOOM
	const float3 bloom = gBuffer2Sam.rgb;
	so.color.rgb += bloom * (1.0 - so.color.rgb);
#endif

	if (false)
	{
		const float gray = dot(composed, 1.0 / 3.0);
		so.color.r = saturate((gray - 5000.0) * 0.001);
		so.color.gb *= 0.5;
	}

	return so;
}
#endif
#endif

//@main:#Compose COMPOSE
//@main:#ToneMapping TONE_MAPPING
