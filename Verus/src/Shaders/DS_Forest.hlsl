// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#include "Lib.hlsl"
#include "LibDeferredShading.hlsl"
#include "DS_Forest.inc.hlsl"

ConstantBuffer<UB_ForestVS> g_ubForestVS : register(b0, space0);
ConstantBuffer<UB_ForestFS> g_ubForestFS : register(b0, space1);

Texture2D    g_texGBuffer0 : register(t1, space1);
SamplerState g_samGBuffer0 : register(s1, space1);
Texture2D    g_texGBuffer1 : register(t2, space1);
SamplerState g_samGBuffer1 : register(s2, space1);
Texture2D    g_texGBuffer2 : register(t3, space1);
SamplerState g_samGBuffer2 : register(s3, space1);

struct VSI
{
	VK_LOCATION_POSITION float4 pos : POSITION;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
};

struct VSO
{
	float4 pos    : SV_Position;
	float2 tc0    : TEXCOORD0;
	float2 angles : TEXCOORD1;
	float4 color  : COLOR0;
	float3 psize  : PSIZE;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float pointSpriteSize = si.tc0.x / 500.0;
	const float angle = si.tc0.y / 32767.0;

	const float3 toEye = g_ubForestVS._eyePos.xyz - si.pos.xyz;
	const float distToScreen = length(g_ubForestVS._eyePosScreen.xyz - si.pos.xyz);

	const float nearAlpha = saturate((distToScreen - 60.0) * (1.0 / 30.0));
	const float farAlpha = 1.0 - saturate((distToScreen - 900.0) * (1.0 / 100.0));

	so.pos = mul(si.pos, g_ubForestVS._matWVP);
	so.tc0 = 0.0;
	so.color.rgb = RandomColor(si.pos.xz, 0.3, 0.2);
	so.color.a = nearAlpha * farAlpha;
	so.psize.xy = pointSpriteSize * (g_ubForestVS._viewportSize.yx * g_ubForestVS._viewportSize.z) * g_ubForestVS._matP._m11;
	so.psize.xy *= ceil(so.color.a); // Hide if too close.
	so.psize.z = saturate(pointSpriteSize * 0.25 - 0.5);

	float2 param0 = toEye.xy;
	float2 param1 = toEye.zz;
	param0.y = max(0.0, param0.y); // Only upper hemisphere.
	param1.y = length(toEye.xz); // Distance in XZ-plane.
	so.angles.xy = (atan2(param0, param1) + _PI) * (0.5 / _PI); // atan2(x, z) and atan2(max(0.0, y), length(toEye.xz)). From 0 to 1.
	so.angles.y = (so.angles.y - 0.5) * 4.0; // Choose this quadrant.
	so.angles.xy = saturate(so.angles.xy);
	so.angles.x = frac(so.angles.x - angle + 0.5); // Turn.

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
		so.pos.xy = center + _POINT_SPRITE_POS_OFFSETS[i] * so.psize.xy;
		so.tc0.xy = _POINT_SPRITE_TEX_COORDS[i];
		stream.Append(so);
	}
}
#endif

float2 ComputeTexCoords(float2 tc, float2 angles)
{
	const float marginBias = 16.0 / 512.0;
	const float marginScale = 1.0 - marginBias * 2.0;
	const float2 tcMargin = tc * marginScale + marginBias;

	const float2 frameCount = float2(16, 16);
	const float2 frameScale = 1.0 / frameCount;
	const float2 frameBias = floor(min(angles * frameCount + 0.5, float2(256, frameCount.y - 0.5)));
	return (tcMargin + frameBias) * frameScale;
}

float ComputeMask(float2 tc, float alpha)
{
	const float2 tcCenter = tc - 0.5;
	const float rad = saturate(dot(tcCenter, tcCenter) * 4.0);
	return saturate(rad + (alpha * 2.0 - 1.0));
}

#ifdef _FS
#ifdef DEF_DEPTH
void mainFS(VSO si)
{
	const float2 tc = ComputeTexCoords(si.tc0, si.angles);
	const float mask = ComputeMask(si.tc0, si.color.a);

	const float alpha = g_texGBuffer1.Sample(g_samGBuffer1, tc).a;

	clip(alpha * mask - 0.53);
}
#else
DS_FSO mainFS(VSO si)
{
	DS_FSO so;

	const float2 tc = ComputeTexCoords(si.tc0, si.angles);
	const float mask = ComputeMask(si.tc0, si.color.a);

	const float4 rawGBuffer0 = g_texGBuffer0.Sample(g_samGBuffer0, tc);
	const float4 rawGBuffer1 = g_texGBuffer1.Sample(g_samGBuffer1, tc);
	const float4 rawGBuffer2 = g_texGBuffer2.Sample(g_samGBuffer2, tc);

	so.target0 = rawGBuffer0;
	so.target1 = rawGBuffer1;
	so.target2 = rawGBuffer2;

	so.target0.rgb *= si.color.rgb;
	const float lamBiasRatio = 1.0 - ComputeMask(si.tc0, 0.5);
	so.target2.g = lerp(so.target2.g, 0.25, lamBiasRatio * lamBiasRatio * si.psize.z);

	clip(rawGBuffer1.a * mask - 0.53);

	return so;
}
#endif
#endif

//@main:# (VGF)
//@main:#Depth DEPTH (VGF)
