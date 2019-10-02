#include "stdafx.h"

using namespace verus;

VkFormat CGI::ToNativeFormat(Format format)
{
	switch (format)
	{
	case Format::unormB4G4R4A4:     return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
	case Format::unormB5G6R5:       return VK_FORMAT_B5G6R5_UNORM_PACK16;
	case Format::unormR10G10B10A2:  return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case Format::sintR16:           return VK_FORMAT_R16_SINT;

	case Format::unormR8:           return VK_FORMAT_R8_UNORM;
	case Format::unormR8G8:         return VK_FORMAT_R8G8_UNORM;
	case Format::unormR8G8B8A8:     return VK_FORMAT_R8G8B8A8_UNORM;
	case Format::unormB8G8R8A8:     return VK_FORMAT_B8G8R8A8_UNORM;
	case Format::srgbR8G8B8A8:      return VK_FORMAT_R8G8B8A8_SRGB;
	case Format::srgbB8G8R8A8:      return VK_FORMAT_B8G8R8A8_SRGB;

	case Format::floatR16:          return VK_FORMAT_R16_SFLOAT;
	case Format::floatR16G16:       return VK_FORMAT_R16G16_SFLOAT;
	case Format::floatR16G16B16A16: return VK_FORMAT_R16G16B16A16_SFLOAT;

	case Format::floatR32:          return VK_FORMAT_R32_SFLOAT;
	case Format::floatR32G32:       return VK_FORMAT_R32G32_SFLOAT;
	case Format::floatR32G32B32A32: return VK_FORMAT_R32G32B32A32_SFLOAT;

	case Format::unormD16:          return VK_FORMAT_D16_UNORM;
	case Format::unormD24uintS8:    return VK_FORMAT_D24_UNORM_S8_UINT;
	case Format::floatD32:          return VK_FORMAT_D32_SFLOAT;

	case Format::unormBC1:          return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case Format::unormBC2:          return VK_FORMAT_BC2_UNORM_BLOCK;
	case Format::unormBC3:          return VK_FORMAT_BC3_UNORM_BLOCK;
	case Format::srgbBC1:           return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case Format::srgbBC2:           return VK_FORMAT_BC2_SRGB_BLOCK;
	case Format::srgbBC3:           return VK_FORMAT_BC3_SRGB_BLOCK;

	default: throw VERUS_RECOVERABLE << "ToNativeFormat()";
	}
}

VkCullModeFlagBits CGI::ToNativeCullMode(CullMode cullMode)
{
	switch (cullMode)
	{
	case CullMode::none:  return VK_CULL_MODE_NONE;
	case CullMode::front: return VK_CULL_MODE_FRONT_BIT;
	case CullMode::back:  return VK_CULL_MODE_BACK_BIT;
	default: throw VERUS_RECOVERABLE << "ToNativeCullMode()";
	}
}

VkImageLayout CGI::ToNativeImageLayout(ImageLayout layout)
{
	switch (layout)
	{
	case ImageLayout::undefined:                     return VK_IMAGE_LAYOUT_UNDEFINED;
	case ImageLayout::general:                       return VK_IMAGE_LAYOUT_GENERAL;
	case ImageLayout::colorAttachmentOptimal:        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case ImageLayout::depthStencilAttachmentOptimal: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case ImageLayout::depthStencilReadOnlyOptimal:   return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case ImageLayout::shaderReadOnlyOptimal:         return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case ImageLayout::transferSrcOptimal:            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case ImageLayout::transferDstOptimal:            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case ImageLayout::presentSrc:                    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default: throw VERUS_RECOVERABLE << "ToNativeImageLayout()";
	}
}

VkPolygonMode CGI::ToNativePolygonMode(PolygonMode polygonMode)
{
	switch (polygonMode)
	{
	case PolygonMode::fill: return VK_POLYGON_MODE_FILL;
	case PolygonMode::line: return VK_POLYGON_MODE_LINE;
	default: throw VERUS_RECOVERABLE << "ToNativePolygonMode()";
	}
}

VkPrimitiveTopology CGI::ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
	case PrimitiveTopology::pointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case PrimitiveTopology::lineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case PrimitiveTopology::lineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case PrimitiveTopology::triangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case PrimitiveTopology::triangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	default: throw VERUS_RECOVERABLE << "ToNativePrimitiveTopology()";
	}
}

VkSampleCountFlagBits CGI::ToNativeSampleCount(int sampleCount)
{
	switch (sampleCount)
	{
	case 1:  return VK_SAMPLE_COUNT_1_BIT;
	case 2:  return VK_SAMPLE_COUNT_2_BIT;
	case 4:  return VK_SAMPLE_COUNT_4_BIT;
	case 8:  return VK_SAMPLE_COUNT_8_BIT;
	case 16: return VK_SAMPLE_COUNT_16_BIT;
	case 32: return VK_SAMPLE_COUNT_32_BIT;
	case 64: return VK_SAMPLE_COUNT_64_BIT;
	default: throw VERUS_RECOVERABLE << "ToNativeSampleCount()";
	}
}

VkCompareOp CGI::ToNativeCompareOp(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::never:          return VK_COMPARE_OP_NEVER;
	case CompareOp::less:           return VK_COMPARE_OP_LESS;
	case CompareOp::equal:          return VK_COMPARE_OP_EQUAL;
	case CompareOp::lessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
	case CompareOp::greater:        return VK_COMPARE_OP_GREATER;
	case CompareOp::notEqual:       return VK_COMPARE_OP_NOT_EQUAL;
	case CompareOp::greaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case CompareOp::always:         return VK_COMPARE_OP_ALWAYS;
	default: throw VERUS_RECOVERABLE << "ToNativeCompareOp()";
	}
}

VkShaderStageFlags CGI::ToNativeStageFlags(ShaderStageFlags stageFlags)
{
	VkShaderStageFlags ret = 0;
	if (stageFlags & ShaderStageFlags::vs)
		ret |= VK_SHADER_STAGE_VERTEX_BIT;
	if (stageFlags & ShaderStageFlags::hs)
		ret |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	if (stageFlags & ShaderStageFlags::ds)
		ret |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	if (stageFlags & ShaderStageFlags::gs)
		ret |= VK_SHADER_STAGE_GEOMETRY_BIT;
	if (stageFlags & ShaderStageFlags::fs)
		ret |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if (stageFlags & ShaderStageFlags::cs)
		ret |= VK_SHADER_STAGE_COMPUTE_BIT;
	return ret;
}

int CGI::ToNativeLocation(IeUsage usage, int usageIndex)
{
	switch (usage)
	{
	case IeUsage::position: return 0;
	case IeUsage::blendWeight: return 1;
	case IeUsage::blendIndices: return 6;
	case IeUsage::normal: return 2;
	case IeUsage::psize: return 7;
	case IeUsage::texCoord: return 8 + usageIndex;
	case IeUsage::tangent: return 14;
	case IeUsage::binormal: return 15;
	case IeUsage::color: return 3 + usageIndex;
	default: throw VERUS_RECOVERABLE << "ToNativeLocation()";
	}
}

VkFormat CGI::ToNativeFormat(IeUsage usage, IeType type, int components)
{
	VERUS_RT_ASSERT(components >= 1 && components <= 4);
	int index = components - 1;
	static const VkFormat floats[] =
	{
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT
	};
	static const VkFormat bytes[] =
	{
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM
	};
	static const VkFormat shorts[] =
	{
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_SNORM
	};
	switch (type)
	{
	case IeType::floats:
	{
		return floats[index];
	}
	break;
	case IeType::ubytes:
	{
		VERUS_RT_ASSERT(4 == components);
		switch (usage)
		{
		case IeUsage::normal: index += 8; break; // SNORM.
		case IeUsage::color: index += 4; break; // UNORM.
		}
		return bytes[index];
	}
	break;
	case IeType::shorts:
	{
		VERUS_RT_ASSERT(2 == components || 4 == components);
		switch (usage)
		{
		case IeUsage::tangent:
		case IeUsage::binormal:
			index += 4; break; // SNORM.
		}
		return shorts[index];
	}
	break;
	default: throw VERUS_RECOVERABLE << "ToNativeFormat(), IeType=?";
	}
}
