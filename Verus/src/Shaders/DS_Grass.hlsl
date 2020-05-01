// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "LibVertex.hlsl"
#include "DS_Grass.inc.hlsl"

ConstantBuffer<UB_GrassVS> g_ubGrassVS : register(b0, space0);
ConstantBuffer<UB_GrassFS> g_ubGrassFS : register(b0, space1);

Texture2D    g_texHeightVS : register(t1, space0);
SamplerState g_samHeightVS : register(s1, space0);
Texture2D    g_texNormalVS : register(t2, space0);
SamplerState g_samNormalVS : register(s2, space0);
Texture2D    g_texMLayerVS : register(t3, space0);
SamplerState g_samMLayerVS : register(s3, space0);

Texture2D    g_texAlbedo : register(t1, space1);
SamplerState g_samAlbedo : register(s1, space1);

struct VSI
{
	VK_LOCATION_POSITION int4 pos      : POSITION;
	VK_LOCATION(8)       int4 tc       : TEXCOORD0;
	VK_LOCATION(16)      int4 patchPos : INSTDATA0;
	uint vertexID                      : SV_VertexID;
};

struct VSO
{
	float4 pos                 : SV_Position;
	float2 tc0                 : TEXCOORD0;
	float3 tcOffset_phaseShift : TEXCOORD1;
	float4 normal_top          : TEXCOORD2;
	float2 psize               : PSIZE;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float phase = g_ubGrassVS._phase_mapSideInv_bushMask.x;
	const float mapSideInv = g_ubGrassVS._phase_mapSideInv_bushMask.y;
	const float bushMask = g_ubGrassVS._phase_mapSideInv_bushMask.z;
	const float3 warp = g_ubGrassVS._warp_turb.xyz;
	const float turbulence = g_ubGrassVS._warp_turb.w;

#ifdef DEF_BILLBOARDS
	const uint bushID = si.vertexID;
#else
	const uint bushID = si.vertexID >> 3; // 8 vertices per bush.
#endif

	// <FromTextures>
	float3 intactPos;
	float3 pos;
	float2 center;
	float2 pointSpriteScale = 1.0;
	float groundHeight;
	float3 normal;
	float2 tc0;
	bool bushMaskOK = true;
	{
		intactPos = si.pos.xyz * (1.0 / 1000.0);
		pos = intactPos + float3(si.patchPos.x, 0, si.patchPos.z);
		center = si.tc.zw * (1.0 / 1000.0) + si.patchPos.xz;
#ifdef DEF_BILLBOARDS
		pointSpriteScale = intactPos.y;
		pos = float3(center.x, 0.45 * pointSpriteScale.y, center.y);
#endif

		const float bestPrecision = 50.0;
		const float distToEye = distance(pos + float3(0, si.patchPos.y * 0.01, 0), g_ubGrassVS._posEye.xyz);
		const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
		const float texelCenter = 0.5 * mapSideInv;
		const float mipTexelCenter = texelCenter * exp2(geomipsLod);
		const float2 tcMap = pos.xz * mapSideInv + 0.5;
		const float2 tcUniform = center * mapSideInv + 0.5;
		groundHeight = g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + mipTexelCenter, geomipsLod).r + bestPrecision;
		pos.y += groundHeight;

		const float4 rawNormal = g_texNormalVS.SampleLevel(g_samNormalVS, tcUniform + texelCenter, 0.0);
		normal = float3(rawNormal.x, 0, rawNormal.y) * 2.0 - 1.0;
		normal.y = ComputeNormalZ(normal.xz);

		const float layer = g_texMLayerVS.SampleLevel(g_samMLayerVS, tcUniform, 0.0).r * 4.0;
		const float uShift = frac(layer) - (16.0 / 256.0);
		const float vShift = floor(layer) * 0.25;
		float vShiftAlt = 0.0;
		if (!(bushID & 0xF))
			vShiftAlt = 0.5; // Every 16th bush uses alternative texture.
		tc0 = si.tc.xy * (1.0 / 100.0) + float2(uShift, vShift + vShiftAlt);

		// Cull blank bushes:
		const int mainLayer = round(layer * 4.0);
		const int ibushMask = asint(bushMask);
		if (!((ibushMask >> mainLayer) & 0x1))
		{
			bushMaskOK = false;
		}
	}
	// </FromTextures>

	// <Special>
	float phaseShift = 0.0;
	float2 windWarp = 0.0;
	float top = 0.0;
	{
#ifdef DEF_BILLBOARDS
		phaseShift = frac(si.pos.w * (1.0 / 100.0));
		windWarp = 0.0;
		top = 0.28;
#else
		if (intactPos.y >= 0.1)
		{
			phaseShift = frac(si.pos.w * (1.0 / 100.0));
			windWarp = warp.xz * (1.0 + turbulence * sin((phase + phaseShift) * (_PI * 2.0)));
			top = 1.0;
		}
#endif
	}
	// </Special>

	// <Warp>
	float3 posWarped = pos;
	{
		const float distToEye = -mul(float4(posWarped, 1), g_ubGrassVS._matWV).z;
		const float distant = saturate((distToEye - 50.0) * (1.0 / 50.0)); // [50.0 to 100.0] -> [0.0 to 1.0].
		const float distExt = saturate((distToEye - 25.0) * (1.0 / 25.0)); // [25.0 to 50.0] -> [0.0 to 1.0].

		const float cliff = dot(normal.xz, normal.xz);
		float hide = cliff + step(groundHeight, 1.0) + distant;
		if (!bushMaskOK)
			hide = 1.0;
		posWarped.xz -= normal.xz;
#ifdef DEF_BILLBOARDS
		const float distExtInv = 1.0 - distExt;
		posWarped.xz += windWarp * 0.5;
		hide += distExtInv * distExtInv;
#else
		posWarped.xz += windWarp;
		posWarped.y -= dot(windWarp, windWarp);
		hide += distExt * distExt;
#endif
		hide = saturate(hide);

#ifdef DEF_BILLBOARDS
		pointSpriteScale = lerp(pointSpriteScale, float2(0.0, pointSpriteScale.y), hide);
#else
		posWarped = lerp(posWarped, float3(center.x, posWarped.y, center.y), hide); // Optimize by morphing to center point.
#endif
	}
	// </Warp>

	const float3 posW = mul(float4(posWarped, 1), g_ubGrassVS._matW).xyz;
	so.pos = mul(float4(posW, 1), g_ubGrassVS._matVP);
	so.tc0 = tc0;
	so.tcOffset_phaseShift = float3(0, 0, phaseShift);
	so.normal_top.xyz = mul(normal, (float3x3)g_ubGrassVS._matWV);
	so.normal_top.w = top;
	so.psize = 1.0;

#ifdef DEF_BILLBOARDS
	so.tcOffset_phaseShift.xy = tc0;
	const float2 pointSize = g_ubGrassVS._viewportSize.yx * g_ubGrassVS._viewportSize.z * pointSpriteScale;
	so.psize = pointSize * g_ubGrassVS._matP._m11;
#else
	so.normal_top.xyz += float3(0, 0, top * top * 0.25);
#endif

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
	for (int i = 0; i < 4; ++i)
	{
		so.pos.xy = center + _POINT_SPRITE_POS_OFFSETS[i] * so.psize;
		so.tc0.xy = _POINT_SPRITE_TEX_COORDS[i];
		stream.Append(so);
	}
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float3 rand = Rand(si.pos.xy);

	float2 tc = si.tc0;
#ifdef DEF_BILLBOARDS
	tc = si.tc0 * 0.23 + si.tcOffset_phaseShift.xy;
#endif

	const float4 rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, tc);
	const float3 normal = normalize(si.normal_top.xyz);
	const float gray = Grayscale(rawAlbedo.rgb);
	const float mask = saturate((gray - 0.25) * 4.0 + 0.25);

	const float top = si.normal_top.w;
	const float spec = saturate(top * top * (mask + si.tcOffset_phaseShift.z * 0.1));
	const float gloss = lerp(4.0, 12.0, spec);

	{
		DS_Reset(so);

		DS_SetAlbedo(so, rawAlbedo.rgb);
		DS_SetSpec(so, spec);

		DS_SetNormal(so, normal + NormalDither(rand));
		DS_SetEmission(so, 0.0, 0.0);

		DS_SetLamScaleBias(so, float2(1.2, -0.2), 0.0);
		DS_SetMetallicity(so, 0.05, 0.0);
		DS_SetGloss(so, gloss);
	}

	clip(rawAlbedo.a - 0.5);

	return so;
}
#endif

//@main:#
//@main:#Billboards BILLBOARDS (VGF)
