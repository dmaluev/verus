// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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

		VkCompareOp ToNativeCompareOp(CompareOp compareOp);

		VkShaderStageFlags ToNativeStageFlags(ShaderStageFlags stageFlags);

		int ToNativeLocation(ViaUsage usage, int usageIndex);
		VkFormat ToNativeFormat(ViaUsage usage, ViaType type, int components);

		uint32_t ToNativeCubeMapFace(CubeMapFace face);
	}
}
