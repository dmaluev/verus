#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

TextureD3D12::TextureD3D12()
{
}

TextureD3D12::~TextureD3D12()
{
	Done();
}

void TextureD3D12::Init(RcTextureDesc desc)
{
	VERUS_INIT();
	VERUS_RT_ASSERT(desc._width > 0 && desc._height > 0);
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	_size = Vector4(
		float(desc._width),
		float(desc._height),
		1.f / desc._width,
		1.f / desc._height);
	_desc = desc;
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);
	const bool depthFormat = IsDepthFormat(desc._format);

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = desc._depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = desc._width;
	resDesc.Height = desc._height;
	resDesc.DepthOrArraySize = Math::Max(desc._depth, desc._arrayLayers);
	resDesc.MipLevels = desc._mipLevels;
	resDesc.Format = ToNativeFormat(desc._format);
	resDesc.SampleDesc.Count = desc._sampleCount;
	resDesc.Flags = depthFormat ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE;
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = resDesc.Format;
	clearValue.DepthStencil.Depth = 1;
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		depthFormat ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST,
		depthFormat ? &clearValue : nullptr,
		IID_PPV_ARGS(&_pResource))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(), hr=" << VERUS_HR(hr);

	if (depthFormat)
	{
		_dhDSV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		pRendererD3D12->GetD3DDevice()->CreateDepthStencilView(_pResource.Get(), nullptr, _dhDSV.AtCPU(0));
	}
	else
	{
		_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_pResource.Get(), nullptr, _dhSRV.AtCPU(0));
	}

	_dhSampler.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);
	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	pRendererD3D12->GetD3DDevice()->CreateSampler(&samplerDesc, _dhSampler.AtCPU(0));

	_vStagingBuffers.reserve(desc._mipLevels);
}

void TextureD3D12::Done()
{
	_destroyStagingBuffers.Allow();
	DestroyStagingBuffers();

	_dhSampler.Reset();
	_dhDSV.Reset();
	_dhRTV.Reset();
	_dhSRV.Reset();
	_pResource.Reset();

	VERUS_DONE(TextureD3D12);
}

void TextureD3D12::UpdateImage(int mipLevel, const void* p, int arrayLayer, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const int w = Math::Max(1, _desc._width >> mipLevel);
	const int h = Math::Max(1, _desc._height >> mipLevel);
	UINT64 bufferSize = w * h * _bytesPerPixel;
	if (IsBC(_desc._format))
		bufferSize = IO::DDSHeader::ComputeBcLevelSize(w, h, IsBC1(_desc._format));

	if (_vStagingBuffers.size() <= mipLevel)
		_vStagingBuffers.resize(mipLevel + 1);

	auto& sb = _vStagingBuffers[mipLevel];
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&sb))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(), hr=" << VERUS_HR(hr);

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	D3D12_SUBRESOURCE_DATA sd = {};
	sd.pData = p;
	sd.RowPitch = IsBC(_desc._format) ? IO::DDSHeader::ComputeBcPitch(w, h, IsBC1(_desc._format)) : w * _bytesPerPixel;
	sd.SlicePitch = bufferSize;
	const UINT subresource = D3D12CalcSubresource(mipLevel, 0, 0, _desc._mipLevels, _desc._arrayLayers);
	UpdateSubresources(pCmdList,
		_pResource.Get(),
		sb.Get(),
		0,
		subresource,
		1,
		&sd);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, mipLevel);

	_destroyStagingBuffers.Schedule();
}

void TextureD3D12::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	for (const auto& x : _vStagingBuffers)
		VERUS_COM_RELEASE_CHECK(x.Get());
	_vStagingBuffers.clear();
}
