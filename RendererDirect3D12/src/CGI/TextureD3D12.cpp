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
	_desc._mipLevels = desc._mipLevels ? desc._mipLevels : Math::ComputeMipLevels(desc._width, desc._height, desc._depth);
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);
	const bool depthFormat = IsDepthFormat(desc._format);

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = _desc._depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = _desc._width;
	resDesc.Height = _desc._height;
	resDesc.DepthOrArraySize = Math::Max(_desc._depth, _desc._arrayLayers);
	resDesc.MipLevels = _desc._mipLevels;
	resDesc.Format = ToNativeFormat(_desc._format);
	resDesc.SampleDesc.Count = _desc._sampleCount;
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

	if (_desc._generateMips)
	{
		resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(&_pResourceUAV))))
			throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(), hr=" << VERUS_HR(hr);

		_dhUAV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _desc._mipLevels);
		VERUS_FOR(i, _desc._mipLevels)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = resDesc.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = i;
			pRendererD3D12->GetD3DDevice()->CreateUnorderedAccessView(_pResourceUAV.Get(), nullptr, &uavDesc, _dhUAV.AtCPU(i));
		}
	}

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

	_vStagingBuffers.reserve(_desc._mipLevels*_desc._arrayLayers);
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
	UINT64 bufferSize = Math::AlignUp(_bytesPerPixel * w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * h;
	if (IsBC(_desc._format))
		bufferSize = Math::AlignUp(IO::DDSHeader::ComputeBcPitch(w, h, IsBC1(_desc._format)), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * Math::Max(h / 4, 1);

	const int sbIndex = arrayLayer * _desc._mipLevels + mipLevel;
	if (_vStagingBuffers.size() <= sbIndex)
		_vStagingBuffers.resize(sbIndex + 1);

	auto& sb = _vStagingBuffers[sbIndex];
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
	if (IsBC(_desc._format))
	{
		sd.RowPitch = IO::DDSHeader::ComputeBcPitch(w, h, IsBC1(_desc._format));
		sd.SlicePitch = IO::DDSHeader::ComputeBcLevelSize(w, h, IsBC1(_desc._format));
	}
	else
	{
		sd.RowPitch = _bytesPerPixel * w;
		sd.SlicePitch = _bytesPerPixel * w * h;
	}
	const UINT subresource = D3D12CalcSubresource(mipLevel, arrayLayer, 0, _desc._mipLevels, _desc._arrayLayers);
	UpdateSubresources<1>(pCmdList,
		_pResource.Get(),
		sb.Get(),
		0,
		subresource,
		1,
		&sd);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, mipLevel, arrayLayer);

	_destroyStagingBuffers.Schedule();
}

void TextureD3D12::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._generateMips);
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();

	auto tex = TexturePtr::From(this);

	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, Range<int>(1, _desc._mipLevels - 1));

	pCB->BindPipeline(renderer.GetPipelineGenerateMips());

	auto shader = renderer.GetShaderGenerateMips();
	shader->BeginBindDescriptors();

	auto& ub = renderer.GetUbGenerateMips();
	ub._isSRGB = false;

	for (int srcMip = 0; srcMip < _desc._mipLevels - 1;)
	{
		const int srcWidth = _desc._width >> srcMip;
		const int srcHeight = _desc._height >> srcMip;
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		ub._srcDimensionCase = (srcHeight & 1) << 1 | (srcWidth & 1);

		int numMips = 4;
		numMips = Math::Min(4, numMips + 1);
		numMips = ((srcMip + numMips) >= _desc._mipLevels) ? _desc._mipLevels - srcMip - 1 : numMips;

		ub._srcMipLevel = srcMip;
		ub._numMipLevels = numMips;
		ub._texelSize.x = 1.f / dstWidth;
		ub._texelSize.y = 1.f / dstHeight;

		int mips[5] = {};
		VERUS_FOR(mip, numMips)
			mips[mip + 1] = srcMip + mip + 1;

		const int complexDescSetID = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex }, mips);

		pCB->BindDescriptors(shader, 0, complexDescSetID);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		auto rb = CD3DX12_RESOURCE_BARRIER::UAV(_pResourceUAV.Get());
		pCmdList->ResourceBarrier(1, &rb);

		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::shaderReadOnlyOptimal, ImageLayout::transferDstOptimal, Range<int>(srcMip + 1, srcMip + numMips));
		VERUS_FOR(mip, numMips)
		{
			const int sub = srcMip + mip + 1;
			auto rb = CD3DX12_RESOURCE_BARRIER::Transition(_pResourceUAV.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE, sub);
			pCmdList->ResourceBarrier(1, &rb);
			pCmdList->CopyTextureRegion(
				&CD3DX12_TEXTURE_COPY_LOCATION(_pResource.Get(), sub),
				0, 0, 0,
				&CD3DX12_TEXTURE_COPY_LOCATION(_pResourceUAV.Get(), sub),
				nullptr);
		}
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, Range<int>(srcMip + 1, srcMip + numMips));

		srcMip += numMips;
	}

	shader->EndBindDescriptors();
}

void TextureD3D12::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	for (const auto& x : _vStagingBuffers)
		VERUS_COM_RELEASE_CHECK(x.Get());
	_vStagingBuffers.clear();
}
