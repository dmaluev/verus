// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibVertex.hlsl"
#include "Sky.inc.hlsl"

ConstantBuffer<UB_PerFrame>      g_ubPerFrame      : register(b0, space0);
ConstantBuffer<UB_PerMaterialFS> g_ubPerMaterialFS : register(b0, space1);
ConstantBuffer<UB_PerMeshVS>     g_ubPerMeshVS     : register(b0, space2);
ConstantBuffer<UB_PerObject>     g_ubPerObject     : register(b0, space3);

Texture2D    g_texSky      : register(t1, space1);
SamplerState g_samSky      : register(s1, space1);
Texture2D    g_texStars    : register(t2, space1);
SamplerState g_samStars    : register(s2, space1);
Texture2D    g_texClouds   : register(t3, space1);
SamplerState g_samClouds   : register(s3, space1);
Texture2D    g_texCloudsNM : register(t4, space1);
SamplerState g_samCloudsNM : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
};

struct VSO
{
	float4 pos      : SV_Position;
	float3 color0   : COLOR0;
	float2 tc0      : TEXCOORD0;
	float2 tcStars  : TEXCOORD1;
#ifdef DEF_CLOUDS
	float2 tcPhaseA : TEXCOORD2;
	float2 tcPhaseB : TEXCOORD3;
#endif
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float time = g_ubPerFrame._time_cloudiness_expo.x;

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);

	so.pos = mul(float4(intactPos, 1), g_ubPerObject._matWVP).xyww; // Peg the depth. (c) ATI
	so.color0 = pow(saturate(dot(normalize(intactPos), g_ubPerFrame._dirToSun.xyz)), 3.0);
	so.tc0 = float2(time, 0.5 - intactPos.y);
	so.tcStars = intactPos.xz * 16.0; // Stars.
#ifdef DEF_CLOUDS
	so.tcPhaseA = intactPos.xz * (4.0 - 4.0 * intactPos.y) + g_ubPerFrame._phaseAB.xy;
	so.tcPhaseB = intactPos.xz * (2.0 - 2.0 * intactPos.y) + g_ubPerFrame._phaseAB.zw;
#endif

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 rand = Rand(si.pos.xy);

	const float time = g_ubPerFrame._time_cloudiness_expo.x;
	const float cloudiness = g_ubPerFrame._time_cloudiness_expo.y;

	const float dayRatio = (1.0 - abs(0.5 - time) * 1.9);

#ifdef DEF_CLOUDS
	const float3 rawSky = g_texSky.Sample(g_samSky, float2(time, 0.3)).rgb;
	const float3 rawRim = g_texSky.Sample(g_samSky, float2(time, 0.4)).rgb;
	const float colorA = g_texClouds.Sample(g_samClouds, si.tcPhaseA).r;
	const float colorB = g_texClouds.Sample(g_samClouds, si.tcPhaseB).r;
	const float4 normalA = g_texCloudsNM.Sample(g_samCloudsNM, si.tcPhaseA);
	const float4 normalB = g_texCloudsNM.Sample(g_samCloudsNM, si.tcPhaseB);

	const float noon = saturate(g_ubPerFrame._dirToSun.y);
	const float noonHalf = noon * 0.5;
	const float avgColor = (colorA + colorB) * 0.5;
	const float3 cloudColor = saturate(Desaturate(rawSky, 0.4 + noonHalf) + 0.4 * noon); // White, bright clouds at noon.
	const float3 rimColor = saturate(rawRim * 2.0);

	float3 normal = 1.0 - (normalA.agb + normalB.agb);
	normal.b = ComputeNormalZ(normal.rg);
	normal.xyz = normal.xzy; // From normal-map space to world.

	const float clearSky = 1.0 - cloudiness;
	const float clearSkySoft = 0.1 + 0.9 * clearSky;
	const float alpha = saturate((avgColor - clearSky) * 4.0);

	const float sunBoost = si.color0.r * dayRatio;
	const float directGlow = saturate(dot(normal, g_ubPerFrame._dirToSun.xyz));
	const float edgeGlow = saturate((0.75 - alpha) * 1.5);
	const float ambientGlow = edgeGlow * 0.5 + noonHalf * 0.25 + sunBoost * dayRatio;
	const float glow = saturate(lerp(directGlow * 0.25, edgeGlow, noonHalf) + ambientGlow);
	const float diff = saturate((avgColor + directGlow + sunBoost * 0.25) * dayRatio * clearSkySoft);

	const float3 finalColor = saturate(diff * cloudColor + glow * rimColor * clearSkySoft);

	const float hdrScale = dayRatio * dayRatio * 1000.0;
	so.color.rgb = finalColor * hdrScale;
	so.color.a = alpha;
#else
	const float4 rawSky = g_texSky.Sample(g_samSky, si.tc0);
	const float3 skyColor = rawSky.rgb + rand * lerp(0.001, 0.01, rawSky.rgb);
	const float4 rawStars = g_texStars.Sample(g_samStars, si.tcStars);

	const float night = abs(0.5 - time) * 2.0;
	const float3 colorWithStars = skyColor + si.color0 * 0.1 + rawStars.rgb * night * night;

	const float hdrScale = dayRatio * dayRatio * 1000.0;
	so.color = float4(colorWithStars.rgb * hdrScale, 1);
#endif

	return so;
}
#endif

//@main:#Sky SKY
//@main:#Clouds CLOUDS
