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

float CalcNormalZ(float2 v)
{
	return sqrt(saturate(1.0 - dot(v.rg, v.rg)));
}

float4 NormalMapAA(float4 rawNormal)
{
	float3 normal = rawNormal.agb*-2.0 + 1.0; // Dmitry's reverse!
	normal.b = CalcNormalZ(normal.rg);
	return float4(normal, 0.8 + rawNormal.b*0.8);
}
