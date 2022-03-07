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

VERUS_UBUFFER UB_ShaderVS
{
	matrix _matWVP;
};

VERUS_UBUFFER UB_ShaderFS
{
	float _phase;
};

ConstantBuffer<UB_ShaderVS> g_ubShaderVS : register(b0, space0);
ConstantBuffer<UB_ShaderFS> g_ubShaderFS : register(b0, space1);

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
