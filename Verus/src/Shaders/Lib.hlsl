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

#define mataff float4x3
