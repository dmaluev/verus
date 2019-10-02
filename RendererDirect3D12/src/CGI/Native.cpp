#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

DXGI_FORMAT CGI::ToNativeFormat(Format format)
{
	switch (format)
	{
	case Format::unormB4G4R4A4:     return DXGI_FORMAT_B4G4R4A4_UNORM;
	case Format::unormB5G6R5:       return DXGI_FORMAT_B5G6R5_UNORM;
	case Format::unormR10G10B10A2:  return DXGI_FORMAT_R10G10B10A2_UNORM;
	case Format::sintR16:           return DXGI_FORMAT_R16_SINT;

	case Format::unormR8:           return DXGI_FORMAT_R8_UNORM;
	case Format::unormR8G8:         return DXGI_FORMAT_R8G8_UNORM;
	case Format::unormR8G8B8A8:     return DXGI_FORMAT_R8G8B8A8_UNORM;
	case Format::unormB8G8R8A8:     return DXGI_FORMAT_B8G8R8A8_UNORM;
	case Format::srgbR8G8B8A8:      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case Format::srgbB8G8R8A8:      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

	case Format::floatR16:          return DXGI_FORMAT_R16_FLOAT;
	case Format::floatR16G16:       return DXGI_FORMAT_R16G16_FLOAT;
	case Format::floatR16G16B16A16: return DXGI_FORMAT_R16G16B16A16_FLOAT;

	case Format::floatR32:          return DXGI_FORMAT_R32_FLOAT;
	case Format::floatR32G32:       return DXGI_FORMAT_R32G32_FLOAT;
	case Format::floatR32G32B32A32: return DXGI_FORMAT_R32G32B32A32_FLOAT;

	case Format::unormD16:          return DXGI_FORMAT_D16_UNORM;
	case Format::unormD24uintS8:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case Format::floatD32:          return DXGI_FORMAT_D32_FLOAT;

	case Format::unormBC1:          return DXGI_FORMAT_BC1_UNORM;
	case Format::unormBC2:          return DXGI_FORMAT_BC2_UNORM;
	case Format::unormBC3:          return DXGI_FORMAT_BC3_UNORM;
	case Format::srgbBC1:           return DXGI_FORMAT_BC1_UNORM_SRGB;
	case Format::srgbBC2:           return DXGI_FORMAT_BC2_UNORM_SRGB;
	case Format::srgbBC3:           return DXGI_FORMAT_BC3_UNORM_SRGB;

	default: throw VERUS_RECOVERABLE << "ToNativeFormat()";
	};
}

D3D12_CULL_MODE CGI::ToNativeCullMode(CullMode cullMode)
{
	switch (cullMode)
	{
	case CullMode::none:  return D3D12_CULL_MODE_NONE;
	case CullMode::front: return D3D12_CULL_MODE_FRONT;
	case CullMode::back:  return D3D12_CULL_MODE_BACK;
	default: throw VERUS_RECOVERABLE << "ToNativeCullMode()";
	}
}

D3D12_RESOURCE_STATES CGI::ToNativeImageLayout(ImageLayout layout)
{
	switch (layout)
	{
	case ImageLayout::undefined:                     return D3D12_RESOURCE_STATE_COMMON;
	case ImageLayout::general:                       return D3D12_RESOURCE_STATE_COMMON;
	case ImageLayout::colorAttachmentOptimal:        return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case ImageLayout::depthStencilAttachmentOptimal: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case ImageLayout::depthStencilReadOnlyOptimal:   return D3D12_RESOURCE_STATE_DEPTH_READ;
	case ImageLayout::shaderReadOnlyOptimal:         return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case ImageLayout::transferSrcOptimal:            return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case ImageLayout::transferDstOptimal:            return D3D12_RESOURCE_STATE_COPY_DEST;
	case ImageLayout::presentSrc:                    return D3D12_RESOURCE_STATE_PRESENT;
	default: throw VERUS_RECOVERABLE << "ToNativeImageLayout()";
	}
}

D3D12_FILL_MODE CGI::ToNativePolygonMode(PolygonMode polygonMode)
{
	switch (polygonMode)
	{
	case PolygonMode::fill: return D3D12_FILL_MODE_SOLID;
	case PolygonMode::line: return D3D12_FILL_MODE_WIREFRAME;
	default: throw VERUS_RECOVERABLE << "ToNativePolygonMode()";
	}
}

D3D_PRIMITIVE_TOPOLOGY CGI::ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
	case PrimitiveTopology::pointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PrimitiveTopology::lineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PrimitiveTopology::lineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case PrimitiveTopology::triangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PrimitiveTopology::triangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	default: throw VERUS_RECOVERABLE << "ToNativePrimitiveTopology()";
	}
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE CGI::ToNativePrimitiveTopologyType(PrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
	case PrimitiveTopology::pointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case PrimitiveTopology::lineList:      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::lineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopology::triangleList:  return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	case PrimitiveTopology::triangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	default: throw VERUS_RECOVERABLE << "ToNativePrimitiveTopologyType()";
	}
}

D3D12_COMPARISON_FUNC CGI::ToNativeCompareOp(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::never:          return D3D12_COMPARISON_FUNC_NEVER;
	case CompareOp::less:           return D3D12_COMPARISON_FUNC_LESS;
	case CompareOp::equal:          return D3D12_COMPARISON_FUNC_EQUAL;
	case CompareOp::lessOrEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case CompareOp::greater:        return D3D12_COMPARISON_FUNC_GREATER;
	case CompareOp::notEqual:       return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case CompareOp::greaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case CompareOp::always:         return D3D12_COMPARISON_FUNC_ALWAYS;
	default: throw VERUS_RECOVERABLE << "ToNativeCompareOp()";
	}
}

CSZ CGI::ToNativeSemanticName(IeUsage usage)
{
	static const CSZ names[] =
	{
		"POSITION",     // IeUsage::position
		"BLENDWEIGHT",  // IeUsage::blendWeight
		"BLENDINDICES", // IeUsage::blendIndices
		"NORMAL",       // IeUsage::normal
		"PSIZE",        // IeUsage::psize
		"TEXCOORD",     // IeUsage::texCoord
		"TEXCOORD",     // IeUsage::tangent
		"TEXCOORD",     // IeUsage::binormal
		"COLOR"         // IeUsage::color
	};
	return names[+usage];
}

DXGI_FORMAT CGI::ToNativeFormat(IeUsage usage, IeType type, int components)
{
	VERUS_RT_ASSERT(components >= 1 && components <= 4);
	int index = components - 1;
	static const DXGI_FORMAT floats[] =
	{
		DXGI_FORMAT_R32_FLOAT,
		DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT_R32G32B32_FLOAT,
		DXGI_FORMAT_R32G32B32A32_FLOAT
	};
	static const DXGI_FORMAT bytes[] =
	{
		DXGI_FORMAT_R8G8B8A8_UINT,
		DXGI_FORMAT_R8G8B8A8_UINT,
		DXGI_FORMAT_R8G8B8A8_UINT,
		DXGI_FORMAT_R8G8B8A8_UINT,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		DXGI_FORMAT_R8G8B8A8_SNORM
	};
	static const DXGI_FORMAT shorts[] =
	{
		DXGI_FORMAT_R16G16_SINT,
		DXGI_FORMAT_R16G16_SINT,
		DXGI_FORMAT_R16G16B16A16_SINT,
		DXGI_FORMAT_R16G16B16A16_SINT,
		DXGI_FORMAT_R16G16_SNORM,
		DXGI_FORMAT_R16G16_SNORM,
		DXGI_FORMAT_R16G16B16A16_SNORM,
		DXGI_FORMAT_R16G16B16A16_SNORM
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
