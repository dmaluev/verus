// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibSurface.hlsl"
#include "WaterGen.inc.hlsl"

ConstantBuffer<UB_Gen>            g_ubGen            : register(b0, space0);
ConstantBuffer<UB_GenHeightmapFS> g_ubGenHeightmapFS : register(b0, space1);
ConstantBuffer<UB_GenNormalsFS>   g_ubGenNormalsFS   : register(b0, space2);

Texture2D    g_texSourceHeightmap : register(t1, space1);
SamplerState g_samSourceHeightmap : register(s1, space1);
Texture2D    g_texGenHeightmap    : register(t1, space2);
SamplerState g_samGenHeightmap    : register(s1, space2);

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
VSO mainGenHeightmapVS(VSI si)
{
	VSO so;

	so.pos = float4(mul(si.pos, g_ubGen._matW), 1);
	so.tc0 = mul(si.pos, g_ubGen._matV).xy;

	return so;
}
#endif

#ifdef _VS
VSO mainGenNormalsVS(VSI si)
{
	VSO so;

	so.pos = float4(mul(si.pos, g_ubGen._matW), 1);
	so.tc0 = mul(si.pos, g_ubGen._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainGenHeightmapFS(VSO si)
{
	FSO so;

	const float4 amplitudes = g_ubGenHeightmapFS._amplitudes; // Local copy.
	const float2 dirs[4] =
	{
		float2(+1, +0),
		float2(+0, +1),
		float2(-1, +0),
		float2(+0, -1)
	};
	const float4 offsets = float4(0, 0.3f, 0.5f, 0.7f);
	float accHeight = 0.f;
	[unroll] for (uint i = 0; i < 4; ++i)
	{
		float height = g_texSourceHeightmap.SampleLevel(g_samSourceHeightmap, (si.tc0 + offsets[i]) * (i / 2 + 1) + g_ubGenHeightmapFS._phase.x * dirs[i], 0.f).r - 0.5f;
		height *= amplitudes[i]; // Must be local float4 to work in D3D12!
		accHeight += height;
	}
	const float splash = max(0.f, accHeight);
	so.color = accHeight * 2.f + splash * splash * splash * 16.f;

	return so;
}
#endif

#ifdef _FS
FSO mainGenNormalsFS(VSO si)
{
	FSO so;

	const float damp = (1.f / 512.f / 2.f) * g_ubGenNormalsFS._waterScale.x * g_ubGenNormalsFS._textureSize.x;
	so.color.rgb = ComputeNormals(g_texGenHeightmap, g_samGenHeightmap, si.tc0, damp, 2);
	so.color.a = 1.f;

	return so;
}
#endif

//@mainGenHeightmap:#GenHeightmap
//@mainGenNormals:#GenNormals
