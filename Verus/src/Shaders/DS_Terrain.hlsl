#include "Lib.hlsl"
#include "DS_Terrain.inc.hlsl"

#define HEIGHT_SCALE 0.01

ConstantBuffer<UB_DrawDepth> g_drawDepth : register(b0, space0);

Texture2D      g_texHeight    : register(t1, space0);
SamplerState   g_samHeight    : register(s1, space0);
Texture2D      g_texNormal    : register(t2, space0);
SamplerState   g_samNormal    : register(s2, space0);
Texture2D      g_texBlend     : register(t3, space0);
SamplerState   g_samBlend     : register(s3, space0);
Texture2DArray g_texLayers    : register(t4, space0);
SamplerState   g_samLayers    : register(s4, space0);
Texture2DArray g_texLayersNM  : register(t5, space0);
SamplerState   g_samLayersNM  : register(s5, space0);

struct VSI
{
	VK_LOCATION_POSITION int4 pos : POSITION;

	VK_LOCATION(16)      int4 posPatch : TEXCOORD8;
	VK_LOCATION(17)      int4 layers : TEXCOORD9;
};

struct VSO
{
	float4 pos : SV_Position;
	float4 tcLayer_tcMap : TEXCOORD0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 posEye = g_drawDepth._posEye_mapSideInv.xyz;
	const float mapSideInv = g_drawDepth._posEye_mapSideInv.w;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz*float3(1, HEIGHT_SCALE, 1);

	const float bestPrecision = 50.0;
	const float2 tcMap = pos.xz*mapSideInv + 0.5; // Range [0, 1).
	const float distToEye = distance(pos, posEye);
	const float geomipsLod = log2(clamp(distToEye*(2.0 / 100.0), 1.0, 18.0));
	const float geomipsLodFrac = frac(geomipsLod);
	const float geomipsLodBase = floor(geomipsLod);
	const float geomipsLodNext = geomipsLodBase + 1.0;
	const float2 halfTexelAB = (0.5*mapSideInv)*exp2(float2(geomipsLodBase, geomipsLodNext));
	const float yA = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.xx, geomipsLodBase).r + bestPrecision;
	const float yB = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.yy, geomipsLodNext).r + bestPrecision;
	pos.y = lerp(yA, yB, geomipsLodFrac);

	so.pos = mul(float4(pos, 1), g_drawDepth._matVP);

	so.tcLayer_tcMap.xy = pos.xz * (1.0 / 8.0);
	so.tcLayer_tcMap.zw = (pos.xz + 0.5)*mapSideInv + 0.5; // Texel's center.

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float4 normal = g_texNormal.Sample(g_samNormal, tcMap);

	const float4 albedo = g_texLayers.Sample(g_samLayers, float3(tcLayer, 1.0));

	so.color = float4(albedo.xyz * normal.r, 1);

	return so;
}
#endif

//@main:T
