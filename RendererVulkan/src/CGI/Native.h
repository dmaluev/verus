#pragma once

namespace verus
{
	namespace CGI
	{
		VkFormat ToNativeFormat(Format format);

		VkCullModeFlagBits ToNativeCullMode(CullMode cullMode);

		VkImageLayout ToNativeImageLayout(ImageLayout layout);

		VkPolygonMode ToNativePolygonMode(PolygonMode polygonMode);

		VkPrimitiveTopology ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology);

		VkSampleCountFlagBits ToNativeSampleCount(int sampleCount);

		VkShaderStageFlags ToNativeStageFlags(ShaderStageFlags stageFlags);

		int ToNativeLocation(IeUsage usage, int usageIndex);
		VkFormat ToNativeFormat(IeUsage usage, IeType type, int components);
	}
}
