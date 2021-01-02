// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#define VERUS_UBUFFER struct
#define mataff float4x3

#define _Q_LOW    0
#define _Q_MEDIUM 1
#define _Q_HIGH   2
#define _Q_ULTRA  3

#ifdef _VULKAN
#	define VK_LOCATION(x)           [[vk::location(x)]]
#	define VK_LOCATION_POSITION     [[vk::location(0)]]
#	define VK_LOCATION_BLENDWEIGHTS [[vk::location(1)]]
#	define VK_LOCATION_BLENDINDICES [[vk::location(6)]]
#	define VK_LOCATION_NORMAL       [[vk::location(2)]]
#	define VK_LOCATION_TANGENT      [[vk::location(14)]]
#	define VK_LOCATION_BINORMAL     [[vk::location(15)]]
#	define VK_LOCATION_COLOR0       [[vk::location(3)]]
#	define VK_LOCATION_COLOR1       [[vk::location(4)]]
#	define VK_LOCATION_PSIZE        [[vk::location(7)]]

#	define VK_PUSH_CONSTANT [[vk::push_constant]]
#	define VK_SUBPASS_INPUT(index, tex, sam, t, s, space) layout(input_attachment_index = index) SubpassInput<float4> tex : register(t, space)
#	define VK_SUBPASS_LOAD(tex, sam, tc) tex.SubpassLoad()

#	ifdef _VS
#		define VK_POINT_SIZE [[vk::builtin("PointSize")]] float psize : PSIZE;
#	else
#		define VK_POINT_SIZE
#	endif
#	define VK_SET_POINT_SIZE so.psize = 1.f
#else
#	define VK_LOCATION(x)
#	define VK_LOCATION_POSITION
#	define VK_LOCATION_BLENDWEIGHTS
#	define VK_LOCATION_BLENDINDICES
#	define VK_LOCATION_NORMAL
#	define VK_LOCATION_TANGENT
#	define VK_LOCATION_BINORMAL
#	define VK_LOCATION_COLOR0
#	define VK_LOCATION_COLOR1
#	define VK_LOCATION_PSIZE

#	define VK_PUSH_CONSTANT
#	define VK_SUBPASS_INPUT(index, tex, sam, t, s, space)\
	Texture2D    tex : register(t, space);\
	SamplerState sam : register(s, space)
#	define VK_SUBPASS_LOAD(tex, sam, tc) tex.SampleLevel(sam, tc, 0.f)

#	define VK_POINT_SIZE
#	define VK_SET_POINT_SIZE
#endif

#ifdef DEF_INSTANCED
#	define _PER_INSTANCE_DATA\
	VK_LOCATION(16) float4 matPart0 : INSTDATA0;\
	VK_LOCATION(17) float4 matPart1 : INSTDATA1;\
	VK_LOCATION(18) float4 matPart2 : INSTDATA2;\
	VK_LOCATION(19) float4 instData : INSTDATA3;
#else
#	define _PER_INSTANCE_DATA
#endif

#define _TBN_SPACE(tan, bin, nrm)\
	const float3x3 matFromTBN = float3x3(tan, bin, nrm);\
	const float3x3 matToTBN   = transpose(matFromTBN);

#define _PI 3.141592654f

#define _SINGULARITY_FIX 0.001f

#define _MAX_TERRAIN_LAYERS 32

matrix ToFloat4x4(mataff m)
{
	return matrix(
		float4(m[0], 0),
		float4(m[1], 0),
		float4(m[2], 0),
		float4(m[3], 1));
}

float2 ToNdcPos(float2 tc)
{
	return tc * float2(2, -2) - float2(1, -1);
}

float2 ToTexCoords(float2 ndcPos)
{
	return ndcPos * float2(0.5f, -0.5f) + 0.5f;
}

// Asymmetric abs():
float2 AsymAbs(float2 x, float negScale = -1.f, float posScale = 1.f)
{
	return x * lerp(posScale, negScale, step(x, 0.f));
}

float3 Rand(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898f, 78.233f)) * float3(1, 2, 3)) * 43758.5453f);
}

float4 Rand2(float2 pos)
{
	const float4 primeA = float4(9.907f, 9.923f, 9.929f, 9.931f);
	const float4 primeB = float4(9.941f, 9.949f, 9.967f, 9.973f);
	return frac(primeA * pos.x + primeB * pos.y);
}

float3 RandomColor(float2 pos, float randLum, float randRGB)
{
	const float4 r = Rand2(pos);
	return (1.f - randLum - randRGB) + r.a * randLum + r.rgb * randRGB;
}

float3 NormalDither(float3 rand)
{
	const float2 rr = rand.xy * (1.f / 333.f) - (0.5f / 333.f);
	return float3(rr, 0);
}

static const float2 _POINT_SPRITE_POS_OFFSETS[4] =
{
	float2(-0.5f,  0.5f),
	float2(-0.5f, -0.5f),
	float2(0.5f,  0.5f),
	float2(0.5f, -0.5f)
};

static const float2 _POINT_SPRITE_TEX_COORDS[4] =
{
	float2(0, 0),
	float2(0, 1),
	float2(1, 0),
	float2(1, 1)
};

// <PhysicallyBasedRendering>

float3 ToRealAlbedo(float3 albedo)
{
	return max(albedo.rgb * 0.5f, 0.0001f);
}

float3 ToMetalColor(float3 albedo)
{
	const float gray = dot(albedo, 1.f / 3.f);
	return saturate(albedo / (gray + _SINGULARITY_FIX));
}

// Prevent values from reaching inf and nan.
float3 ToSafeHDR(float3 hdr)
{
	// Typical ambient is 4000.
	// Typical sunlight is 16000.
	return clamp(hdr, 0.f, 32000.f);
}

// See: https://en.wikipedia.org/wiki/Schlick%27s_approximation
float FresnelSchlick(float minRef, float maxAdd, float power)
{
	return saturate(minRef + maxAdd * power);
}

// </PhysicallyBasedRendering>

float UnpackTerrainHeight(float height)
{
	return height * 0.01f;
}

float ToLandMask(float height)
{
	return saturate(height * 0.2f + 1.f); // [-5 to 0] -> [0 to 1]
}

float ToWaterDiffuseMask(float height)
{
	const float mask = saturate(height * 0.02f + 1.f); // [-50 to 0] -> [0 to 1]
	return mask * mask * mask;
}

float3 ReflectionDimming(float3 hdr, float scale)
{
	const float gray = dot(hdr, 1.f / 3.f);
	return lerp(hdr * scale, hdr, saturate(gray * (1.f / 65536.f)));
}
