// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	D3D12_COMPARISON_FUNC ToNativeCompareOp(CompareOp compareOp);

	UINT ToNativeCubeMapFace(CubeMapFace face);

	D3D12_FILL_MODE ToNativePolygonMode(PolygonMode polygonMode);

	D3D12_CULL_MODE ToNativeCullMode(CullMode cullMode);

	D3D_PRIMITIVE_TOPOLOGY ToNativePrimitiveTopology(PrimitiveTopology primitiveTopology);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE ToNativePrimitiveTopologyType(PrimitiveTopology primitiveTopology);

	D3D12_RESOURCE_STATES ToNativeImageLayout(ImageLayout layout);

	DXGI_FORMAT ToNativeFormat(Format format, bool typeless);
	DXGI_FORMAT ToNativeSampledDepthFormat(Format format);

	CSZ ToNativeSemanticName(ViaUsage usage);
	DXGI_FORMAT ToNativeFormat(ViaUsage usage, ViaType type, int components);
}
