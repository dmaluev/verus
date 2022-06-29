// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

// Copied from Lib.hlsl:
#define VERUS_UBUFFER struct
#define mataff float4x3
#ifdef _VULKAN
#	define VK_LOCATION_POSITION [[vk::location(0)]]
#	define VK_LOCATION_COLOR0   [[vk::location(3)]]
#else
#	define VK_LOCATION_POSITION
#	define VK_LOCATION_COLOR0
#endif

#ifdef _DIRECT3D11
#	define CBUFFER(set, type, name) cbuffer type : register(b##set) { type name; }
#	define REG(slot, space, sm50slot) register(sm50slot)
#else
#	define CBUFFER(set, type, name) ConstantBuffer<type> name : register(b0, space##set);
#	define REG(slot, space, sm50slot) register(slot, space)
#endif

VERUS_UBUFFER UB_ShaderVS
{
	mataff _matW;
	matrix _matVP;
};

VERUS_UBUFFER UB_ShaderFS
{
	float _phase;
};

CBUFFER(0, UB_ShaderVS, g_ubShaderVS)
CBUFFER(1, UB_ShaderFS, g_ubShaderFS)

Texture2D    g_tex : REG(t1, space1, t0);
SamplerState g_sam : REG(s1, space1, s0);

struct VSI
{
	VK_LOCATION_POSITION float3 pos   : POSITION;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
};

struct VSO
{
	float4 pos    : SV_Position;
	float3 tcCube : TEXCOORD0;
	float4 color  : COLOR0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 posW = mul(float4(si.pos, 1), g_ubShaderVS._matW);
	so.pos = mul(float4(posW, 1), g_ubShaderVS._matVP);
	so.tcCube = si.pos.xyz * float3(1, -1, 1);
	so.color = si.color;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	// Generate texture coordinates using CubeMap method:
	float2 tc = 0.0;
	const float3 mag = abs(si.tcCube);
	if (mag.x >= mag.y && mag.x >= mag.z)
		tc = 0.5 * (si.tcCube.zy / mag.x + 1.0);
	else if (mag.y >= mag.x && mag.y >= mag.z)
		tc = 0.5 * (si.tcCube.xz / mag.y + 1.0);
	else
		tc = 0.5 * (si.tcCube.xy / mag.z + 1.0);

	so.color = g_tex.Sample(g_sam, tc);

	so.color = lerp(so.color, si.color, g_ubShaderFS._phase);

	return so;
}
#endif

//@main:#
