// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

TextureD3D11::TextureD3D11()
{
}

TextureD3D11::~TextureD3D11()
{
	Done();
}

void TextureD3D11::Init(RcTextureDesc desc)
{
	VERUS_INIT();
	VERUS_RT_ASSERT(desc._width > 0 && desc._height > 0);
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D11;
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
	D3D11_TEXTURE2D_DESC tex2DDesc = {};
	tex2DDesc.Width = _desc._width;
	tex2DDesc.Height = _desc._height;
	tex2DDesc.MipLevels = _desc._mipLevels;
	tex2DDesc.ArraySize = _desc._arrayLayers;
	tex2DDesc.Format = ToNativeFormat(_desc._format, depthSampled);
	tex2DDesc.SampleDesc.Count = _desc._sampleCount;
	tex2DDesc.Usage = D3D11_USAGE_DEFAULT;
	tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex2DDesc.CPUAccessFlags = 0;
	tex2DDesc.MiscFlags = 0;
	if (renderTarget)
		tex2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	if (depthFormat)
	{
		tex2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if (depthSampled)
			tex2DDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}
	if (cubeMap)
		tex2DDesc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

	D3D11_TEXTURE3D_DESC tex3DDesc = {};
	tex3DDesc.Width = tex2DDesc.Width;
	tex3DDesc.Height = tex2DDesc.Height;
	tex3DDesc.Depth = _desc._depth;
	tex3DDesc.MipLevels = tex2DDesc.MipLevels;
	tex3DDesc.Format = tex2DDesc.Format;
	tex3DDesc.Usage = tex2DDesc.Usage;
	tex3DDesc.BindFlags = tex2DDesc.BindFlags;
	tex3DDesc.CPUAccessFlags = tex2DDesc.CPUAccessFlags;
	tex3DDesc.MiscFlags = tex2DDesc.MiscFlags;

	if (_desc._depth > 1)
	{
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateTexture3D(&tex3DDesc, nullptr, &_pTexture3D)))
			throw VERUS_RUNTIME_ERROR << "CreateTexture3D(); hr=" << VERUS_HR(hr);
		SetDebugObjectName(_pTexture3D.Get(), _C(_name));
	}
	else
	{
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateTexture2D(&tex2DDesc, nullptr, &_pTexture2D)))
			throw VERUS_RUNTIME_ERROR << "CreateTexture2D(); hr=" << VERUS_HR(hr);
		SetDebugObjectName(_pTexture2D.Get(), _C(_name));
	}

	// Optional mipmap:
	if (_desc._flags & TextureDesc::Flags::generateMips)
	{
		VERUS_RT_ASSERT(_desc._mipLevels > 1);
		// Create storage image for compute shader. First mip level is not required.
		const int uaMipLevels = Math::Max(1, _desc._mipLevels - 1);
		D3D11_TEXTURE2D_DESC uaTexDesc = tex2DDesc;
		uaTexDesc.Width = Math::Max(1, _desc._width >> 1);
		uaTexDesc.Height = Math::Max(1, _desc._height >> 1);
		uaTexDesc.MipLevels = uaMipLevels;
		uaTexDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		// sRGB cannot be used with UAV:
		uaTexDesc.Format = RemoveSRGB(uaTexDesc.Format);

		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateTexture2D(&uaTexDesc, nullptr, &_pUaTexture)))
			throw VERUS_RUNTIME_ERROR << "CreateTexture2D(UAV); hr=" << VERUS_HR(hr);
		SetDebugObjectName(_pUaTexture.Get(), _C(_name + " (UAV)"));

		_vCshGenerateMips.reserve(Math::DivideByMultiple<int>(_desc._mipLevels, 4));
		_vUAVs.resize(uaMipLevels * _desc._arrayLayers);
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = uaTexDesc.Format;
		uavDesc.ViewDimension = (_desc._arrayLayers > 1) ? D3D11_UAV_DIMENSION_TEXTURE2DARRAY : D3D11_UAV_DIMENSION_TEXTURE2D;
		VERUS_FOR(layer, _desc._arrayLayers)
		{
			VERUS_FOR(mip, uaMipLevels)
			{
				if (D3D11_UAV_DIMENSION_TEXTURE2D == uavDesc.ViewDimension)
				{
					uavDesc.Texture2D.MipSlice = mip;
					if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateUnorderedAccessView(_pUaTexture.Get(), &uavDesc, &_vUAVs[mip])))
						throw VERUS_RUNTIME_ERROR << "CreateUnorderedAccessView(TEXTURE2D); hr=" << VERUS_HR(hr);
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
					if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateUnorderedAccessView(_pUaTexture.Get(), &uavDesc, &_vUAVs[mip + layer * uaMipLevels])))
						throw VERUS_RUNTIME_ERROR << "CreateUnorderedAccessView(TEXTURE2DARRAY); hr=" << VERUS_HR(hr);
				}
			}
		}
	}

	// Optional readback texture:
	if (_desc._readbackMip != SHRT_MAX)
	{
		if (_desc._readbackMip < 0)
			_desc._readbackMip = _desc._mipLevels + _desc._readbackMip;
		const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
		const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
		_vReadbackTextures.resize(BaseRenderer::s_ringBufferSize);
		for (auto& readbackTexture : _vReadbackTextures)
		{
			D3D11_TEXTURE2D_DESC texDesc = {};
			texDesc.Width = w;
			texDesc.Height = h;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 1;
			texDesc.Format = ToNativeFormat(_desc._format, false);
			texDesc.SampleDesc.Count = 1;
			texDesc.Usage = D3D11_USAGE_STAGING;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateTexture2D(&texDesc, nullptr, &readbackTexture)))
				throw VERUS_RUNTIME_ERROR << "CreateTexture2D(Readback); hr=" << VERUS_HR(hr);
			SetDebugObjectName(readbackTexture.Get(), _C(_name + " (Readback)"));
		}
	}

	// Create views:
	if (renderTarget)
	{
		if (cubeMap)
		{
			_vRTVs.resize(+CubeMapFace::count);
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = ToNativeFormat(_desc._format, false);
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			VERUS_FOR(i, +CubeMapFace::count)
			{
				rtvDesc.Texture2DArray.FirstArraySlice = ToNativeCubeMapFace(static_cast<CubeMapFace>(i));
				rtvDesc.Texture2DArray.ArraySize = 1;
				if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateRenderTargetView(GetD3DResource(), &rtvDesc, &_vRTVs[i])))
					throw VERUS_RUNTIME_ERROR << "CreateRenderTargetView(cubeMap); hr=" << VERUS_HR(hr);
			}
		}
		else
		{
			_vRTVs.resize(1);
			if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateRenderTargetView(GetD3DResource(), nullptr, &_vRTVs[0])))
				throw VERUS_RUNTIME_ERROR << "CreateRenderTargetView(); hr=" << VERUS_HR(hr);
		}
	}
	if (depthFormat)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = ToNativeFormat(_desc._format, false);
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateDepthStencilView(GetD3DResource(), &dsvDesc, &_pDSV[0])))
			throw VERUS_RUNTIME_ERROR << "CreateDepthStencilView(); hr=" << VERUS_HR(hr);
		dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateDepthStencilView(GetD3DResource(), &dsvDesc, &_pDSV[1])))
			throw VERUS_RUNTIME_ERROR << "CreateDepthStencilView(READ_ONLY); hr=" << VERUS_HR(hr);
		if (depthSampled)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = ToNativeSampledDepthFormat(_desc._format);
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateShaderResourceView(GetD3DResource(), &srvDesc, &_pSRV)))
				throw VERUS_RUNTIME_ERROR << "CreateShaderResourceView(depthSampled); hr=" << VERUS_HR(hr);
		}
	}
	else
	{
		if (cubeMap)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = ToNativeFormat(_desc._format, false);
			if (_desc._arrayLayers > +CubeMapFace::count)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				srvDesc.TextureCubeArray.MipLevels = _desc._mipLevels;
				srvDesc.TextureCubeArray.NumCubes = _desc._arrayLayers / +CubeMapFace::count;
			}
			else
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = _desc._mipLevels;
			}
			if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateShaderResourceView(GetD3DResource(), &srvDesc, &_pSRV)))
				throw VERUS_RUNTIME_ERROR << "CreateShaderResourceView(cubeMap); hr=" << VERUS_HR(hr);
		}
		else
		{
			if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateShaderResourceView(GetD3DResource(), nullptr, &_pSRV)))
				throw VERUS_RUNTIME_ERROR << "CreateShaderResourceView(); hr=" << VERUS_HR(hr);
		}
	}

	// Custom sampler:
	if (_desc._pSamplerDesc)
		CreateSampler();
}

void TextureD3D11::Done()
{
	ForceScheduled();

	_pSamplerState.Reset();

	_pDSV[1].Reset();
	_pDSV[0].Reset();

	for (auto& x : _vRTVs)
		x.Reset();
	_vRTVs.clear();

	for (auto& x : _vUAVs)
		x.Reset();
	_vUAVs.clear();

	_pSRV.Reset();

	_vCshGenerateMips.clear();

	for (auto& x : _vReadbackTextures)
		x.Reset();
	_vReadbackTextures.clear();

	_pUaTexture.Reset();
	_pTexture3D.Reset();
	_pTexture2D.Reset();

	VERUS_DONE(TextureD3D11);
}

void TextureD3D11::UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	const int w = Math::Max(1, _desc._width >> mipLevel);
	const int h = Math::Max(1, _desc._height >> mipLevel);

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();

	UINT rowPitch;
	UINT depthPitch;
	if (IsBC(_desc._format))
	{
		rowPitch = IO::DDSHeader::ComputeBcPitch(w, h, Is4BitsBC(_desc._format));
		depthPitch = IO::DDSHeader::ComputeBcLevelSize(w, h, Is4BitsBC(_desc._format));
	}
	else
	{
		rowPitch = _bytesPerPixel * w;
		depthPitch = _bytesPerPixel * w * h;
	}
	const UINT subresource = D3D11CalcSubresource(mipLevel, arrayLayer, _desc._mipLevels);
	pDeviceContext->UpdateSubresource(
		_pTexture2D.Get(), subresource, nullptr,
		p, rowPitch, depthPitch);
}

bool TextureD3D11::ReadbackSubresource(void* p, bool recordCopyCommand, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();

	const int w = Math::Max(1, _desc._width >> _desc._readbackMip);
	const int h = Math::Max(1, _desc._height >> _desc._readbackMip);
	const UINT rowPitch = _bytesPerPixel * w;

	auto& rt = _vReadbackTextures[renderer->GetRingBufferIndex()];

	if (recordCopyCommand)
	{
		const UINT subresource = D3D11CalcSubresource(_desc._readbackMip, 0, _desc._mipLevels);
		pDeviceContext->CopySubresourceRegion(
			rt.Get(), 0,
			0, 0, 0,
			GetD3DResource(), subresource,
			nullptr);
	}

	if (p) // Read current data from readback texture:
	{
		D3D11_MAPPED_SUBRESOURCE ms = {};
		if (FAILED(hr = pDeviceContext->Map(rt.Get(), 0, D3D11_MAP_READ, 0, &ms)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pDst = static_cast<BYTE*>(p);
		BYTE* pSrc = static_cast<BYTE*>(ms.pData);
		VERUS_FOR(i, h)
		{
			memcpy(
				pDst + i * _bytesPerPixel * w,
				pSrc + i * rowPitch,
				_bytesPerPixel * w);
		}
		pDeviceContext->Unmap(rt.Get(), 0);
	}

	return _initAtFrame + BaseRenderer::s_ringBufferSize < renderer.GetFrameCount();
}

void TextureD3D11::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._flags & TextureDesc::Flags::generateMips);
	if (_desc._flags & TextureDesc::Flags::cubeMap)
		return GenerateCubeMapMips(pCB);

	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();
	auto shader = renderer.GetShaderGenerateMips();
	auto& ub = renderer.GetUbGenerateMips();
	auto tex = TexturePtr::From(this);

	const int maxMipLevel = _desc._mipLevels - 1;

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

		int mipLevels[5] = { 0, -1, -1, -1, -1 }; // For input texture (always mip 0) and 4 UAV mips.
		VERUS_FOR(i, dispatchMipCount)
			mipLevels[i + 1] = srcMip + i;
		const CSHandle complexSetHandle = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex }, mipLevels);
		_vCshGenerateMips.push_back(complexSetHandle);
		pCB->BindDescriptors(shader, 0, complexSetHandle);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		VERUS_FOR(i, dispatchMipCount)
		{
			const int subUAV = srcMip + i;
			const int subSRV = subUAV + 1;
			pDeviceContext->CopySubresourceRegion(
				GetD3DResource(), subSRV,
				0, 0, 0,
				_pUaTexture.Get(), subUAV,
				nullptr);
		}

		srcMip += dispatchMipCount;
		dispatchIndex++;
	}
	shader->EndBindDescriptors();

	ClearCshGenerateMips();
}

void TextureD3D11::GenerateCubeMapMips(PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();
	auto shader = renderer.GetShaderGenerateCubeMapMips();
	auto& ub = renderer.GetUbGenerateCubeMapMips();
	auto tex = TexturePtr::From(this);

	const int maxMipLevel = _desc._mipLevels - 1;

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
		const CSHandle complexSetHandle = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex, tex, tex }, mipLevels, arrayLayers);
		_vCshGenerateMips.push_back(complexSetHandle);
		pCB->BindDescriptors(shader, 0, complexSetHandle);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		VERUS_FOR(i, +CubeMapFace::count)
		{
			const int subUAV = D3D11CalcSubresource(srcMip, i, _desc._mipLevels - 1);
			const int subSRV = D3D11CalcSubresource(srcMip + 1, i, _desc._mipLevels);
			pDeviceContext->CopySubresourceRegion(
				GetD3DResource(), subSRV,
				0, 0, 0,
				_pUaTexture.Get(), subUAV,
				nullptr);
		}

		srcMip++;
		dispatchIndex++;
	}
	shader->EndBindDescriptors();

	ClearCshGenerateMips();
}

Continue TextureD3D11::Scheduled_Update()
{
	return Continue::yes;
}

void TextureD3D11::ClearCshGenerateMips()
{
	if (!_vCshGenerateMips.empty())
	{
		VERUS_QREF_RENDERER;
		auto shader = renderer.GetShaderGenerateMips();
		for (auto& csh : _vCshGenerateMips)
			shader->FreeDescriptorSet(csh);
		_vCshGenerateMips.clear();
	}
}

void TextureD3D11::CreateSampler()
{
	VERUS_QREF_RENDERER_D3D11;
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	const bool tf = settings._gpuTrilinearFilter;

	D3D11_SAMPLER_DESC samplerDesc = {};

	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	if ('a' == _desc._pSamplerDesc->_filterMagMinMip[0])
	{
		if (settings._gpuAnisotropyLevel > 0)
			samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		else
			samplerDesc.Filter = tf ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}
	else if ('s' == _desc._pSamplerDesc->_filterMagMinMip[0]) // Shadow map:
	{
		if (settings._sceneShadowQuality <= App::Settings::Quality::low)
			samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else
			samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	}
	else
	{
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "nn"))
			samplerDesc.Filter = tf ? D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "nl"))
			samplerDesc.Filter = tf ? D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR : D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "ln"))
			samplerDesc.Filter = tf ? D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR : D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		if (!strcmp(_desc._pSamplerDesc->_filterMagMinMip, "ll"))
			samplerDesc.Filter = tf ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	VERUS_FOR(i, 3)
	{
		if (!_desc._pSamplerDesc->_addressModeUVW[i])
			break;
		D3D11_TEXTURE_ADDRESS_MODE tam = D3D11_TEXTURE_ADDRESS_CLAMP;
		switch (_desc._pSamplerDesc->_addressModeUVW[i])
		{
		case 'r': tam = D3D11_TEXTURE_ADDRESS_WRAP; break;
		case 'm': tam = D3D11_TEXTURE_ADDRESS_MIRROR; break;
		case 'c': tam = D3D11_TEXTURE_ADDRESS_CLAMP; break;
		case 'b': tam = D3D11_TEXTURE_ADDRESS_BORDER; break;
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

	if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateSamplerState(&samplerDesc, &_pSamplerState)))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	_desc._pSamplerDesc = nullptr;
}

ID3D11Resource* TextureD3D11::GetD3DResource() const
{
	if (_pTexture2D)
		return _pTexture2D.Get();
	else
		return _pTexture3D.Get();
}

DXGI_FORMAT TextureD3D11::RemoveSRGB(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
	return format;
}
