// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDepth.hlsl"
#include "Particles.inc.hlsl"

ConstantBuffer<UB_ParticlesVS> g_ubParticlesVS : register(b0, space0);
ConstantBuffer<UB_ParticlesFS> g_ubParticlesFS : register(b0, space1);

Texture2D    g_tex : register(t1, space1);
SamplerState g_sam : register(s1, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos   : POSITION;
	VK_LOCATION(8)       float2 tc0   : TEXCOORD0;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
	VK_LOCATION_PSIZE    float psize : PSIZE;
};

struct VSO
{
	float4 pos      : SV_Position;
	float2 tc0      : TEXCOORD0;
	float  additive : TEXCOORD1;
	float4 color0   : COLOR0;
	float2 psize    : PSIZE;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	so.pos = mul(float4(si.pos.xyz, 1), g_ubParticlesVS._matWVP);
	so.tc0 = si.tc0;
	so.additive = si.pos.w;
	so.color0 = si.color * float4(g_ubParticlesVS._brightness.xxx, 1);
	so.psize = si.psize * (g_ubParticlesVS._viewportSize.yx * g_ubParticlesVS._viewportSize.z) * g_ubParticlesVS._matP._m11;

	return so;
}
#endif

#ifdef _GS
[maxvertexcount(4)]
void mainGS(point VSO si[1], inout TriangleStream<VSO> stream)
{
	VSO so;

	so = si[0];
	const float2 center = so.pos.xy;
	const float2 tcBias = so.tc0;
	for (int i = 0; i < 4; ++i)
	{
		so.pos.xy = center + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.xy;
		so.tc0.xy = _POINT_SPRITE_TEX_COORDS[i] * g_ubParticlesFS._tilesetSize.zw + tcBias;
		stream.Append(so);
	}
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float4 colorSam = g_tex.Sample(g_sam, si.tc0);

	so.color = colorSam * si.color0;
	so.color.a *= 1.0 - si.additive;

	return so;
}
#endif

//@main:# (VGF)
//@main:#Billboards
