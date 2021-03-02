// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
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

struct VSI_SKY_BODY
{
	VK_LOCATION_POSITION float4 pos : POSITION;
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

struct VSO_SKY_BODY
{
	float4 pos   : SV_Position;
	float2 tc0   : TEXCOORD0;
	float height : TEXCOORD1;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);

	so.pos = mul(float4(intactPos, 1), g_ubPerObject._matWVP).xyww; // Peg the depth. (c) ATI
	so.color0 = pow(saturate(dot(normalize(intactPos * float3(1, 0.25f, 1)), g_ubPerFrame._dirToSun.xyz)), 4.f);
	so.tc0 = float2(g_ubPerFrame._time_cloudiness.x, 0.5f - intactPos.y);
	so.tcStars = intactPos.xz * 16.f; // Stars.
#ifdef DEF_CLOUDS
	so.tcPhaseA = intactPos.xz * (4.f - 4.f * intactPos.y) + g_ubPerFrame._phaseAB.xy;
	so.tcPhaseB = intactPos.xz * (2.f - 2.f * intactPos.y) + g_ubPerFrame._phaseAB.zw;
#endif

	return so;
}

VSO_SKY_BODY mainSkyBodyVS(VSI_SKY_BODY si)
{
	VSO_SKY_BODY so;

	so.pos = mul(si.pos, g_ubPerObject._matWVP).xyww;
	so.tc0 = si.tc0.xy;
	so.height = mul(si.pos.xyz, (float3x3)g_ubPerObject._matW).y;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float3 rand = Rand(si.pos.xy);

	const float time = g_ubPerFrame._time_cloudiness.x;
	const float cloudiness = g_ubPerFrame._time_cloudiness.y;

	const float dayRatio = (1.f - abs(0.5f - time) * 1.9f);
	const float sunAlpha = g_ubPerFrame._sunColor.a;
	const float sunBoost = si.color0.r * sunAlpha;

#ifdef DEF_CLOUDS
	const float3 rawSky = g_texSky.Sample(g_samSky, float2(time, 0.3f)).rgb;
	const float3 rawRim = g_texSky.Sample(g_samSky, float2(time, 0.4f)).rgb;
	const float colorA = g_texClouds.Sample(g_samClouds, si.tcPhaseA).r;
	const float colorB = g_texClouds.Sample(g_samClouds, si.tcPhaseB).r;
	const float4 normalA = g_texCloudsNM.Sample(g_samCloudsNM, si.tcPhaseA);
	const float4 normalB = g_texCloudsNM.Sample(g_samCloudsNM, si.tcPhaseB);

	const float noon = saturate(g_ubPerFrame._dirToSun.y);
	const float noonHalf = noon * 0.5f;
	const float avgColor = (colorA + colorB) * 0.5f;
	const float negative = 1.f - avgColor;
	const float3 cloudColor = saturate(Desaturate(rawSky, 0.4f + noonHalf) + 0.4f * noon); // White, bright clouds at noon.
	const float3 rimColor = saturate(rawRim * 2.f);

	float3 normal = 1.f - (normalA.agb + normalB.agb);
	normal.b = ComputeNormalZ(normal.rg);
	normal.xyz = normal.xzy; // From normal-map space to world.

	const float clearSky = 1.f - cloudiness;
	const float clearSkySoft = 0.6f + 0.4f * clearSky;
	const float alpha = saturate((avgColor - clearSky) * 3.f);

	const float directLight = saturate(dot(normal, g_ubPerFrame._dirToSun.xyz)) * sunAlpha;
	const float edgeMask = saturate((0.9f - alpha) * 2.f);
	const float glowAmbient = edgeMask * 0.1f + noonHalf * 0.25f + sunBoost * dayRatio;
	const float glow = saturate(lerp(directLight * directLight, edgeMask, noonHalf) + glowAmbient);
	const float diff = saturate((0.6f + 0.4f * (max(directLight, negative) + sunBoost)) * dayRatio * clearSkySoft);

	const float3 finalColor = saturate(diff * cloudColor + glow * rimColor * clearSkySoft * 0.25f);

	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * (10.f - 2.f * cloudiness);
	so.color.rgb = finalColor * hdrScale;
	so.color.a = alpha;

	// <Fog>
	{
		const float cloudScale = cloudiness * cloudiness;
		const float fogBias = 0.01f + 0.49f * cloudScale;
		const float fogContrast = 1.f + 2.f * cloudScale;
		const float fog = saturate((si.tc0.y - 0.5f + fogBias) * (fogContrast / fogBias));
		so.color.rgb = lerp(so.color.rgb, g_ubPerFrame._fogColor.rgb, fog);
		so.color.a = max(so.color.a, fog);
	}
	// </Fog>
#else
	const float4 rawSky = g_texSky.Sample(g_samSky, si.tc0);
	const float3 skyColor = rawSky.rgb + rand * lerp(0.001f, 0.01f, rawSky.rgb); // Dithering.
	const float4 rawStars = g_texStars.Sample(g_samStars, si.tcStars);

	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * (5.5f + 4.5f * sunBoost);
	so.color = float4(skyColor.rgb * hdrScale + rawStars.rgb * 50.f, 1);
#endif

	so.color.rgb = ToSafeHDR(so.color.rgb);

	return so;
}

FSO mainSkyBodyFS(VSO_SKY_BODY si)
{
	FSO so;

	so.color = g_texClouds.Sample(g_samClouds, si.tc0);
	const float mask = abs(si.height);
#ifdef DEF_SUN
	so.color.rgb *= lerp(float3(1.f, 0.2f, 0.001f), 1.f, mask);
	so.color.rgb *= (60.f * 1000.f) + (100.f * 1000.f) * mask;
	so.color.rgb = ToSafeHDR(so.color.rgb);
#endif
#ifdef DEF_MOON
	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * 12.f;
	so.color.rgb *= lerp(float3(1.f, 0.9f, 0.8f), 1.f, mask);
	so.color.rgb *= max(100.f, hdrScale);
	so.color.a *= mask;
#endif

	return so;
}
#endif

//@main:#Sky SKY
//@main:#Clouds CLOUDS
//@mainSkyBody:#Sun SUN
//@mainSkyBody:#Moon MOON
