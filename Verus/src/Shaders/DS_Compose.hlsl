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
FSO mainFS(VSO si)
{
	FSO so;

#ifdef DEF_COMPOSE
	const float3 colorAmbient = g_ubComposeFS._colorAmbient.rgb;

	const float2 ndcPos = si.clipSpacePos.xy;

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, si.tc0);
	const float4 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, si.tc0);
	const float4 rawGBuffer2 = g_texGBuffer2.Sample(g_samGBuffer2, si.tc0);
	const float1 rawDepth = g_texDepth.Sample(g_samDepth, si.tc0).r;
	const float3 normalWV = DS_GetNormal(rawGBuffer1);
	const float2 emission = DS_GetEmission(rawGBuffer1);

	const float4 rawAccDiff = g_texAccDiff.Sample(g_samAccDiff, si.tc0);
	const float4 rawAccSpec = g_texAccSpec.Sample(g_samAccSpec, si.tc0);
	const float4 accDiff = rawAccDiff;
	const float4 accSpec = rawAccSpec;

	const float ssaoDiff = 1.0;
	const float ssaoSpec = 1.0;
	const float ssaoAmb = 1.0;

	const float3 albedo = max(rawGBuffer0.rgb * g_ubComposeFS._toneMappingConfig.z, 0.0001);

	const float depth = ToLinearDepth(rawDepth, g_ubComposeFS._zNearFarEx);
	const float3 posW = DS_GetPosition(rawDepth, g_ubComposeFS._matInvVP, ndcPos);
	const float fog = ComputeFog(depth, g_ubComposeFS._fogColor.a, posW.y);

	const float3 normalW = mul(normalWV, (float3x3)g_ubComposeFS._matInvV);
	const float grayAmbient = Grayscale(colorAmbient);
	const float3 colorGround = lerp(colorAmbient, float3(1, 0.88, 0.47) * grayAmbient, saturate(grayAmbient * 14.0));
	const float3 colorAmbientFinal = lerp(colorGround, colorAmbient, saturate(normalW.y * 2.0 + 0.5));

	const float3 color =
		albedo * (accDiff.rgb * ssaoDiff + colorAmbientFinal * ssaoAmb) +
		accSpec.rgb * ssaoSpec +
		albedo * emission.x;

	const float3 colorWithFog = lerp(color, g_ubComposeFS._fogColor.rgb, fog);

	so.color.rgb = colorWithFog;
	so.color.rgb = lerp(so.color.rgb, g_ubComposeFS._colorBackground.rgb, floor(rawDepth) * g_ubComposeFS._colorBackground.a);
	so.color.a = 1.0;
#endif

#ifdef DEF_TONE_MAPPING
	const float4 rawComposed = g_texGBuffer0.Sample(g_samGBuffer0, si.tc0);

	so.color.rgb = VerusToneMapping(rawComposed.rgb * g_ubComposeFS._toneMappingConfig.x, g_ubComposeFS._toneMappingConfig.y);
	so.color.a = 1.0;

#endif

	return so;
}
#endif

//@main:#Compose COMPOSE
//@main:#ToneMapping TONE_MAPPING
