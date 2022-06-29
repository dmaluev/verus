R"(

// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

// Copied from Lib.hlsl:
#define VERUS_UBUFFER struct
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
	matrix _matWVP;
};

VERUS_UBUFFER UB_ShaderFS
{
	float _phase;
};

CBUFFER(0, UB_ShaderVS, g_ubShaderVS)
CBUFFER(1, UB_ShaderFS, g_ubShaderFS)

struct VSI
{
	VK_LOCATION_POSITION float4 pos   : POSITION;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
};

struct VSO
{
	float4 pos   : SV_Position;
	float4 color : COLOR0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	so.pos = mul(si.pos, g_ubShaderVS._matWVP);
	so.color = si.color;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = saturate(si.color + g_ubShaderFS._phase);

	return so;
}
#endif

//@main:#

)"
