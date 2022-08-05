// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

static XrResult GetD3D11GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D11KHR* pGraphicsRequirements)
{
	PFN_xrGetD3D11GraphicsRequirementsKHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, systemId, pGraphicsRequirements) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

// ExtRealityD3D11:

ExtRealityD3D11::ExtRealityD3D11()
{
	VERUS_ZERO_MEM(_adapterLuid);
}

ExtRealityD3D11::~ExtRealityD3D11()
{
	Done();
}

void ExtRealityD3D11::Init()
{
	VERUS_INIT();

	_vRequiredExtensions.reserve(2);
	_vRequiredExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	_vRequiredExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	if (!CheckRequiredExtensions())
		throw VERUS_RUNTIME_ERROR << "CheckRequiredExtensions()";

	CreateInstance();
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	CreateDebugUtilsMessenger();
#endif
	GetSystem();
}

void ExtRealityD3D11::Done()
{
	for (auto& swapChainEx : _vSwapChains)
	{
		swapChainEx._vImageViews.clear();
		VERUS_XR_DESTROY(swapChainEx._handle, xrDestroySwapchain(swapChainEx._handle));
	}
	_vSwapChains.clear();

	VERUS_DONE(ExtRealityD3D11);
}

void ExtRealityD3D11::InitByRenderer(PRendererD3D11 pRenderer)
{
	VERUS_QREF_CONST_SETTINGS;

	XrResult res = XR_SUCCESS;
	XrGraphicsBindingD3D11KHR xrgb = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	xrgb.device = pRenderer->GetD3DDevice();
	XrSessionCreateInfo xrsci = { XR_TYPE_SESSION_CREATE_INFO };
	xrsci.next = &xrgb;
	xrsci.systemId = _systemId;
	if (XR_SUCCESS != (res = xrCreateSession(_instance, &xrsci, &_session)))
		throw VERUS_RUNTIME_ERROR << "xrCreateSession(); res=" << res;

	CreateReferenceSpace();
	CreateHeadSpace();
	CreateSwapChains(pRenderer->GetD3DDevice(), ToNativeFormat(Renderer::GetSwapChainFormat(), false));

	const RP::Attachment::LoadOp colorLoadOp = settings._displayOffscreenDraw ? RP::Attachment::LoadOp::dontCare : RP::Attachment::LoadOp::clear;
	_rph = pRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Renderer::GetSwapChainFormat()).SetLoadOp(colorLoadOp).Layout(ImageLayout::undefined, ImageLayout::colorAttachment)
		},
		{
			RP::Subpass("Sp0").Color({RP::Ref("Color", ImageLayout::colorAttachment)})
		},
		{});
	_fbh.resize(_vSwapChains.size() * _swapChainBufferCount);
	VERUS_FOR(swapChainIndex, _vSwapChains.size())
	{
		VERUS_FOR(bufferIndex, _swapChainBufferCount)
		{
			const int swapChainBufferIndex = (+ViewType::openXR << 16) | (swapChainIndex << 8) | bufferIndex;
			_fbh[swapChainIndex * _swapChainBufferCount + bufferIndex] = pRenderer->CreateFramebuffer(_rph, {},
				_vSwapChains[swapChainIndex]._width, _vSwapChains[swapChainIndex]._height, swapChainBufferIndex);
		}
	}
}

void ExtRealityD3D11::GetSystem()
{
	XrResult res = XR_SUCCESS;

	XrSystemGetInfo xrsgi = { XR_TYPE_SYSTEM_GET_INFO };
	xrsgi.formFactor = _formFactor;
	if (XR_SUCCESS != (res = xrGetSystem(_instance, &xrsgi, &_systemId)))
		throw VERUS_RUNTIME_ERROR << "xrGetSystem(); res=" << res;

	uint32_t blendModeCount = 0;
	if (XR_SUCCESS != (res = xrEnumerateEnvironmentBlendModes(_instance, _systemId, _viewConfigurationType,
		1, &blendModeCount, &_envBlendMode)))
		throw VERUS_RUNTIME_ERROR << "xrEnumerateEnvironmentBlendModes(); res=" << res;

	XrGraphicsRequirementsD3D11KHR xrgr = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	if (XR_SUCCESS != (res = GetD3D11GraphicsRequirementsKHR(_instance, _systemId, &xrgr)))
		throw VERUS_RUNTIME_ERROR << "GetD3D11GraphicsRequirementsKHR(); res=" << res;

	_adapterLuid = xrgr.adapterLuid;
	_minFeatureLevel = xrgr.minFeatureLevel;
}

void ExtRealityD3D11::CreateSwapChains(ID3D11Device* pDevice, int64_t format)
{
	XrResult res = XR_SUCCESS;

	uint32_t viewCount = 0;
	xrEnumerateViewConfigurationViews(_instance, _systemId, _viewConfigurationType, 0, &viewCount, nullptr);
	Vector<XrViewConfigurationView> vViewConfigurationViews(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	_vViews.resize(viewCount, { XR_TYPE_VIEW });
	xrEnumerateViewConfigurationViews(_instance, _systemId, _viewConfigurationType, viewCount, &viewCount, vViewConfigurationViews.data());

	_vSwapChains.reserve(viewCount);
	_combinedSwapChainWidth = 0;
	_combinedSwapChainHeight = 0;
	VERUS_U_FOR(viewIndex, viewCount)
	{
		XrViewConfigurationView& xrvcv = vViewConfigurationViews[viewIndex];
		XrSwapchainCreateInfo xrsci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		xrsci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
		xrsci.format = format;
		xrsci.sampleCount = xrvcv.recommendedSwapchainSampleCount;
		xrsci.width = xrvcv.recommendedImageRectWidth;
		xrsci.height = xrvcv.recommendedImageRectHeight;
		xrsci.faceCount = 1;
		xrsci.arraySize = 1;
		xrsci.mipCount = 1;
		XrSwapchain swapchain = XR_NULL_HANDLE;
		if (XR_SUCCESS != (res = xrCreateSwapchain(_session, &xrsci, &swapchain)))
			throw VERUS_RUNTIME_ERROR << "xrCreateSwapchain(); res=" << res;

		uint32_t imageCount = 0;
		xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
		SwapChainEx swapChainEx;
		swapChainEx._handle = swapchain;
		swapChainEx._width = xrsci.width;
		swapChainEx._height = xrsci.height;
		swapChainEx._vImages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
		swapChainEx._vImageViews.resize(imageCount);
		xrEnumerateSwapchainImages(swapChainEx._handle, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapChainEx._vImages.data()));

		VERUS_U_FOR(imageIndex, imageCount)
			swapChainEx._vImageViews[imageIndex] = CreateImageView(pDevice, format, swapChainEx._vImages[imageIndex]);

		_combinedSwapChainWidth = Math::Max(_combinedSwapChainWidth, swapChainEx._width);
		_combinedSwapChainHeight = Math::Max(_combinedSwapChainHeight, swapChainEx._height);

		_swapChainBufferCount = imageCount;

		_vSwapChains.push_back(swapChainEx);
	}
}

ComPtr<ID3D11RenderTargetView> ExtRealityD3D11::CreateImageView(ID3D11Device* pDevice, int64_t format, XrSwapchainImageD3D11KHR& image)
{
	HRESULT hr = 0;
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = static_cast<DXGI_FORMAT>(format);
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	ComPtr<ID3D11RenderTargetView> pRTV;
	if (FAILED(hr = pDevice->CreateRenderTargetView(image.texture, &rtvDesc, &pRTV)))
		throw VERUS_RUNTIME_ERROR << "CreateRenderTargetView(); hr=" << VERUS_HR(hr);
	return pRTV;
}

XrSwapchain ExtRealityD3D11::GetSwapChain(int viewIndex)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
		return _vSwapChains[viewIndex]._handle;
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityD3D11::GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
	{
		w = _vSwapChains[viewIndex]._width;
		h = _vSwapChains[viewIndex]._height;
	}
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

ID3D11RenderTargetView* ExtRealityD3D11::GetRTV(int viewIndex, int imageIndex) const
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
	{
		return _vSwapChains[viewIndex]._vImageViews[imageIndex].Get();
	}
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityD3D11::CreateActions()
{
	BaseExtReality::CreateActions();
}

void ExtRealityD3D11::PollEvents()
{
	BaseExtReality::PollEvents();
}

void ExtRealityD3D11::SyncActions(UINT32 activeActionSetsMask)
{
	BaseExtReality::SyncActions(activeActionSetsMask);
}

bool ExtRealityD3D11::GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateBoolean(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityD3D11::GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateFloat(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityD3D11::GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction)
{
	return BaseExtReality::GetActionStatePose(actionIndex, currentState, pose, subaction);
}

void ExtRealityD3D11::BeginFrame()
{
	BaseExtReality::BeginFrame();
}

int ExtRealityD3D11::LocateViews()
{
	return BaseExtReality::LocateViews();
}

void ExtRealityD3D11::BeginView(int viewIndex, RViewDesc viewDesc)
{
	BaseExtReality::BeginView(viewIndex, viewDesc);
}

void ExtRealityD3D11::AcquireSwapChainImage()
{
	BaseExtReality::AcquireSwapChainImage();
}

void ExtRealityD3D11::EndView(int viewIndex)
{
	BaseExtReality::EndView(viewIndex);
}

void ExtRealityD3D11::EndFrame()
{
	BaseExtReality::EndFrame();
}
