// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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

	const float4 rawGBuffer0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0);
	const float4 rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0);
	const float4 rawGBuffer2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0);

	so.target0 = rawGBuffer0;
	so.target1 = rawGBuffer1;
	so.target2 = rawGBuffer2;

	if (rawGBuffer1.a < 0.5)
	{
		// Defringe:
		float minDist = 1000.0;
		[unroll] for (int i = -7; i <= 7; ++i)
		{
			[unroll] for (int j = -7; j <= 7; ++j)
			{
				const float2 offset = float2(j, i);
				const float dist = dot(offset, offset);

				const float4 rawKernel1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, si.tc0, 0.0, int2(j, i));
				const float kernelAlpha = rawKernel1.a;

				if (kernelAlpha >= 0.5 && dist < minDist)
				{
					minDist = dist;

					const float4 rawKernel0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, si.tc0, 0.0, int2(j, i));
					const float4 rawKernel2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, si.tc0, 0.0, int2(j, i));

					so.target0 = rawKernel0;
					so.target1.rgb = rawKernel1.rgb;
					so.target2 = rawKernel2;
				}
			}
		}
	}

	return so;
}
#endif

//@main:#
