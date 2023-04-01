// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

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

	// Variables:
	_size = Vector4(
		float(desc._width),
		float(desc._height),
		1.f / desc._width,
		1.f / desc._height);
	if (desc._name)
		_name = desc._name;
	_desc = desc;
	_initAtFrame = renderer.GetFrameCount();
	if (desc._flags & TextureDesc::Flags::anyShaderResource)
		_mainLayout = ImageLayout::xsReadOnly;
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);

	_desc._mipLevels = _desc._mipLevels ? _desc._mipLevels : Math::ComputeMipLevels(_desc._width, _desc._height, _desc._depth);
	const bool renderTarget = (_desc._flags & TextureDesc::Flags::colorAttachment);
	const bool depthFormat = IsDepthFormat(_desc._format);
	const bool depthSampled = _desc._flags & (TextureDesc::Flags::depthSampledR | TextureDesc::Flags::depthSampledW);
	const bool cubeMap = (_desc._flags & TextureDesc::Flags::cubeMap);
	if (cubeMap)
		_desc._arrayLayers *= +CubeMapFace::count;

	// Create:
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = (_desc._depth > 1) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
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
	{
		resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (!depthSampled)
			resDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}
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
		throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT); hr=" << VERUS_HR(hr);
	_resource._pResource->SetName(_C(Str::Utf8ToWide(_name)));

	// Optional mipmap:
	if (_desc._flags & TextureDesc::Flags::generateMips)
	{
		VERUS_RT_ASSERT(_desc._mipLevels > 1);
		// Create storage image for compute shader. First mip level is not required.
		const int uaMipLevels = Math::Max(1, _desc._mipLevels - 1);
		D3D12_RESOURCE_DESC uaResDesc = resDesc;
		uaResDesc.Width = Math::Max(1, _desc._width >> 1);
		uaResDesc.Height = Math::Max(1, _desc._height >> 1);
		uaResDesc.MipLevels = uaMipLevels;
		uaResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		// sRGB cannot be used with UAV:
		uaResDesc.Format = RemoveSRGB(uaResDesc.Format);

		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&uaResDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			&_uaResource._pMaAllocation,
			IID_PPV_ARGS(&_uaResource._pResource))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT); hr=" << VERUS_HR(hr);
		_uaResource._pResource->SetName(_C(Str::Utf8ToWide(_name + " (UAV)")));

		_vCshGenerateMips.reserve(Math::DivideByMultiple<int>(_desc._mipLevels, 4));
		_dhUAV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, uaMipLevels * _desc._arrayLayers);
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = uaResDesc.Format;
		uavDesc.ViewDimension = (_desc._arrayLayers > 1) ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY : D3D12_UAV_DIMENSION_TEXTURE2D;
		VERUS_FOR(layer, _desc._arrayLayers)
		{
			VERUS_FOR(mip, uaMipLevels)
			{
				if (D3D12_UAV_DIMENSION_TEXTURE2D == uavDesc.ViewDimension)
				{
					uavDesc.Texture2D.MipSlice = mip;
					pRendererD3D12->GetD3DDevice()->CreateUnorderedAccessView(_uaResource._pResource.Get(), nullptr, &uavDesc, _dhUAV.AtCPU(mip));
				}
				else
				{
					uavDesc.Texture2DArray.MipSlice = mip;
					uavDesc.Texture2DArray.FirstArraySlice = layer;
					uavDesc.Texture2DArray.ArraySize = 1;
					if (cubeMap)
					{
						const int cubeIndex = layer / +CubeMapFace::count;
						const int faceIndex = layer % +CubeMapFace::count;
						uavDesc.Texture2DArray.FirstArraySlice = (cubeIndex * +CubeMapFace::count) + ToNativeCubeMapFace(static_cast<CubeMapFace>(faceIndex));
					}
					pRendererD3D12->GetD3DDevice()->CreateUnorderedAccessView(_uaResource._pResource.Get(), nullptr, &uavDesc, _dhUAV.AtCPU(mip + layer * uaMipLevels));
				}
			}
		}
	}

	// Optional readback buffer:
	if (_desc._readbackMip != SHRT_MAX)
	{
		if (_desc._readbackMip < 0)
			_desc._readbackMip = _desc._mipLevels + _desc._readbackMip;
		const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
		const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
		const UINT64 bufferSize = _bytesPerPixel * w * h;
		_vReadbackBuffers.resize(BaseRenderer::s_ringBufferSize);
		for (auto& readbackBuffer : _vReadbackBuffers)
		{
			D3D12MA::ALLOCATION_DESC allocDesc = {};
			allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
			const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
				&allocDesc,
				&resDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				&readbackBuffer._pMaAllocation,
				IID_PPV_ARGS(&readbackBuffer._pResource))))
				throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_READBACK); hr=" << VERUS_HR(hr);
			readbackBuffer._pResource->SetName(_C(Str::Utf8ToWide(_name + " (Readback)")));
		}
	}

	// Create views:
	if (renderTarget)
	{
		if (cubeMap)
		{
			_dhRTV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, +CubeMapFace::count);
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = ToNativeFormat(_desc._format, false);
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			VERUS_FOR(i, +CubeMapFace::count)
			{
				rtvDesc.Texture2DArray.FirstArraySlice = ToNativeCubeMapFace(static_cast<CubeMapFace>(i));
				rtvDesc.Texture2DArray.ArraySize = 1;
				pRendererD3D12->GetD3DDevice()->CreateRenderTargetView(_resource._pResource.Get(), &rtvDesc, _dhRTV.AtCPU(i));
			}
		}
		else
		{
			_dhRTV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1);
			pRendererD3D12->GetD3DDevice()->CreateRenderTargetView(_resource._pResource.Get(), nullptr, _dhRTV.AtCPU(0));
		}
	}
	if (depthFormat)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = clearValue.Format;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		_dhDSV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
		pRendererD3D12->GetD3DDevice()->CreateDepthStencilView(_resource._pResource.Get(), &dsvDesc, _dhDSV.AtCPU(0));
		if (depthSampled)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = ToNativeSampledDepthFormat(_desc._format);
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_resource._pResource.Get(), &srvDesc, _dhSRV.AtCPU(0));
		}
	}
	else
	{
		if (cubeMap)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = ToNativeFormat(_desc._format, false);
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			if (_desc._arrayLayers > +CubeMapFace::count)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				srvDesc.TextureCubeArray.MipLevels = _desc._mipLevels;
				srvDesc.TextureCubeArray.NumCubes = _desc._arrayLayers / +CubeMapFace::count;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = _desc._mipLevels;
			}
			_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_resource._pResource.Get(), &srvDesc, _dhSRV.AtCPU(0));
		}
		else
		{
			_dhSRV.Create(pRendererD3D12->GetD3DDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			pRendererD3D12->GetD3DDevice()->CreateShaderResourceView(_resource._pResource.Get(), nullptr, _dhSRV.AtCPU(0));
		}
	}

	// Custom sampler:
	if (_desc._pSamplerDesc)
		CreateSampler();

	_vStagingBuffers.reserve(_desc._mipLevels * _desc._arrayLayers);
}

void TextureD3D12::Done()
{
	ForceScheduled();

	_dhSampler.Reset();
	_dhDSV.Reset();
	_dhRTV.Reset();
	_dhUAV.Reset();
	_dhSRV.Reset();

	_vCshGenerateMips.clear();

	for (auto& x : _vReadbackBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pResource.Get());
		x._pResource.Reset();
	}
	_vReadbackBuffers.clear();

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
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&sb._pMaAllocation,
			IID_PPV_ARGS(&sb._pResource))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
		sb._pResource->SetName(_C(Str::Utf8ToWide(_name + " (Staging)")));
	}

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
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
		sb._pResource.Get(), 0,
		subresource, 1, &sd);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDst, _mainLayout, mipLevel, arrayLayer);

	Schedule();
}

bool TextureD3D12::ReadbackSubresource(void* p, bool recordCopyCommand, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
	const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
	const UINT rowPitch = Math::AlignUp(_bytesPerPixel * w, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	auto& rb = _vReadbackBuffers[renderer->GetRingBufferIndex()];

	if (recordCopyCommand)
	{
		if (!pCB)
			pCB = renderer.GetCommandBuffer().Get();
		auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
		pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), _mainLayout, ImageLayout::transferSrc, _desc._readbackMip, 0);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		footprint.Footprint.Format = ToNativeFormat(_desc._format, false);
		footprint.Footprint.Width = w;
		footprint.Footprint.Height = h;
		footprint.Footprint.Depth = 1;
		footprint.Footprint.RowPitch = rowPitch;
		const UINT subresource = D3D12CalcSubresource(_desc._readbackMip, 0, 0, _desc._mipLevels, _desc._arrayLayers);
		const auto dstCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(rb._pResource.Get(), footprint);
		const auto srcCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), subresource);
		pCmdList->CopyTextureRegion(
			&dstCopyLoc,
			0, 0, 0,
			&srcCopyLoc,
			nullptr);
		pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferSrc, _mainLayout, _desc._readbackMip, 0);
	}

	if (p) // Read current data from readback buffer:
	{
		CD3DX12_RANGE readRange(0, 0);
		void* pData = nullptr;
		if (FAILED(hr = rb._pResource->Map(0, &readRange, &pData)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pDst = static_cast<BYTE*>(p);
		BYTE* pSrc = static_cast<BYTE*>(pData);
		VERUS_FOR(i, h)
		{
			memcpy(
				pDst + i * _bytesPerPixel * w,
				pSrc + i * rowPitch,
				_bytesPerPixel * w);
		}
		rb._pResource->Unmap(0, nullptr);
	}

	return _initAtFrame + BaseRenderer::s_ringBufferSize < renderer.GetFrameCount();
}

void TextureD3D12::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._flags & TextureDesc::Flags::generateMips);
	if (_desc._flags & TextureDesc::Flags::cubeMap)
		return GenerateCubeMapMips(pCB);

	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	auto shader = renderer.GetShaderGenerateMips();
	auto& ub = renderer.GetUbGenerateMips();
	auto tex = TexturePtr::From(this);

	const int maxMipLevel = _desc._mipLevels - 1;

	// Transition state for sampling in compute shader:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, _mainLayout, ImageLayout::xsReadOnly, Range(0, _desc._mipLevels));

	pCB->BindPipeline(renderer.GetPipelineGenerateMips(!!(_desc._flags & TextureDesc::Flags::exposureMips)));

	shader->BeginBindDescriptors();
	int dispatchIndex = 0;
	for (int srcMip = 0; srcMip < maxMipLevel;)
	{
		const int srcWidth = Math::Max(1, _desc._width >> srcMip);
		const int srcHeight = Math::Max(1, _desc._height >> srcMip);
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		int dispatchMipCount = Math::LowestBit((dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
		dispatchMipCount = Math::Min(4, dispatchMipCount + 1);
		dispatchMipCount = ((srcMip + dispatchMipCount) >= _desc._mipLevels) ? _desc._mipLevels - srcMip - 1 : dispatchMipCount; // Edge case.

		ub._srcMipLevel = srcMip;
		ub._mipLevelCount = dispatchMipCount;
		ub._srcDimensionCase = ((srcHeight & 1) << 1) | (srcWidth & 1);
		ub._srgb = IsSRGBFormat(_desc._format);
		ub._dstTexelSize.x = 1.f / dstWidth;
		ub._dstTexelSize.y = 1.f / dstHeight;

		int mipLevels[5] = {}; // For input texture (always mip 0) and 4 UAV mips.
		VERUS_FOR(i, dispatchMipCount)
			mipLevels[i + 1] = srcMip + i;
		const CSHandle complexSetHandle = shader->BindDescriptorSetTextures(0 | ShaderD3D12::SET_MOD_TO_VIEW_HEAP, { tex, tex, tex, tex, tex }, mipLevels);
		_vCshGenerateMips.push_back(complexSetHandle);
		pCB->BindDescriptors(shader, 0, complexSetHandle);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		auto rb = CD3DX12_RESOURCE_BARRIER::UAV(_uaResource._pResource.Get());
		pCmdList->ResourceBarrier(1, &rb);

		// Transition state for upcoming CopyTextureRegion():
		{
			CD3DX12_RESOURCE_BARRIER barriers[4];
			int barrierCount = 0;
			VERUS_FOR(i, dispatchMipCount)
			{
				const int subUAV = srcMip + i;
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);
			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, ImageLayout::transferDst, Range(0, dispatchMipCount).OffsetBy(srcMip + 1));
		}
		VERUS_FOR(i, dispatchMipCount)
		{
			const int subUAV = srcMip + i;
			const int subSRV = subUAV + 1;
			const auto dstCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), subSRV);
			const auto srcCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(_uaResource._pResource.Get(), subUAV);
			pCmdList->CopyTextureRegion(
				&dstCopyLoc,
				0, 0, 0,
				&srcCopyLoc,
				nullptr);
		}
		// Transition state for next Dispatch():
		{
			CD3DX12_RESOURCE_BARRIER barriers[4];
			int barrierCount = 0;
			VERUS_FOR(i, dispatchMipCount)
			{
				const int subUAV = srcMip + i;
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);
			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::xsReadOnly, Range(0, dispatchMipCount).OffsetBy(srcMip + 1));
		}

		srcMip += dispatchMipCount;
		dispatchIndex++;
	}
	shader->EndBindDescriptors();

	// Revert to main state:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, _mainLayout, Range(0, _desc._mipLevels));

	ClearCshGenerateMips();
	Schedule(0);
}

void TextureD3D12::GenerateCubeMapMips(PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	auto shader = renderer.GetShaderGenerateCubeMapMips();
	auto& ub = renderer.GetUbGenerateCubeMapMips();
	auto tex = TexturePtr::From(this);

	const int maxMipLevel = _desc._mipLevels - 1;

	// Transition state for sampling in compute shader:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, _mainLayout, ImageLayout::xsReadOnly, Range(0, _desc._mipLevels), Range(0, +CubeMapFace::count));

	pCB->BindPipeline(renderer.GetPipelineGenerateCubeMapMips());

	shader->BeginBindDescriptors();
	int dispatchIndex = 0;
	for (int srcMip = 0; srcMip < maxMipLevel;)
	{
		const int srcWidth = Math::Max(1, _desc._width >> srcMip);
		const int srcHeight = Math::Max(1, _desc._height >> srcMip);
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		ub._srcMipLevel = srcMip;
		ub._srgb = IsSRGBFormat(_desc._format);
		ub._dstTexelSize.x = 1.f / dstWidth;
		ub._dstTexelSize.y = 1.f / dstHeight;

		int mipLevels[7] = {};
		VERUS_FOR(i, +CubeMapFace::count)
			mipLevels[i + 1] = srcMip;
		int arrayLayers[7] = {};
		VERUS_FOR(i, +CubeMapFace::count)
			arrayLayers[i + 1] = ToNativeCubeMapFace(static_cast<CubeMapFace>(i));
		const CSHandle complexSetHandle = shader->BindDescriptorSetTextures(0 | ShaderD3D12::SET_MOD_TO_VIEW_HEAP, { tex, tex, tex, tex, tex, tex, tex }, mipLevels, arrayLayers);
		_vCshGenerateMips.push_back(complexSetHandle);
		pCB->BindDescriptors(shader, 0, complexSetHandle);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		auto rb = CD3DX12_RESOURCE_BARRIER::UAV(_uaResource._pResource.Get());
		pCmdList->ResourceBarrier(1, &rb);

		// Transition state for upcoming CopyTextureRegion():
		{
			CD3DX12_RESOURCE_BARRIER barriers[+CubeMapFace::count];
			int barrierCount = 0;
			VERUS_FOR(i, +CubeMapFace::count)
			{
				const int subUAV = D3D12CalcSubresource(srcMip, i, 0, _desc._mipLevels - 1, _desc._arrayLayers);
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);
			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, ImageLayout::transferDst, srcMip + 1, Range(0, +CubeMapFace::count));
		}

		VERUS_FOR(i, +CubeMapFace::count)
		{
			const int subUAV = D3D12CalcSubresource(srcMip, i, 0, _desc._mipLevels - 1, _desc._arrayLayers);
			const int subSRV = D3D12CalcSubresource(srcMip + 1, i, 0, _desc._mipLevels, _desc._arrayLayers);
			const auto dstCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(_resource._pResource.Get(), subSRV);
			const auto srcCopyLoc = CD3DX12_TEXTURE_COPY_LOCATION(_uaResource._pResource.Get(), subUAV);
			pCmdList->CopyTextureRegion(
				&dstCopyLoc,
				0, 0, 0,
				&srcCopyLoc,
				nullptr);
		}

		// Transition state for next Dispatch():
		{
			CD3DX12_RESOURCE_BARRIER barriers[+CubeMapFace::count];
			int barrierCount = 0;
			VERUS_FOR(i, +CubeMapFace::count)
			{
				const int subUAV = D3D12CalcSubresource(srcMip, i, 0, _desc._mipLevels - 1, _desc._arrayLayers);
				barriers[barrierCount++] = CD3DX12_RESOURCE_BARRIER::Transition(_uaResource._pResource.Get(),
					D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, subUAV);
			}
			pCmdList->ResourceBarrier(barrierCount, barriers);
			pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::xsReadOnly, srcMip + 1, Range(0, +CubeMapFace::count));
		}

		srcMip++;
		dispatchIndex++;
	}
	shader->EndBindDescriptors();

	// Revert to main state:
	if (_mainLayout != ImageLayout::xsReadOnly)
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::xsReadOnly, _mainLayout, Range(0, _desc._mipLevels), Range(0, +CubeMapFace::count));

	ClearCshGenerateMips();
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

	ClearCshGenerateMips();

	return Continue::no;
}

void TextureD3D12::ClearCshGenerateMips()
{
	if (!_vCshGenerateMips.empty())
	{
		VERUS_QREF_RENDERER;
		auto shader = (_desc._flags & TextureDesc::Flags::cubeMap) ? renderer.GetShaderGenerateCubeMapMips() : renderer.GetShaderGenerateMips();
		for (auto& csh : _vCshGenerateMips)
			shader->FreeDescriptorSet(csh);
		_vCshGenerateMips.clear();
	}
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
		if (settings._sceneShadowQuality <= App::Settings::Quality::low)
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

DXGI_FORMAT TextureD3D12::RemoveSRGB(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	return format;
}
