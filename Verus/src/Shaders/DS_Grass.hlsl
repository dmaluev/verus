// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
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
	float2 pointSpriteSize = 1.f;
	float groundHeight;
	float3 normal;
	float2 tc0;
	bool bushMaskOK = true;
	{
		intactPos = si.pos.xyz * (1.f / 1000.f);
		pos = intactPos + float3(si.patchPos.x, 0, si.patchPos.z);
		center = si.tc.zw * (1.f / 1000.f) + si.patchPos.xz;
#ifdef DEF_BILLBOARDS
		pointSpriteSize = intactPos.y;
		pos = float3(center.x, 0.45f * pointSpriteSize.y, center.y);
#endif

		const float distToEye = distance(pos + float3(0, si.patchPos.y * 0.01f, 0), g_ubGrassVS._posEye.xyz);
		const float geomipsLod = log2(clamp(distToEye * (2.f / 100.f), 1.f, 18.f));
		const float texelCenter = 0.5f * mapSideInv;
		const float mipTexelCenter = texelCenter * exp2(geomipsLod);
		const float2 tcMap = pos.xz * mapSideInv + 0.5f;
		const float2 tcUniform = center * mapSideInv + 0.5f;
		groundHeight = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + mipTexelCenter, geomipsLod).r);
		pos.y += groundHeight;

		const float4 rawNormal = g_texNormalVS.SampleLevel(g_samNormalVS, tcUniform + texelCenter, 0.f);
		normal = float3(rawNormal.x, 0, rawNormal.y) * 2.f - 1.f;
		normal.y = ComputeNormalZ(normal.xz);

		const float layer = g_texMLayerVS.SampleLevel(g_samMLayerVS, tcUniform, 0.f).r * 4.f;
		const float uShift = frac(layer) - (16.f / 256.f);
		const float vShift = floor(layer) * 0.25f;
		float vShiftAlt = 0.f;
		if (!(bushID & 0xF))
			vShiftAlt = 0.5f; // Every 16th bush uses alternative texture.
		tc0 = si.tc.xy * (1.f / 100.f) + float2(uShift, vShift + vShiftAlt);

		// Cull blank bushes:
		const int mainLayer = round(layer * 4.f);
		const int ibushMask = asint(bushMask);
		if (!((ibushMask >> mainLayer) & 0x1))
		{
			bushMaskOK = false;
		}
	}
	// </FromTextures>

	// <Special>
	float phaseShift = 0.f;
	float2 windWarp = 0.f;
	float top = 0.f;
	{
#ifdef DEF_BILLBOARDS
		phaseShift = frac(si.pos.w * (1.f / 100.f));
		windWarp = 0.f;
		top = 0.28f;
#else
		if (intactPos.y >= 0.1f)
		{
			phaseShift = frac(si.pos.w * (1.f / 100.f));
			windWarp = warp.xz * (1.f + turbulence * sin((phase + phaseShift) * (_PI * 2.f)));
			top = 1.f;
		}
#endif
	}
	// </Special>

	// <Warp>
	float3 posWarped = pos;
	{
		const float distToEye = -mul(float4(posWarped, 1), g_ubGrassVS._matWV).z;
		const float distant = saturate((distToEye - 50.f) * (1.f / 50.f)); // [50 to 100] -> [0 to 1].
		const float distExt = saturate((distToEye - 25.f) * (1.f / 25.f)); // [25 to 50] -> [0 to 1].

		const float cliff = dot(normal.xz, normal.xz);
		float hide = cliff + step(groundHeight, 1.f) + distant;
		if (!bushMaskOK)
			hide = 1.f;
		posWarped.xz -= normal.xz;
#ifdef DEF_BILLBOARDS
		const float distExtInv = 1.f - distExt;
		posWarped.xz += windWarp * 0.5f;
		hide += distExtInv * distExtInv;
#else
		posWarped.xz += windWarp;
		posWarped.y -= dot(windWarp, windWarp);
		hide += distExt * distExt;
#endif
		hide = saturate(hide);

#ifdef DEF_BILLBOARDS
		pointSpriteSize = lerp(pointSpriteSize, float2(0.f, pointSpriteSize.y), hide);
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
	so.psize = 1.f;

#ifdef DEF_BILLBOARDS
	so.tcOffset_phaseShift.xy = tc0;
	so.psize = pointSpriteSize * (g_ubGrassVS._viewportSize.yx * g_ubGrassVS._viewportSize.z) * g_ubGrassVS._matP._m11;
#else
	so.normal_top.xyz += float3(0, 0, top * top * 0.25f);
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

	float2 tc = si.tc0;
#ifdef DEF_BILLBOARDS
	tc = si.tc0 * 0.23f + si.tcOffset_phaseShift.xy;
#endif

	const float4 rawAlbedo = g_texAlbedo.Sample(g_samAlbedo, tc);
	const float3 normal = normalize(si.normal_top.xyz);
	const float gray = Grayscale(rawAlbedo.rgb);
	const float mask = saturate((gray - 0.25f) * 2.f + 0.2f);

	const float top = si.normal_top.w;
	const float specMask = saturate(top * top * (mask + si.tcOffset_phaseShift.z * 0.1f));
	const float gloss = lerp(4.f, 12.f, specMask);

	{
		DS_Reset(so);

		DS_SetAlbedo(so, rawAlbedo.rgb);
		DS_SetSpecMask(so, specMask);

		DS_SetNormal(so, normal);
		DS_SetEmission(so, 0.f, 0.f);
		DS_SetMotionBlurMask(so, 1.f);

		DS_SetLamScaleBias(so, float2(1.2f, -0.2f), 0.f);
		DS_SetMetallicity(so, 0.05f, 0.f);
		DS_SetGloss(so, gloss);
	}

	clip(rawAlbedo.a - 0.5f);

	return so;
}
#endif

//@main:#
//@main:#Billboards BILLBOARDS (VGF)
