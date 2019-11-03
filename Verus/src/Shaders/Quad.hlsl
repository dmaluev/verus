#include "Lib.hlsl"
#include "Quad.inc.hlsl"

ConstantBuffer<UB_QuadVS> g_ubQuadVS : register(b0, space0);
ConstantBuffer<UB_QuadFS> g_ubQuadFS : register(b0, space1);

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
	so.pos = float4(mul(si.pos, g_ubQuadVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubQuadVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = g_tex.Sample(g_sam, si.tc0);

	return so;
}
#endif

//@main:#
//@main:#Clear CLEAR
//@main:#Depth DEPTH
