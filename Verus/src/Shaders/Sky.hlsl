// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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

	const float3 inPos = DequantizeUsingDeq3D(si.pos.xyz, g_ubPerMeshVS._posDeqScale.xyz, g_ubPerMeshVS._posDeqBias.xyz);

	so.pos = mul(float4(inPos, 1), g_ubPerObject._matWVP).xyww; // Peg the depth. (c) ATI
	so.color0 = pow(saturate(dot(normalize(inPos * float3(1, 0.25, 1)), g_ubPerFrame._dirToSun.xyz)), 4.0);
	so.tc0 = float2(g_ubPerFrame._time_cloudiness.x, 0.5 - inPos.y);
	so.tcStars = inPos.xz * 16.0; // Stars.
#ifdef DEF_CLOUDS
	so.tcPhaseA = inPos.xz * (4.0 - 4.0 * inPos.y) + g_ubPerFrame._phaseAB.xy;
	so.tcPhaseB = inPos.xz * (2.0 - 2.0 * inPos.y) + g_ubPerFrame._phaseAB.zw;
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

	const float dayRatio = (1.0 - abs(0.5 - time) * 1.9);
	const float sunAlpha = g_ubPerFrame._sunColor.a;
	const float sunBoost = si.color0.r * sunAlpha;

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
	const float negative = 1.0 - avgColor;
	const float3 cloudColor = saturate(Desaturate(rawSky, 0.4 + noonHalf) + 0.4 * noon); // White, bright clouds at noon.
	const float3 rimColor = saturate(rawRim * 2.0);

	float3 normal = 1.0 - (normalA.agb + normalB.agb);
	normal.b = ComputeNormalZ(normal.rg);
	normal.xyz = normal.xzy; // From normal-map space to world.

	const float clearSky = 1.0 - cloudiness;
	const float clearSkySoft = 0.6 + 0.4 * clearSky;
	const float alpha = saturate((avgColor - clearSky) * 3.0);

	const float directLight = saturate(dot(normal, g_ubPerFrame._dirToSun.xyz)) * sunAlpha;
	const float edgeMask = saturate((0.9 - alpha) * 2.0);
	const float glowAmbient = edgeMask * 0.1 + noonHalf * 0.25 + sunBoost * dayRatio;
	const float glow = saturate(lerp(directLight * directLight, edgeMask, noonHalf) + glowAmbient);
	const float diff = saturate((0.6 + 0.4 * (max(directLight, negative) + sunBoost)) * dayRatio * clearSkySoft);

	const float3 finalColor = saturate(diff * cloudColor + glow * rimColor * clearSkySoft * 0.25);

	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * (10.0 - 6.0 * cloudiness);
	so.color.rgb = finalColor * hdrScale;
	so.color.a = alpha;

	// <Fog>
	{
		const float cloudScale = cloudiness * cloudiness;
		const float fogBias = 0.01 + 0.49 * cloudScale;
		const float fogContrast = 1.0 + 2.0 * cloudScale;
		const float fog = saturate((si.tc0.y - 0.5 + fogBias) * (fogContrast / fogBias));
		so.color.rgb = lerp(so.color.rgb, g_ubPerFrame._fogColor.rgb, fog);
		so.color.a = max(so.color.a, fog);
	}
	// </Fog>
#else
	const float4 rawSky = g_texSky.Sample(g_samSky, si.tc0);
	const float3 skyColor = rawSky.rgb + rand * lerp(0.001, 0.01, rawSky.rgb); // Dithering.
	const float4 rawStars = g_texStars.Sample(g_samStars, si.tcStars);

	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * (4.0 + 3.0 * sunBoost);
	so.color = float4(skyColor.rgb * hdrScale + rawStars.rgb * 50.0, 1);
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
	so.color.rgb *= lerp(float3(1.0, 0.2, 0.001), 1.0, mask);
	so.color.rgb *= (60.0 * 1000.0) + (100.0 * 1000.0) * mask;
	so.color.rgb = ToSafeHDR(so.color.rgb);
#endif
#ifdef DEF_MOON
	const float hdrScale = Grayscale(g_ubPerFrame._ambientColor.xyz) * 12.0;
	so.color.rgb *= lerp(float3(1.0, 0.9, 0.8), 1.0, mask);
	so.color.rgb *= max(100.0, hdrScale);
	so.color.a *= mask;
#endif

	return so;
}
#endif

//@main:#Sky SKY
//@main:#Clouds CLOUDS
//@mainSkyBody:#Sun SUN
//@mainSkyBody:#Moon MOON
