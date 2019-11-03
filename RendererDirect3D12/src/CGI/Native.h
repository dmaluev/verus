#pragma once

namespace verus
{
	namespace CGI
	{
		DXGI_FORMAT ToNativeFormat(Format format, bool typeless);
		DXGI_FORMAT ToNativeSampledDepthFormat(Format format);

		D3D12_CULL_MODE ToNativeCullMode(CullMode cullMode);

		D3D12_RESOURCE_STATES ToNativeImageLayout(ImageLayout layout);

		D3D12_FILL_MODE ToNativePolygonMode(PolygonMode polygonMode);

		D3D_PRIMITIVE_TOPOLOGY ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology);
		D3D12_PRIMITIVE_TOPOLOGY_TYPE ToNativePrimitiveTopologyType(PrimitiveTopology primitiveTopology);

		D3D12_COMPARISON_FUNC ToNativeCompareOp(CompareOp compareOp);

		CSZ ToNativeSemanticName(IeUsage usage);
		DXGI_FORMAT ToNativeFormat(IeUsage usage, IeType type, int components);
	}
}
