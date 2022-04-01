// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_BakeSprites.inc.hlsl"

ConstantBuffer<UB_BakeSpritesVS> g_ubBakeSpritesVS : register(b0, space0);
ConstantBuffer<UB_BakeSpritesFS> g_ubBakeSpritesFS : register(b0, space1);

Texture2D    g_texGBuffer0 : register(t1, space1);
SamplerState g_samGBuffer0 : register(s1, space1);
Texture2D    g_texGBuffer1 : register(t2, space1);
SamplerState g_samGBuffer1 : register(s2, space1);
Texture2D    g_texGBuffer2 : register(t3, space1);
SamplerState g_samGBuffer2 : register(s3, space1);
Texture2D    g_texGBuffer3 : register(t4, space1);
SamplerState g_samGBuffer3 : register(s4, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
};

struct VSO
{
	float4 pos : SV_Position;
	float2 tc0 : TEXCOORD0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	// Standard quad:
	so.pos = float4(mul(si.pos, g_ubBakeSpritesVS._matW), 1);
	so.tc0 = mul(si.pos, g_ubBakeSpritesVS._matV).xy;

	return so;
}
#endif

#ifdef _FS
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float4 gBuffer0Sam = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0);
	const float4 gBuffer1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float4 gBuffer2Sam = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0);
	const float4 gBuffer3Sam = g_texGBuffer3.SampleLevel(g_samGBuffer3, si.tc0, 0.0);

	so.target0 = gBuffer0Sam;
	so.target1 = gBuffer1Sam;
	so.target2 = gBuffer2Sam;
	so.target3 = gBuffer3Sam;

	if (gBuffer1Sam.a < 0.5)
	{
		// Edge padding:
		float minDist = 1000.0;
		[unroll] for (int y = -7; y <= 7; ++y)
		{
			[unroll] for (int x = -7; x <= 7; ++x)
			{
				const float2 offset = float2(x, y);
				const float dist = dot(offset, offset);

				const float4 kernel1Sam = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(x, y));
				const float kernelAlpha = kernel1Sam.a;

				if (kernelAlpha >= 0.5 && dist < minDist)
				{
					minDist = dist;

					const float4 kernel0Sam = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0, int2(x, y));
					const float4 kernel2Sam = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0, int2(x, y));
					const float4 kernel3Sam = g_texGBuffer3.SampleLevel(g_samGBuffer3, si.tc0, 0.0, int2(x, y));

					so.target0 = kernel0Sam;
					so.target1.rgb = kernel1Sam.rgb;
					so.target2 = kernel2Sam;
					so.target3 = kernel3Sam;
				}
			}
		}
	}

	return so;
}
#endif

//@main:#
