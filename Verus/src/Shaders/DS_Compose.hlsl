#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibDepth.hlsl"
#include "DS_Compose.inc.hlsl"

ConstantBuffer<UB_ComposePerObject> g_composePerObject : register(b0, space0);

Texture2D    g_texGBuffer0 : register(t1, space0);
SamplerState g_samGBuffer0 : register(s1, space0);
Texture2D    g_texGBuffer1 : register(t2, space0);
SamplerState g_samGBuffer1 : register(s2, space0);
Texture2D    g_texGBuffer2 : register(t3, space0);
SamplerState g_samGBuffer2 : register(s3, space0);
Texture2D    g_texGBuffer3 : register(t4, space0);
SamplerState g_samGBuffer3 : register(s4, space0);
Texture2D    g_texAccDiff  : register(t5, space0);
SamplerState g_samAccDiff  : register(s5, space0);
Texture2D    g_texAccSpec  : register(t6, space0);
SamplerState g_samAccSpec  : register(s6, space0);

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

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_composePerObject._matW), 1);
	so.tc0 = mul(si.pos, g_composePerObject._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, si.tc0);
	const float1 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, si.tc0).r; // Depth for fog.
	const float4 rawGBuffer2 = g_texGBuffer2.Sample(g_samGBuffer2, si.tc0);
	const float3 normalWV = DS_GetNormal(rawGBuffer2);
	const float2 emission = DS_GetEmission(rawGBuffer2);

	const float4 rawAccDiff = g_texAccDiff.Sample(g_samAccDiff, si.tc0);
	const float4 rawAccSpec = g_texAccSpec.Sample(g_samAccSpec, si.tc0);

	const float ssaoDiff = 1.0;
	const float ssaoSpec = 1.0;
	const float ssaoAmb = 1.0;

	const float3 albedo = rawGBuffer0.rgb;

	const float depth = ToLinearDepth(rawGBuffer1, g_composePerObject._zNearFarEx);
	const float fog = ComputeFog(depth, g_composePerObject._fogColor.a);
	const float3 normalW = mul(normalWV, (float3x3)g_composePerObject._matInvV);
	const float grey = Greyscale(g_composePerObject._colorAmbient);
	const float3 colorGround = lerp(g_composePerObject._colorAmbient, float3(1, 0.88, 0.47)*grey, saturate(grey*14.0));
	const float3 colorAmbient = lerp(colorGround, g_composePerObject._colorAmbient, saturate(normalW.y*2.0 + 0.5));
	const float3 color = lerp(
		ApplyEmission(albedo, emission.x)*saturate(rawAccDiff.rgb*ssaoDiff + colorAmbient * ssaoAmb) + rawAccSpec.rgb*ssaoSpec,
		g_composePerObject._fogColor.rgb,
		fog);

	so.color.rgb = color;
	so.color.a = 1.0;

	return so;
}
#endif

//@main:T
