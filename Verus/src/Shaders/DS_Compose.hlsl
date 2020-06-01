// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

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
	float4 pos : SV_Position;
	float2 tc0 : TEXCOORD0;
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

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, si.tc0);
	const float4 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, si.tc0);
	const float1 rawDepth = g_texDepth.Sample(g_samDepth, si.tc0).r;

	const float4 rawAccDiff = g_texAccDiff.Sample(g_samAccDiff, si.tc0);
	const float4 rawAccSpec = g_texAccSpec.Sample(g_samAccSpec, si.tc0);
	const float4 accDiff = rawAccDiff;
	const float4 accSpec = rawAccSpec;

	// White color means 50% albedo, black is not really black:
	const float3 albedo = max(rawGBuffer0.rgb * 0.5, 0.0001);
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float2 emission = DS_GetEmission(rawGBuffer1);
	const float3 posW = DS_GetPosition(rawDepth, g_ubComposeFS._matInvVP, ndcPos);
	const float depth = ToLinearDepth(rawDepth, g_ubComposeFS._zNearFarEx);

	const float ssaoDiff = 1.0;
	const float ssaoSpec = 1.0;
	const float ssaoAmb = 1.0;

	const float3 normalW = mul(normalWV, (float3x3)g_ubComposeFS._matInvV);
	const float grayAmbient = Grayscale(ambientColor);
	const float3 finalAmbientColor = lerp(grayAmbient * 0.5, ambientColor, normalW.y * 0.5 + 0.5);

	const float3 color =
		albedo * (accDiff.rgb * ssaoDiff + finalAmbientColor * ssaoAmb) +
		accSpec.rgb * ssaoSpec +
		albedo * emission.x;

	// <Fog>
	float3 colorWithFog;
	{
		const float fog = ComputeFog(depth, g_ubComposeFS._fogColor.a, posW.y);
		colorWithFog = lerp(color, g_ubComposeFS._fogColor.rgb, fog);
	}
	// </Fog>

	// <Underwater>
	float3 underwaterColor;
	{
		const float diffuseMask = ToWaterDiffuseMask(posW.y);
		const float refractMask = saturate((diffuseMask - 0.5) * 2.0);
		const float3 waterDiffColorShallowAmbient = g_ubComposeFS._waterDiffColorShallow.rgb * ambientColor;
		const float3 waterDiffColorDeepAmbient = g_ubComposeFS._waterDiffColorDeep.rgb * ambientColor;
		const float deepAmbientColor = 0.5 + 0.5 * refractMask;
		const float3 planktonColor = lerp(
			waterDiffColorDeepAmbient * deepAmbientColor,
			waterDiffColorShallowAmbient,
			diffuseMask);
		const float3 color = lerp(albedo * planktonColor * 10.0, colorWithFog, refractMask);
		const float fog = ComputeFog(depth, g_ubComposeFS._waterDiffColorShallow.a);
		underwaterColor = lerp(color, planktonColor * 0.1, fog * saturate(1.0 - refractMask + g_ubComposeFS._waterDiffColorDeep.a));
	}
	// </Underwater>

	so.target0.rgb = underwaterColor;
	so.target0.a = 1.0;

	so.target1 = so.target0;

	return so;
}
#endif
#ifdef DEF_TONE_MAPPING
FSO mainFS(VSO si)
{
	FSO so;

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, si.tc0);
	const float4 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, si.tc0);
	const float4 rawComposed = g_texDepth.Sample(g_samDepth, si.tc0);

	const float3 exposedComposed = rawComposed.rgb * g_ubComposeFS._ambientColor_exposure.a;
	so.color.rgb = VerusToneMapping(exposedComposed, 0.5);
	so.color.a = 1.0;

	// SolidColor (using special value 1.0 for emission):
	so.color.rgb = lerp(so.color.rgb, rawGBuffer0.rgb, floor(rawGBuffer1.b));

	// <BackgroundColor>
	const float2 normalWasSet = ceil(rawGBuffer1.rg);
	const float bg = 1.0 - saturate(normalWasSet.r + normalWasSet.g);
	so.color.rgb = lerp(so.color.rgb, g_ubComposeFS._backgroundColor.rgb, bg * g_ubComposeFS._backgroundColor.a);
	// </BackgroundColor>

	if (false)
	{
		const float gray = Grayscale(rawComposed.rgb);
		so.color.r = saturate((gray - 25.0) * 0.001);
		so.color.gb *= 0.5;
	}

	return so;
}
#endif
#endif

//@main:#Compose COMPOSE
//@main:#ToneMapping TONE_MAPPING
