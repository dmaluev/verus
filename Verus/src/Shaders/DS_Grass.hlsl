// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibSurface.hlsl"
#include "DS_Grass.inc.hlsl"

CBUFFER(0, UB_GrassVS, g_ubGrassVS)
CBUFFER(1, UB_GrassFS, g_ubGrassFS)

Texture2D    g_texHeightVS : REG(t1, space0, t0);
SamplerState g_samHeightVS : REG(s1, space0, s0);
Texture2D    g_texNormalVS : REG(t2, space0, t1);
SamplerState g_samNormalVS : REG(s2, space0, s1);
Texture2D    g_texMLayerVS : REG(t3, space0, t2);
SamplerState g_samMLayerVS : REG(s3, space0, s2);

Texture2D    g_texAlbedo : REG(t1, space1, t3);
SamplerState g_samAlbedo : REG(s1, space1, s3);

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

	const float phase = g_ubGrassVS._phase_invMapSide_bushMask.x;
	const float invMapSide = g_ubGrassVS._phase_invMapSide_bushMask.y;
	const float bushMask = g_ubGrassVS._phase_invMapSide_bushMask.z;
	const float3 warp = g_ubGrassVS._warp_turb.xyz;
	const float turbulence = g_ubGrassVS._warp_turb.w;

#ifdef DEF_BILLBOARDS
	const uint bushID = si.vertexID;
#else
	const uint bushID = si.vertexID >> 3; // 8 vertices per bush.
#endif

	// <FromTextures>
	float3 inPos;
	float3 pos;
	float2 center;
	float2 pointSpriteSize = 1.0;
	float groundHeight;
	float3 normal;
	float2 tc0;
	bool bushMaskOK = true;
	{
		inPos = si.pos.xyz * (1.0 / 1000.0);
		pos = inPos + float3(si.patchPos.x, 0, si.patchPos.z); // Preserve original height.
		center = si.tc.zw * (1.0 / 1000.0) + si.patchPos.xz;
#ifdef DEF_BILLBOARDS
		pointSpriteSize = inPos.y;
		pos = float3(center.x, 0.45 * pointSpriteSize.y, center.y);
#endif

		const float distToEye = distance(pos + float3(0, UnpackTerrainHeight(si.patchPos.y), 0), g_ubGrassVS._posEye.xyz);
		const float geomipsLod = log2(clamp(distToEye * (2.0 / 100.0), 1.0, 18.0));
		const float texelCenter = 0.5 * invMapSide;
		const float mipTexelCenter = texelCenter * exp2(geomipsLod);
		const float2 tcMap = pos.xz * invMapSide + 0.5;
		const float2 tcUniform = center * invMapSide + 0.5;
		groundHeight = UnpackTerrainHeight(g_texHeightVS.SampleLevel(g_samHeightVS, tcMap + mipTexelCenter, geomipsLod).r);
		pos.y += groundHeight;

		const float4 normalSam = g_texNormalVS.SampleLevel(g_samNormalVS, tcUniform + texelCenter, 0.0);
		normal = float3(normalSam.x, 0, normalSam.y) * 2.0 - 1.0;
		normal.y = ComputeNormalZ(normal.xz);

		const float layer = g_texMLayerVS.SampleLevel(g_samMLayerVS, tcUniform, 0.0).r * 4.0;
		const float uShift = frac(layer) - ((4.0 * 4.0) / 256.0);
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
		if (inPos.y >= 0.1)
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
		const float depth = -mul(float4(posWarped, 1), g_ubGrassVS._matWV).z;

		const float nearAlpha = ComputeFade(depth, 25.0, 50.0);
		const float farAlpha = ComputeFade(depth, 50.0, 100.0);

		const float cliff = dot(normal.xz, normal.xz);
		const float underwater = step(groundHeight, 1.0);
		float hide = cliff + underwater + farAlpha;
		if (!bushMaskOK)
			hide = 1.0;
		posWarped.xz -= normal.xz; // Push into terrain.
#ifdef DEF_BILLBOARDS
		const float oneMinusNearAlpha = 1.0 - nearAlpha;
		posWarped.xz += windWarp * 0.5;
		hide += oneMinusNearAlpha * oneMinusNearAlpha;
#else
		posWarped.xz += windWarp;
		posWarped.y -= dot(windWarp, windWarp);
		hide += nearAlpha * nearAlpha;
#endif
		hide = saturate(hide);

#ifdef DEF_BILLBOARDS
		pointSpriteSize = lerp(pointSpriteSize, float2(0, pointSpriteSize.y), hide);
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
	so.psize = pointSpriteSize * (g_ubGrassVS._viewportSize.yx * g_ubGrassVS._viewportSize.z) * g_ubGrassVS._matP._m11;
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
		so.tc0 = _POINT_SPRITE_TEX_COORDS[i];
		stream.Append(so);
	}
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;
	DS_Reset(so);

	const float dither = Dither2x2(si.pos.xy);

	float2 tc = si.tc0;
#ifdef DEF_BILLBOARDS
	tc = si.tc0 * 0.23 + si.tcOffset_phaseShift.xy;
#endif

	const float top = si.normal_top.w;
	const float bottom = 1.0 - top;

	const float4 albedoSam = g_texAlbedo.Sample(g_samAlbedo, tc);
	const float3 normal = normalize(lerp(si.normal_top.xyz, float3(0, 0, 1), top * 0.35));
	const float gray = Grayscale(albedoSam.rgb);
	const float mask = saturate((gray - 0.25) * 2.0 + 0.2);

	const float occlusion = 0.6 + 0.4 * (1.0 - bottom * bottom);
	const float roughnessMask = saturate(top * (mask + si.tcOffset_phaseShift.z * 0.1));
	const float roughness = lerp(0.9, 0.3, roughnessMask);
	const float wrapDiffuse = 0.25 * top + 0.25 * (1.0 - albedoSam.a);

	{
		DS_SetAlbedo(so, albedoSam.rgb);
		DS_SetSSSHue(so, 1.0);

		DS_SetNormal(so, normal);
		DS_SetEmission(so, 0.0);
		DS_SetMotionBlur(so, 1.0);

		DS_SetOcclusion(so, occlusion);
		DS_SetRoughness(so, roughness);
		DS_SetMetallic(so, 0.0);
		DS_SetWrapDiffuse(so, wrapDiffuse);

		DS_SetTangent(so, float3(0, 0, 1));
		DS_SetAnisoSpec(so, 0.0);
		DS_SetRoughDiffuse(so, max(0.05, AlphaToResolveDitheringMask(albedoSam.a)));
	}

	clip(albedoSam.a - (dither + (1.0 / 8.0)));

	return so;
}
#endif

//@main:#
//@main:#Billboards BILLBOARDS (VGF)
