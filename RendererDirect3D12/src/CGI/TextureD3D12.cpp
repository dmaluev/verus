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
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	_initAtFrame = renderer.GetFrameCount();
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
	const bool depthSampled = _desc._flags & (TextureDesc::Flags::depthSampledR | TextureDesc::Flags::depthSampledW);
	if (_desc._flags & TextureDesc::Flags::anyShaderResource)
		_mainLayout = ImageLayout::xsReadOnly;

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

	D3D12_RESOURCE_STATES initialResourceState = ToNativeImageLayout(_mainLayout);
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = ToNativeFormat(_desc._format, false);
	memcpy(clearValue.Color, _desc._clearValue.ToPointer(), sizeof(clearValue.Color));
	if (depthFormat)
	{
		initialResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		clearValue.DepthStencil.Depth = _desc._clearValue.getX();
		clearValue.DepthStencil.Stencil = static_cast<UINT8>(_desc._clearValue.getY());
		if (_desc._flags & TextureDesc::Flags::depthSampledR)
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
		// Create storage image for compute shader's output. No need to have the first mip level.
		const int uavMipLevels = Math::Max(1, _desc._mipLevels - 1);
		D3D12_RESOURCE_DESC resDescUAV = resDesc;
		resDescUAV.Width = Math::Max(1, _desc._width >> 1);
		resDescUAV.Height = Math::Max(1, _desc._height >> 1);
		resDescUAV.MipLevels = uavMipLevels;
		resDescUAV.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		// sRGB cannot be used with UAV:
		switch (resDescUAV.Format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: resDescUAV.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: resDescUAV.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
		}

		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDescUAV,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			&_uaResource._pMaAllocation,
			IID_PPV_ARGS(&_uaResource._pResource))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

		_dhUAV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uavMipLevels);
		VERUS_FOR(i, uavMipLevels)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = resDescUAV.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = i;
			pRendererD3D12->GetD3DDevice()->CreateUnorderedAccessView(_uaResource._pResource.Get(), nullptr, &uavDesc, _dhUAV.AtCPU(i));
		}
	}

	if (_desc._readbackMip != SHRT_MAX)
	{
		if (_desc._readbackMip < 0)
			_desc._readbackMip = _desc._mipLevels + _desc._readbackMip;
		const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
		const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
		const UINT64 bufferSize = _bytesPerPixel * w * h;
		_vReadbackBuffers.resize(BaseRenderer::s_ringBufferSize);
		for (auto& x : _vReadbackBuffers)
		{
			D3D12MA::ALLOCATION_DESC allocDesc = {};
			allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
			if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
				&allocDesc,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				&x._pMaAllocation,
				IID_PPV_ARGS(&x._pResource))))
				throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_READBACK), hr=" << VERUS_HR(hr);
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
	ForceScheduled();

	for (auto& x : _vReadbackBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pResource.Get());
		x._pResource.Reset();
	}
	_vReadbackBuffers.clear();
	_dhSampler.Reset();
	_dhDSV.Reset();
	_dhRTV.Reset();
	_dhSRV.Reset();
	VERUS_SMART_RELEASE(_uaResource._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_uaResource._pResource.Get());
	_uaResource._pResource.Reset();
	VERUS_SMART_RELEASE(_resource._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_resource._pResource.Get());
	_resource._pResource.Reset();

	VERUS_DONE(TextureD3D12);
}

void TextureD3D12::UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB)
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
	if (!sb._pResource)
	{
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
	}

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), _mainLayout, ImageLayout::transferDst, mipLevel, arrayLayer);
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
	UpdateSubresources<1>(
		pCmdList,
		_resource._pResource.Get(),
		sb._pResource.Get(),
		0,
		subresource,
		1,
		&sd);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDst, _mainLayout, mipLevel, arrayLayer);

	Schedule();
}

bool TextureD3D12::ReadbackSubresource(void* p, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
	const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
	const UINT rowPitch = Math::AlignUp(_bytesPerPixel * w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	auto& rb = _vReadbackBuffers[renderer->GetRingBufferIndex()];

	// Schedule copying to readback buffer:
	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), _mainLayout, ImageLayout::transferSrc, _desc._readbackMip, 0);
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Footprint.Format = ToNativeFormat(_desc._format, false);
	footprint.Footprint.Width = w;
	footprint.Footprint.Height = h;
	footprint.Footprint.Depth = 1;
	footprint.Footprint.RowPitch = rowPitch;
	const UINT subresource = D3D12CalcSubresource(_desc._readbackMip, 0, 0, _desc._mipLevels, _desc._arrayLayers);
	pCmdList->CopyTextureRegion(
		&CD3DX12_TEXTURE_COPY_LOCATION(rb._pResource.Get(), footprint),
		0, 0, 0,
		&CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), subresource),
		nullptr);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferSrc, _mainLayout, _desc._readbackMip, 0);

	// Read current data from readback buffer:
	CD3DX12_RANGE readRange(0, 0);
	void* pData = nullptr;
	if (FAILED(hr = rb._pResource->Map(0, &readRange, &pData)))
		throw VERUS_RUNTIME_ERROR << "Map(), hr=" << VERUS_HR(hr);
	memcpy(p, pData, _bytesPerPixel * w);
	rb._pResource->Unmap(0, nullptr);

	return _initAtFrame + BaseRenderer::s_ringBufferSize < renderer.GetFrameCount();
}

void TextureD3D12::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._flags & TextureDesc::Flags::generateMips);
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	auto tex = TexturePtr::From(this);

	// Transition state for sampling in compute shader:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, _mainLayout, ImageLayout::xsReadOnly, Range<int>(0, _desc._mipLevels - 1));

	pCB->BindPipeline(renderer.GetPipelineGenerateMips(!!(_desc._flags & TextureDesc::Flags::exposureMips)));

	auto shader = renderer.GetShaderGenerateMips();
	shader->BeginBindDescriptors();
	auto& ub = renderer.GetUbGenerateMips();

	const bool createComplexSets = _vCshGenerateMips.empty();
	int dispatchIndex = 0;
	for (int srcMip = 0; srcMip < _desc._mipLevels - 1;)
	{
		const int srcWidth = _desc._width >> srcMip;
		const int srcHeight = _desc._height >> srcMip;
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		int dispatchMipCount = Math::LowestBit((dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
		dispatchMipCount = Math::Min(4, dispatchMipCount + 1);
		dispatchMipCount = ((srcMip + dispatchMipCount) >= _desc._mipLevels) ? _desc._mipLevels - srcMip - 1 : dispatchMipCount; // Edge case.

		ub._srcMipLevel = srcMip;
		ub._mipLevelCount = dispatchMipCount;
		ub._srcDimensionCase = (srcHeight & 1) << 1 | (srcWidth & 1);
		ub._srgb = IsSRGBFormat(_desc._format);
		ub._texelSize.x = 1.f / dstWidth;
		ub._texelSize.y = 1.f / dstHeight;

		if (createComplexSets)
		{
			int mips[5] = {}; // For input texture (always mip 0) and 4 UAV mips.
			VERUS_FOR(mip, dispatchMipCount)
				mips[mip + 1] = srcMip + mip;
			const CSHandle complexSetHandle = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex }, mips);
			_vCshGenerateMips.push_back(complexSetHandle);
			pCB->BindDescriptors(shader, 0, complexSetHandle);
		}
		else
		{
			pCB->BindDescriptors(shader, 0, _vCshGenerateMips[dispatchIndex]);
		}

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		auto rb = CD3DX12_RESOURCE_BARRIER::UAV(_uaResource._pResource.Get());
		pCmdList->ResourceBarrier(1, &rb);

		// Transition state for upcoming CopyTextureRegion():
		{
			CD3DX12_RESOURCE_BARRIER barriers[4];
			int barrierCount = 0;
			VERUS_FOR(mip, dispatchMipCount)
			{
				const int subUAV = srcMip + mip;
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);

			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, ImageLayout::transferDst, Range<int>(srcMip + 1, srcMip + dispatchMipCount));
		}
		VERUS_FOR(mip, dispatchMipCount)
		{
			const int subUAV = srcMip + mip;
			const int subSRV = subUAV + 1;
			pCmdList->CopyTextureRegion(
				&CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), subSRV),
				0, 0, 0,
				&CD3DX12_TEXTURE_COPY_LOCATION(_uaResource._pResource.Get(), subUAV),
				nullptr);
		}
		// Transition state for next Dispatch():
		{
			CD3DX12_RESOURCE_BARRIER barriers[4];
			int barrierCount = 0;
			VERUS_FOR(mip, dispatchMipCount)
			{
				const int subUAV = srcMip + mip;
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);

			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::xsReadOnly, Range<int>(srcMip + 1, srcMip + dispatchMipCount));
		}

		srcMip += dispatchMipCount;
		dispatchIndex++;
	}

	shader->EndBindDescriptors();

	// Revert to main state:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, _mainLayout, Range<int>(0, _desc._mipLevels - 1));

	Schedule(0);
}

Continue TextureD3D12::Scheduled_Update()
{
	if (!IsScheduledAllowed())
		return Continue::yes;

	for (auto& x : _vStagingBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pResource.Get());
		x._pResource.Reset();
	}
	_vStagingBuffers.clear();

	if (!_vCshGenerateMips.empty())
	{
		VERUS_QREF_RENDERER;
		auto shader = renderer.GetShaderGenerateMips();
		for (auto& csh : _vCshGenerateMips)
			shader->FreeDescriptorSet(csh);
		_vCshGenerateMips.clear();
	}

	return Continue::no;
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
