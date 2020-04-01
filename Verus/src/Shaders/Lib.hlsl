// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

#define VERUS_UBUFFER struct
#define mataff float4x3

#ifdef _VULKAN
#	define VK_LOCATION(x)           [[vk::location(x)]]
#	define VK_LOCATION_POSITION     [[vk::location(0)]]
#	define VK_LOCATION_BLENDWEIGHT  [[vk::location(1)]]
#	define VK_LOCATION_BLENDINDICES [[vk::location(6)]]
#	define VK_LOCATION_NORMAL       [[vk::location(2)]]
#	define VK_LOCATION_PSIZE        [[vk::location(7)]]
#	define VK_LOCATION_COLOR0       [[vk::location(3)]]
#	define VK_LOCATION_COLOR1       [[vk::location(4)]]

#	define VK_PUSH_CONSTANT [[vk::push_constant]]
#	define VK_SUBPASS_INPUT(index, tex, sam, t, s, space) layout(input_attachment_index = index) SubpassInput<float4> tex : register(t, space)
#	define VK_SUBPASS_LOAD(tex, sam, tc) tex.SubpassLoad()

#	ifdef _VS
#		define VK_POINT_SIZE [[vk::builtin("PointSize")]] float psize : PSIZE;
#	else
#		define VK_POINT_SIZE
#	endif
#	define VK_SET_POINT_SIZE so.psize = 1.0
#else
#	define VK_LOCATION(x)
#	define VK_LOCATION_POSITION
#	define VK_LOCATION_BLENDWEIGHT
#	define VK_LOCATION_BLENDINDICES
#	define VK_LOCATION_NORMAL
#	define VK_LOCATION_PSIZE
#	define VK_LOCATION_COLOR0
#	define VK_LOCATION_COLOR1

#	define VK_PUSH_CONSTANT
#	define VK_SUBPASS_INPUT(index, tex, sam, t, s, space)\
	Texture2D    tex : register(t, space);\
	SamplerState sam : register(s, space)
#	define VK_SUBPASS_LOAD(tex, sam, tc) tex.SampleLevel(sam, tc, 0)

#	define VK_POINT_SIZE
#	define VK_SET_POINT_SIZE
#endif

#ifdef DEF_INSTANCED
#	define _PER_INSTANCE_DATA\
	VK_LOCATION(16) float4 matPart0 : TEXCOORD8;\
	VK_LOCATION(17) float4 matPart1 : TEXCOORD9;\
	VK_LOCATION(18) float4 matPart2 : TEXCOORD10;\
	VK_LOCATION(19) float4 instData : TEXCOORD11;
#else
#	define _PER_INSTANCE_DATA
#endif

#define _TBN_SPACE(tan, bin, nrm)\
	const float3x3 matFromTBN = float3x3(tan, bin, nrm);\
	const float3x3 matToTBN   = transpose(matFromTBN);

#define _SINGULARITY_FIX 0.001

#define _MAX_TERRAIN_LAYERS 32

matrix ToFloat4x4(mataff m)
{
	return matrix(
		float4(m[0], 0),
		float4(m[1], 0),
		float4(m[2], 0),
		float4(m[3], 1));
}

float ComputeNormalZ(float2 v)
{
	return sqrt(saturate(1.0 - dot(v.rg, v.rg)));
}

float4 NormalMapAA(float4 rawNormal)
{
	float3 normal = rawNormal.agb * -2.0 + 1.0; // Dmitry's reverse!
	normal.b = ComputeNormalZ(normal.rg);
	return float4(normal, 0.8 + rawNormal.b * 0.8);
}

float3 Rand(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233)) * float3(1, 2, 3)) * 43758.5453);
}

float3 NormalDither(float3 rand)
{
	const float2 rr = rand.xy * (1.0 / 333.0) - (0.5 / 333.0);
	return float3(rr, 0);
}
