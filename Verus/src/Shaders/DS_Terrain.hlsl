#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_Terrain.inc.hlsl"

#define HEIGHT_SCALE 0.01

ConstantBuffer<UB_DrawDepth>     g_ubDrawDepth     : register(b0, space0);
ConstantBuffer<UB_PerMaterialFS> g_ubPerMaterialFS : register(b0, space1);

Texture2D      g_texHeight    : register(t1, space0);
SamplerState   g_samHeight    : register(s1, space0);
Texture2D      g_texNormal    : register(t1, space1);
SamplerState   g_samNormal    : register(s1, space1);
Texture2D      g_texBlend     : register(t2, space1);
SamplerState   g_samBlend     : register(s2, space1);
Texture2DArray g_texLayers    : register(t3, space1);
SamplerState   g_samLayers    : register(s3, space1);
Texture2DArray g_texLayersNM  : register(t4, space1);
SamplerState   g_samLayersNM  : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION int4 pos : POSITION;

	VK_LOCATION(16)      int4 posPatch : TEXCOORD8;
	VK_LOCATION(17)      int4 layers : TEXCOORD9;
};

struct VSO
{
	float4 pos           : SV_Position;
#ifndef DEF_DEPTH
	float4 tcLayer_tcMap : TEXCOORD0;
#endif
	float2 depth         : TEXCOORD1;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 posEye = g_ubDrawDepth._posEye_mapSideInv.xyz;
	const float mapSideInv = g_ubDrawDepth._posEye_mapSideInv.w;

	const float2 edgeCorrection = si.pos.yw;
	si.pos.yw = 0.0;
	float3 pos = si.pos.xyz + si.posPatch.xyz * float3(1, HEIGHT_SCALE, 1);

	const float bestPrecision = 50.0;
	const float2 tcMap = pos.xz * mapSideInv + 0.5; // Range [0, 1).
	const float distToEye = distance(pos, posEye);
	const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
	const float geomipsLodFrac = frac(geomipsLod);
	const float geomipsLodBase = floor(geomipsLod);
	const float geomipsLodNext = geomipsLodBase + 1.0;
	const float2 halfTexelAB = (0.5 * mapSideInv) * exp2(float2(geomipsLodBase, geomipsLodNext));
	const float yA = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.xx, geomipsLodBase).r + bestPrecision;
	const float yB = g_texHeight.SampleLevel(g_samHeight, tcMap + halfTexelAB.yy, geomipsLodNext).r + bestPrecision;
	pos.y = lerp(yA, yB, geomipsLodFrac);

	so.pos = mul(float4(pos, 1), g_ubDrawDepth._matVP);
#ifndef DEF_DEPTH
	so.tcLayer_tcMap.xy = pos.xz * (1.0 / 8.0);
	so.tcLayer_tcMap.zw = (pos.xz + 0.5) * mapSideInv + 0.5; // Texel's center.
#endif
	so.depth = so.pos.zw;

	return so;
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float3 rand = Rand(si.pos.xy);

	const float2 tcLayer = si.tcLayer_tcMap.xy;
	const float2 tcMap = si.tcLayer_tcMap.zw;

	const float4 rawAlbedo = g_texLayers.Sample(g_samLayers, float3(tcLayer, 1.0));

	// <Basis>
	const float4 rawBasis = g_texNormal.Sample(g_samNormal, tcMap);
	const float4 basis = rawBasis * 2.0 - 1.0;
	float3 basisNrm = float3(basis.x, 0, basis.y);
	float3 basisTan = float3(0, basis.z, basis.w);
	basisNrm.y = CalcNormalZ(basisNrm.xz);
	basisTan.x = CalcNormalZ(basisTan.yz);
	const float3 basisBin = cross(basisTan, basisNrm);
	_TBN_SPACE(
		mul(basisTan, (float3x3)g_ubDrawDepth._matWV),
		mul(basisBin, (float3x3)g_ubDrawDepth._matWV),
		mul(basisNrm, (float3x3)g_ubDrawDepth._matWV));
	// </Basis>

	// <Normal>
	const float3 normalWV = mul(float3(0, 0, 1), matFromTBN);
	// </Normal>

	DS_Test(so, 0.0, 0.5, 16.0);
	DS_SetAlbedo(so, rawAlbedo.rgb);
	DS_SetDepth(so, si.depth.x / si.depth.y);
	DS_SetNormal(so, normalWV + NormalDither(rand));

	return so;
}
#endif

//@main:#
//@main:#Depth DEPTH (V)
