// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "Cinema.inc.hlsl"

ConstantBuffer<UB_CinemaVS> g_ubCinemaVS : register(b0, space0);
ConstantBuffer<UB_CinemaFS> g_ubCinemaFS : register(b0, space1);

Texture2D    g_texFilmGrain : register(t1, space1);
SamplerState g_samFilmGrain : register(s1, space1);

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

	const float3 rawFilmGrain = g_texFilmGrain.Sample(g_samFilmGrain, si.tcFilmGrain).rgb;
	const float3 filmGrain = lerp(1.f, 0.5f + rawFilmGrain.rgb, noiseStrength);

	// Vignette:
	const float2 tcOffset = si.tc0 * 1.4f - 0.7f;
	const float lamp = saturate(dot(tcOffset, tcOffset));
	const float3 lampColor = lerp(1.f, float3(0.64f, 0.48f, 0.36f), lamp);

	so.color.rgb = saturate(filmGrain * lampColor * brightness);
	so.color.a = saturate(dot(filmGrain, 1.f / 3.f) - 1.f);

	return so;
}
#endif

//@main:#
