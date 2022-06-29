// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "Cinema.inc.hlsl"

CBUFFER(0, UB_CinemaVS, g_ubCinemaVS)
CBUFFER(1, UB_CinemaFS, g_ubCinemaFS)

Texture2D    g_texFilmGrain : REG(t1, space1, t0);
SamplerState g_samFilmGrain : REG(s1, space1, s0);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos         : SV_Position;
	float2 tc0         : TEXCOORD0;
	float2 tcFilmGrain : TEXCOORD1;
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
	so.pos = float4(mul(si.pos, g_ubCinemaVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubCinemaVS._matV).xy;
	so.tcFilmGrain = mul(si.pos, g_ubCinemaVS._matP).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float brightness = g_ubCinemaFS._brightness_noiseStrength.x;
	const float noiseStrength = g_ubCinemaFS._brightness_noiseStrength.y;

	const float3 filmGrainSam = g_texFilmGrain.Sample(g_samFilmGrain, si.tcFilmGrain).rgb;
	const float3 filmGrain = lerp(1.0, 0.5 + filmGrainSam.rgb, noiseStrength);

	// Vignette:
	const float2 tcOffset = si.tc0 * 1.4 - 0.7;
	const float lamp = saturate(dot(tcOffset, tcOffset));
	const float3 lampColor = lerp(1.0, float3(0.64, 0.48, 0.36), lamp);

	so.color.rgb = saturate(filmGrain * lampColor * brightness);
	so.color.a = saturate(dot(filmGrain, 1.0 / 3.0) - 1.0);

	return so;
}
#endif

//@main:#
