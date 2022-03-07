// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		VkCompareOp ToNativeCompareOp(CompareOp compareOp);

		uint32_t ToNativeCubeMapFace(CubeMapFace face);

		VkPolygonMode ToNativePolygonMode(PolygonMode polygonMode);

		VkCullModeFlagBits ToNativeCullMode(CullMode cullMode);

		VkPrimitiveTopology ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology);

		VkImageLayout ToNativeImageLayout(ImageLayout layout);

		VkShaderStageFlags ToNativeStageFlags(ShaderStageFlags stageFlags);

		VkSampleCountFlagBits ToNativeSampleCount(int sampleCount);

		VkFormat ToNativeFormat(Format format);

		int ToNativeLocation(ViaUsage usage, int usageIndex);
		VkFormat ToNativeFormat(ViaUsage usage, ViaType type, int components);
	}
}
