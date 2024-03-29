// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "GUI.inc.hlsl"

CBUFFER(0, UB_Gui, g_ubGui)
CBUFFER(1, UB_GuiFS, g_ubGuiFS)

Texture2D    g_tex : REG(t1, space1, t0);
SamplerState g_sam : REG(s1, space1, s0);

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
	so.pos = mul(si.pos, g_ubGui._matWVP);
	so.tc.xy = mul(si.pos, g_ubGui._matTex).xy * g_ubGui._tcScaleBias.xy + g_ubGui._tcScaleBias.zw;
	so.tcMask.xy = mul(si.pos, g_ubGui._matTex).xy * g_ubGui._tcMaskScaleBias.xy + g_ubGui._tcMaskScaleBias.zw;

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
