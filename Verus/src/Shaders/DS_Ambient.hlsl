// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibLighting.hlsl"
#include "DS_Ambient.inc.hlsl"

CBUFFER(0, UB_AmbientVS, g_ubAmbientVS)
CBUFFER(1, UB_AmbientFS, g_ubAmbientFS)

VK_SUBPASS_INPUT(0, g_texGBuffer0, g_samGBuffer0, 1, space1);
VK_SUBPASS_INPUT(1, g_texGBuffer1, g_samGBuffer1, 2, space1);
VK_SUBPASS_INPUT(2, g_texGBuffer2, g_samGBuffer2, 3, space1);
VK_SUBPASS_INPUT(3, g_texGBuffer3, g_samGBuffer3, 4, space1);
VK_SUBPASS_INPUT(4, g_texDepth, g_samDepth, 5, space1);
Texture2D    g_texTerrainHeightmap : REG(t6, space1, t5);
SamplerState g_samTerrainHeightmap : REG(s6, space1, s5);
Texture2D    g_texTerrainBlend     : REG(t7, space1, t6);
SamplerState g_samTerrainBlend     : REG(s7, space1, s6);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos : SV_Position;
	float4 tc0 : TEXCOORD0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubAmbientVS._matW), 1);
	so.tc0.xy = mul(si.pos, g_ubAmbientVS._matV).xy;
	so.tc0.zw = so.tc0.xy * g_ubAmbientVS._tcViewScaleBias.xy + g_ubAmbientVS._tcViewScaleBias.zw;

	return so;
}
#endif

#ifdef _FS
DS_ACC_FSO mainFS(VSO si)
{
	DS_ACC_FSO so;
	DS_Reset(so);

	const float2 ndcPos = ToNdcPos(si.tc0.xy);

	// <SampleSurfaceData>
	// GBuffer0:
	const float4 gBuffer0Sam = VK_SUBPASS_LOAD(g_texGBuffer0, g_samGBuffer0, si.tc0.zw);

	// GBuffer1:
	const float4 gBuffer1Sam = VK_SUBPASS_LOAD(g_texGBuffer1, g_samGBuffer1, si.tc0.zw);
	const float3 normalWV = DS_GetNormal(gBuffer1Sam);
	const float3 normal = mul(float4(normalWV, 0), g_ubAmbientFS._matInvV);

	// GBuffer2:
	const float4 gBuffer2Sam = VK_SUBPASS_LOAD(g_texGBuffer2, g_samGBuffer2, si.tc0.zw);
	const float roughness = gBuffer2Sam.g;
	const float metallic = 0.0; // Treat everything as non-metallic.

	// GBuffer3:
	const float4 gBuffer3Sam = VK_SUBPASS_LOAD(g_texGBuffer3, g_samGBuffer3, si.tc0.zw);

	// Depth:
	const float depthSam = VK_SUBPASS_LOAD(g_texDepth, g_samDepth, si.tc0.zw).r;
	const float3 posWV = DS_GetPosition(depthSam, g_ubAmbientFS._matInvP, ndcPos);
	const float3 posW = mul(float4(posWV, 1), g_ubAmbientFS._matInvV);
	// </SampleSurfaceData>

	const float2 tcLand = (posW.xz + 0.5) * g_ubAmbientFS._invMapSide_minOcclusion.x + 0.5;
	const float2 tcBlend = posW.xz * g_ubAmbientFS._invMapSide_minOcclusion.x + 0.5;
	const float landHeight = UnpackTerrainHeight(g_texTerrainHeightmap.SampleLevel(g_samTerrainHeightmap, tcLand, 0.0).r);
	const float blendSam = g_texTerrainBlend.Sample(g_samTerrainBlend, tcBlend).a;

	const float aboveGround = posW.y - landHeight;
	const float mask = saturate(aboveGround * (1.0 / 20.0));
	const float occlusion = lerp(blendSam, 1.0, mask * mask);

	const float3 dirToEyeWV = normalize(-posWV);

	const float3 ambientColorY01 = lerp(
		g_ubAmbientFS._ambientColorY0.rgb,
		g_ubAmbientFS._ambientColorY1.rgb,
		abs(normal.y));
	const float grayAmbient = Grayscale(g_ubAmbientFS._ambientColorY0.rgb);
	const float3 ambientColor = lerp(ambientColorY01, grayAmbient * 0.8, saturate(-normal.y));

	so.target0.rgb = VerusIBL(normalWV, dirToEyeWV,
		gBuffer0Sam.rgb, ambientColor,
		roughness, metallic,
		float2(-1, 1));

	so.target0.rgb *= max(occlusion, g_ubAmbientFS._invMapSide_minOcclusion.y);

	return so;
}
#endif

//@main:#
