// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "GUI.inc.hlsl"

ConstantBuffer<UB_Gui>   g_ubGui   : register(b0, space0);
ConstantBuffer<UB_GuiFS> g_ubGuiFS : register(b0, space1);

Texture2D    g_tex : register(t1, space1);
SamplerState g_sam : register(s1, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos   : POSITION;
};

struct VSO
{
	float4 pos    : SV_Position;
	float2 tc     : TEXCOORD0;
	float2 tcMask : TEXCOORD1;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Quad:
	so.pos = float4(mul(si.pos, g_ubGui._matW), 1);
	so.tc.xy = mul(si.pos, g_ubGui._matV).xy * g_ubGui._tcScaleBias.xy + g_ubGui._tcScaleBias.zw;
	so.tcMask.xy = mul(si.pos, g_ubGui._matV).xy * g_ubGui._tcMaskScaleBias.xy + g_ubGui._tcMaskScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

#ifdef DEF_SOLID_COLOR
	so.color = g_ubGuiFS._color;
#else
	const float4 color = g_tex.Sample(g_sam, si.tc);
	so.color = color * g_ubGuiFS._color;
#ifdef DEF_MASK
	so.color.a = g_ubGuiFS._color.a;
	so.color *= g_tex.Sample(g_sam, si.tcMask).a;
#endif
#endif

	return so;
}
#endif

//@main:#
//@main:#Mask MASK
//@main:#SolidColor SOLID_COLOR
