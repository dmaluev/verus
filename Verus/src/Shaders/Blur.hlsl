// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "Blur.inc.hlsl"

ConstantBuffer<UB_BlurVS> g_ubBlurVS : register(b0, space0);
ConstantBuffer<UB_BlurFS> g_ubBlurFS : register(b0, space1);

Texture2D    g_tex : register(t1, space1);
SamplerState g_sam : register(s1, space1);

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
	so.pos = float4(mul(si.pos, g_ubBlurVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBlurVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#if _ANISOTROPY_LEVEL > 4
	const int step = 2;
#else
	const int step = 4;
#endif

	float4 acc = 0.0;
#ifdef DEF_SSAO
	const float weightSum = 2.0;
	[unroll] for (int i = -1; i <= 1; i += 2)
#else
	float weightSum = _SINGULARITY_FIX;
	[unroll] for (int i = -8; i <= 7; i += step)
#endif
	{
#ifdef DEF_VERTICAL
		const int2 offset = int2(0, i);
#else
		const int2 offset = int2(i, 0);
#endif

#ifdef DEF_SSAO
		const float weight = 1.0;
#else
		const float weight = smoothstep(0.0, 1.0, (1.0 - abs(i) * (1.0 / 10.0)));
		weightSum += weight;
#endif

#if defined(DEF_SSAO) && defined(DEF_VERTICAL) // Sampling from sRGB, use alpha channel:
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.0, offset).aaaa;
#else
		acc += g_tex.SampleLevel(g_sam, si.tc0, 0.0, offset) * weight;
#endif
	}
	acc *= (1.0 / weightSum);

	so.color = acc;
#if defined(DEF_SSAO) && !defined(DEF_VERTICAL) // Make sure to write R channel to alpha, destination is sRGB.
	so.color = acc.rrrr;
#endif

	return so;
}
#endif

//@main:#H
//@main:#V VERTICAL
//@main:#HSsao SSAO
//@main:#VSsao SSAO VERTICAL
