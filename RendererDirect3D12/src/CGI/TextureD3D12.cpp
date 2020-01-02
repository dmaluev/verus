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
	const bool renderTarget = (_desc._flags & TextureDesc::Flags::colorAttachment);
	const bool depthFormat = IsDepthFormat(desc._format);
	const bool depthSampled = _desc._flags & TextureDesc::Flags::depthSampled;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = _desc._depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width = _desc._width;
	resDesc.Height = _desc._height;
	resDesc.DepthOrArraySize = Math::Max(_desc._depth, _desc._arrayLayers);
	resDesc.MipLevels = _desc._mipLevels;
	resDesc.Format = ToNativeFormat(_desc._format, depthSampled);
	resDesc.SampleDesc.Count = _desc._sampleCount;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (renderTarget)
		resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (depthFormat)
		resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = ToNativeFormat(_desc._format, false);
	if (renderTarget)
	{
		initialResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		memcpy(clearValue.Color, _desc._clearValue.ToPointer(), sizeof(clearValue.Color));
	}
	if (depthFormat)
	{
		initialResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		clearValue.DepthStencil.Depth = _desc._clearValue.getX();
		clearValue.DepthStencil.Stencil = static_cast<UINT8>(_desc._clearValue.getY());
		if (depthSampled)
			initialResourceState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
		&allocDesc,
		&resDesc,
		initialResourceState,
		(renderTarget || depthFormat) ? &clearValue : nullptr,
		&_resource._pMaAllocation,
		IID_PPV_ARGS(&_resource._pResource))))
		throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

	if (_desc._flags & TextureDesc::Flags::generateMips)
	{
		resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			&_uavResource._pMaAllocation,
			IID_PPV_ARGS(&_uavResource._pResource))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

		_dhUAV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, _desc._mipLevels);
		VERUS_FOR(i, _desc._mipLevels)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = resDesc.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = i;
			pRendererD3D12->GetD3DDevice()->CreateUnorderedAccessView(_uavResource._pResource.Get(), nullptr, &uavDesc, _dhUAV.AtCPU(i));
		}
	}

	if (renderTarget)
	{
		_dhRTV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
		pRendererD3D12->GetD3DDevice()->CreateRenderTargetView(_resource._pResource.Get(), nullptr, _dhRTV.AtCPU(0));
	}

	if (depthFormat)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = clearValue.Format;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		_dhDSV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		pRendererD3D12->GetD3DDevice()->CreateDepthStencilView(_resource._pResource.Get(), depthSampled ? &dsvDesc : nullptr, _dhDSV.AtCPU(0));
		if (depthSampled)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = ToNativeSampledDepthFormat(_desc._format);
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_resource._pResource.Get(), &srvDesc, _dhSRV.AtCPU(0));
		}
	}
	else
	{
		_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_resource._pResource.Get(), nullptr, _dhSRV.AtCPU(0));
	}

	if (_desc._pSamplerDesc)
		CreateSampler();

	_vStagingBuffers.reserve(_desc._mipLevels * _desc._arrayLayers);
}

void TextureD3D12::Done()
{
	_destroyStagingBuffers.Allow();
	DestroyStagingBuffers();

	_dhSampler.Reset();
	_dhDSV.Reset();
	_dhRTV.Reset();
	_dhSRV.Reset();
	VERUS_SMART_RELEASE(_uavResource._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_uavResource._pResource.Get());
	_uavResource._pResource.Reset();
	VERUS_SMART_RELEASE(_resource._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_resource._pResource.Get());
	_resource._pResource.Reset();

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
		bufferSize = Math::AlignUp(IO::DDSHeader::ComputeBcPitch(w, h, Is4BitsBC(_desc._format)), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * Math::Max(h / 4, 1);

	const int sbIndex = arrayLayer * _desc._mipLevels + mipLevel;
	if (_vStagingBuffers.size() <= sbIndex)
		_vStagingBuffers.resize(sbIndex + 1);

	auto& sb = _vStagingBuffers[sbIndex];
	D3D12MA::ALLOCATION_DESC allocDesc = {};
	allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
		&allocDesc,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		&sb._pMaAllocation,
		IID_PPV_ARGS(&sb._pResource))))
		throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD), hr=" << VERUS_HR(hr);

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	D3D12_SUBRESOURCE_DATA sd = {};
	sd.pData = p;
	if (IsBC(_desc._format))
	{
		sd.RowPitch = IO::DDSHeader::ComputeBcPitch(w, h, Is4BitsBC(_desc._format));
		sd.SlicePitch = IO::DDSHeader::ComputeBcLevelSize(w, h, Is4BitsBC(_desc._format));
	}
	else
	{
		sd.RowPitch = _bytesPerPixel * w;
		sd.SlicePitch = _bytesPerPixel * w * h;
	}
	const UINT subresource = D3D12CalcSubresource(mipLevel, arrayLayer, 0, _desc._mipLevels, _desc._arrayLayers);
	UpdateSubresources<1>(pCmdList,
		_resource._pResource.Get(),
		sb._pResource.Get(),
		0,
		subresource,
		1,
		&sd);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDst, ImageLayout::fsReadOnly, mipLevel, arrayLayer);

	_destroyStagingBuffers.Schedule();
}

void TextureD3D12::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._flags & TextureDesc::Flags::generateMips);
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();

	auto tex = TexturePtr::From(this);

	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::fsReadOnly, ImageLayout::vsReadOnly, 0);
	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::vsReadOnly, Range<int>(1, _desc._mipLevels - 1));

	pCB->BindPipeline(renderer.GetPipelineGenerateMips());

	auto shader = renderer.GetShaderGenerateMips();
	shader->BeginBindDescriptors();

	auto& ub = renderer.GetUbGenerateMips();
	ub._srgb = false;

	for (int srcMip = 0; srcMip < _desc._mipLevels - 1;)
	{
		const int srcWidth = _desc._width >> srcMip;
		const int srcHeight = _desc._height >> srcMip;
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		ub._srcDimensionCase = (srcHeight & 1) << 1 | (srcWidth & 1);

		int mipCount = 4;
		mipCount = Math::Min(4, mipCount + 1);
		mipCount = ((srcMip + mipCount) >= _desc._mipLevels) ? _desc._mipLevels - srcMip - 1 : mipCount;

		ub._srcMipLevel = srcMip;
		ub._mipLevelCount = mipCount;
		ub._texelSize.x = 1.f / dstWidth;
		ub._texelSize.y = 1.f / dstHeight;

		int mips[5] = {};
		VERUS_FOR(mip, mipCount)
			mips[mip + 1] = srcMip + mip + 1;

		const int complexDescSetID = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex }, mips);

		pCB->BindDescriptors(shader, 0, complexDescSetID);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		auto rb = CD3DX12_RESOURCE_BARRIER::UAV(_uavResource._pResource.Get());
		pCmdList->ResourceBarrier(1, &rb);

		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::vsReadOnly, ImageLayout::transferDst, Range<int>(srcMip + 1, srcMip + mipCount));
		VERUS_FOR(mip, mipCount)
		{
			const int sub = srcMip + mip + 1;
			auto rb = CD3DX12_RESOURCE_BARRIER::Transition(_uavResource._pResource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE, sub);
			pCmdList->ResourceBarrier(1, &rb);
			pCmdList->CopyTextureRegion(
				&CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), sub),
				0, 0, 0,
				&CD3DX12_TEXTURE_COPY_LOCATION(_uavResource._pResource.Get(), sub),
				nullptr);
		}
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::vsReadOnly, Range<int>(srcMip + 1, srcMip + mipCount));

		srcMip += mipCount;
	}

	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::vsReadOnly, ImageLayout::fsReadOnly, Range<int>(0, _desc._mipLevels - 1));

	shader->EndBindDescriptors();
}

void TextureD3D12::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	for (auto& x : _vStagingBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pResource.Get());
		x._pResource.Reset();
	}
	_vStagingBuffers.clear();
}

void TextureD3D12::CreateSampler()
{
	VERUS_QREF_RENDERER_D3D12;
	VERUS_QREF_CONST_SETTINGS;

	const bool tf = settings._gpuTrilinearFilter;

	D3D12_SAMPLER_DESC samplerDesc = {};

	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	if ('a' == _desc._pSamplerDesc->_filterMagMinMip[0])
	{
		if (settings._gpuAnisotropyLevel > 0)
			samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
		else
			samplerDesc.Filter = tf ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}
	else if ('s' == _desc._pSamplerDesc->_filterMagMinMip[0]) // Shadow map:
	{
		if (settings._sceneShadowQuality <= App::Settings::ShadowQuality::nearest)
			samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else
			samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
	}
	else
	{
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "nn"))
			samplerDesc.Filter = tf ? D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "nl"))
			samplerDesc.Filter = tf ? D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "ln"))
			samplerDesc.Filter = tf ? D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR : D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "ll"))
			samplerDesc.Filter = tf ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	VERUS_FOR(i, 3)
	{
		if (!_desc._pSamplerDesc->_addressModeUVW[i])
			break;
		D3D12_TEXTURE_ADDRESS_MODE tam = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		switch (_desc._pSamplerDesc->_addressModeUVW[i])
		{
		case 'r': tam = D3D12_TEXTURE_ADDRESS_MODE_WRAP; break;
		case 'm': tam = D3D12_TEXTURE_ADDRESS_MODE_MIRROR; break;
		case 'c': tam = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; break;
		case 'b': tam = D3D12_TEXTURE_ADDRESS_MODE_BORDER; break;
		}
		switch (i)
		{
		case 0: samplerDesc.AddressU = tam; break;
		case 1: samplerDesc.AddressV = tam; break;
		case 2: samplerDesc.AddressW = tam; break;
		}
	}
	samplerDesc.MipLODBias = _desc._pSamplerDesc->_mipLodBias;
	samplerDesc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	samplerDesc.MinLOD = _desc._pSamplerDesc->_minLod;
	samplerDesc.MaxLOD = _desc._pSamplerDesc->_maxLod;
	memcpy(samplerDesc.BorderColor, _desc._pSamplerDesc->_borderColor.ToPointer(), sizeof(samplerDesc.BorderColor));

	_dhSampler.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);
	pRendererD3D12->GetD3DDevice()->CreateSampler(&samplerDesc, _dhSampler.AtCPU(0));

	_desc._pSamplerDesc = nullptr;
}
