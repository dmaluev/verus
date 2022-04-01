// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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
#	define VK_SET_POINT_SIZE so.psize = 1.0
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
#	define VK_SUBPASS_LOAD(tex, sam, tc) tex.SampleLevel(sam, tc, 0.0)

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

// <Constants>
#define _PI 3.141592654
#define _SINGULARITY_FIX 0.001
#define _MAX_TERRAIN_LAYERS 32
// </Constants>

static const float2 _POINT_SPRITE_POS_OFFSETS[4] =
{
	float2(-0.5,  0.5),
	float2(-0.5, -0.5),
	float2(0.5,  0.5),
	float2(0.5, -0.5)
};
static const float2 _POINT_SPRITE_TEX_COORDS[4] =
{
	float2(0, 0),
	float2(0, 1),
	float2(1, 0),
	float2(1, 1)
};

// <TheMatrix>
#define _TBN_SPACE(tan, bin, nrm)\
	const float3x3 matFromTBN = float3x3(tan, bin, nrm);\
	const float3x3 matToTBN   = transpose(matFromTBN);

matrix ToFloat4x4(mataff m)
{
	return matrix(
		float4(m[0], 0),
		float4(m[1], 0),
		float4(m[2], 0),
		float4(m[3], 1));
}
// </TheMatrix>

// <ScreenSpace>
float2 ToNdcPos(float2 tc)
{
	return tc * float2(2, -2) - float2(1, -1);
}

float2 ToTexCoords(float2 ndcPos)
{
	return ndcPos * float2(0.5, -0.5) + 0.5;
}
// </ScreenSpace>

// <Random>
uint3 PcgHash3(uint3 v)
{
	v = v * 1664525u + 1013904223u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	v ^= v >> 16u;

	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;

	return v;
}

uint4 PcgHash4(uint4 v)
{
	v = v * 1664525u + 1013904223u;

	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;

	v ^= v >> 16u;

	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;

	return v;
}

float3 Rand(float2 v)
{
	return PcgHash3(uint3(v, 0)) * (1.0 / float(0xFFFFFFFFu));
}

float4 Rand4(float2 v)
{
	return PcgHash4(uint4(v, v)) * (1.0 / float(0xFFFFFFFFu));
}

float3 RandomColor(float2 pos, float randLum, float randRGB)
{
	const float4 r = Rand4(pos);
	return ((1.0 - randLum - randRGB) * 0.5) + r.a * randLum + r.rgb * randRGB;
}

float3 NormalDither(float3 rand)
{
	const float2 rr = rand.xy * (1.0 / 400.0) - (0.5 / 400.0);
	return float3(rr, 0);
}

float Dither2x2(float2 fragCoord)
{
	const float2 pos = floor(fragCoord);
	const float scale = 0.5;
	return frac((pos.x + pos.y) * scale) + frac(pos.x * scale) * scale;
}

float Dither3x3(float2 fragCoord)
{
	const float2 pos = floor(fragCoord);
	const float scale = 1.0 / 3.0;
	return frac((pos.x + pos.y) * scale) + frac(pos.x * scale) * scale;
}
// </Random>

// <Math>
float ComputeFade(float x, float from, float to)
{
	return saturate((x - from) * (1.0 / (to - from)));
}

// Asymmetric abs():
float2 AsymAbs(float2 x, float negScale = -1.0, float posScale = 1.0)
{
	return x * lerp(posScale, negScale, step(x, 0.0));
}

float SinAcos(float x) // sin(acos(x))
{
	return sqrt(saturate(1.0 - x * x));
}
// </Math>

// <PhysicallyBasedRendering>
// See: https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 FresnelSchlick(float3 f0, float3 f1, float pow5)
{
	return f0 + f1 * pow5;
}

float3 CheapRefract(float3 i, float3 n)
{
	return lerp(i, -n, 0.5);
}

// Prevent values from reaching inf and nan.
float3 SaturateHDR(float3 hdr)
{
	// Typical ambient lit surface is 4000.
	// Typical sunlight lit surface is 16000.
	return clamp(hdr, 0.0, 32000.0);
}

// If two channels are mutually exclusive, they can be stored as one channel:
float CombineMutexChannels(float2 x)
{
	return saturate((x.x - x.y) * 0.5 + 0.5);
}
float2 SeparateMutexChannels(float x)
{
	const float x2 = x * 2.0;
	return saturate(float2(x2 - 1.0, 1.0 - x2));
}
float2 CleanMutexChannels(float2 x) // Filter out BC7 compression artifacts:
{
	return saturate((x - (15.0 / 255.0)) * (255.0 / 240.0));
}
// </PhysicallyBasedRendering>

// <Misc>
float AlphaToResolveDitheringMask(float alpha)
{
	return 1.0 - floor(alpha + (15.0 / 255.0));
}

float UnpackTerrainHeight(float height)
{
	return height * 0.01;
}

float ToLandMask(float height)
{
	return saturate(height * 0.2 + 1.0); // [-5 to 0] -> [0 to 1]
}

float ToWaterPlanktonMask(float height)
{
	const float mask = saturate(height * 0.02 + 1.0); // [-50 to 0] -> [0 to 1]
	return mask * mask * mask;
}

// Also known as The Multiplier Map, for hemicube's shape compensation.
float ComputeHemicubeMask(float2 tc)
{
	// Hint. Compare two pixels when looking at them from the focal point of hemicube:
	// one is in the middle of hemicube's face and the other one is at the corner.
	// What will be the ratio of their areas (solid angles)?
	const float2 atc = abs(tc * 2.0 - 1.0);
	if (atc.y < 0.5)
	{
		if (atc.x < 0.5)
		{
			const float3 faceNrm = float3(0, 0, 1);
			const float3 v = float3(atc.x * 2.0, atc.y * 2.0, 1);
			const float len = length(v);
			const float3 vn = v / len;

			const float cosLaw = vn.z;
			const float distFactor = 1.0 / (len * len);
			const float faceFactor = dot(vn, faceNrm);

			return cosLaw * distFactor * faceFactor;
		}
		else
		{
			const float3 faceNrm = float3(1, 0, 0);
			const float3 v = float3(1, atc.y * 2.0, 2.0 - 2.0 * atc.x);
			const float len = length(v);
			const float3 vn = v / len;

			const float cosLaw = vn.z;
			const float distFactor = 1.0 / (len * len);
			const float faceFactor = dot(vn, faceNrm);

			return cosLaw * distFactor * faceFactor;
		}
	}
	else
	{
		if (atc.x < 0.5)
		{
			const float3 faceNrm = float3(0, 1, 0);
			const float3 v = float3(atc.x * 2.0, 1, 2.0 - 2.0 * atc.y);
			const float len = length(v);
			const float3 vn = v / len;

			const float cosLaw = vn.z;
			const float distFactor = 1.0 / (len * len);
			const float faceFactor = dot(vn, faceNrm);

			return cosLaw * distFactor * faceFactor;
		}
		else
		{
			return 0.0;
		}
	}
}
// </Misc>
