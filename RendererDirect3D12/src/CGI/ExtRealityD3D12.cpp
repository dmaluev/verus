// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

static XrResult GetD3D12GraphicsRequirementsKHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsD3D12KHR* pGraphicsRequirements)
{
	PFN_xrGetD3D12GraphicsRequirementsKHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrGetD3D12GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, systemId, pGraphicsRequirements) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

// ExtRealityD3D12:

ExtRealityD3D12::ExtRealityD3D12()
{
	VERUS_ZERO_MEM(_adapterLuid);
}

ExtRealityD3D12::~ExtRealityD3D12()
{
	Done();
}

void ExtRealityD3D12::Init()
{
	VERUS_INIT();

	BaseExtReality::Init();

	_vRequiredExtensions.reserve(8);
	_vRequiredExtensions.push_back(XR_KHR_D3D12_ENABLE_EXTENSION_NAME);
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

void ExtRealityD3D12::Done()
{
	for (auto& swapChainEx : _vSwapChains)
	{
		swapChainEx._dhImageViews.Reset();
		VERUS_XR_DESTROY(swapChainEx._handle, xrDestroySwapchain(swapChainEx._handle));
	}
	_vSwapChains.clear();

	VERUS_DONE(ExtRealityD3D12);
}

void ExtRealityD3D12::InitByRenderer(PRendererD3D12 pRenderer)
{
	VERUS_QREF_CONST_SETTINGS;

	XrResult res = XR_SUCCESS;
	XrGraphicsBindingD3D12KHR xrgb = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
	xrgb.device = pRenderer->GetD3DDevice();
	xrgb.queue = pRenderer->GetD3DCommandQueue();
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

void ExtRealityD3D12::GetSystem()
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

	XrGraphicsRequirementsD3D12KHR xrgr = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
	if (XR_SUCCESS != (res = GetD3D12GraphicsRequirementsKHR(_instance, _systemId, &xrgr)))
		throw VERUS_RUNTIME_ERROR << "GetD3D12GraphicsRequirementsKHR(); res=" << res;

	_adapterLuid = xrgr.adapterLuid;
	_minFeatureLevel = xrgr.minFeatureLevel;
}

void ExtRealityD3D12::CreateSwapChains(ID3D12Device* pDevice, int64_t format)
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
		swapChainEx._vImages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR });
		swapChainEx._dhImageViews.Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, imageCount);
		xrEnumerateSwapchainImages(swapChainEx._handle, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapChainEx._vImages.data()));

		VERUS_U_FOR(imageIndex, imageCount)
			CreateImageView(pDevice, format, swapChainEx._vImages[imageIndex], swapChainEx._dhImageViews.AtCPU(imageIndex));

		_combinedSwapChainWidth = Math::Max(_combinedSwapChainWidth, swapChainEx._width);
		_combinedSwapChainHeight = Math::Max(_combinedSwapChainHeight, swapChainEx._height);

		_swapChainBufferCount = imageCount;

		_vSwapChains.push_back(swapChainEx);
	}
}

void ExtRealityD3D12::CreateImageView(ID3D12Device* pDevice, int64_t format, XrSwapchainImageD3D12KHR& image, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = static_cast<DXGI_FORMAT>(format);
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	pDevice->CreateRenderTargetView(image.texture, &rtvDesc, handle);
}

XrSwapchain ExtRealityD3D12::GetSwapChain(int viewIndex)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
		return _vSwapChains[viewIndex]._handle;
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityD3D12::GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
	{
		w = _vSwapChains[viewIndex]._width;
		h = _vSwapChains[viewIndex]._height;
	}
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

ID3D12Resource* ExtRealityD3D12::GetD3DResource(int viewIndex, int imageIndex) const
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
		return _vSwapChains[viewIndex]._vImages[imageIndex].texture;
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

D3D12_CPU_DESCRIPTOR_HANDLE ExtRealityD3D12::GetRTV(int viewIndex, int imageIndex) const
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
		return _vSwapChains[viewIndex]._dhImageViews.AtCPU(imageIndex);
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityD3D12::CreateActions()
{
	BaseExtReality::CreateActions();
}

void ExtRealityD3D12::PollEvents()
{
	BaseExtReality::PollEvents();
}

void ExtRealityD3D12::SyncActions(UINT32 activeActionSetsMask)
{
	BaseExtReality::SyncActions(activeActionSetsMask);
}

bool ExtRealityD3D12::GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateBoolean(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityD3D12::GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateFloat(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityD3D12::GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction)
{
	return BaseExtReality::GetActionStatePose(actionIndex, currentState, pose, subaction);
}

void ExtRealityD3D12::BeginFrame()
{
	BaseExtReality::BeginFrame();
}

int ExtRealityD3D12::LocateViews()
{
	return BaseExtReality::LocateViews();
}

void ExtRealityD3D12::BeginView(int viewIndex, RViewDesc viewDesc)
{
	BaseExtReality::BeginView(viewIndex, viewDesc);
}

void ExtRealityD3D12::AcquireSwapChainImage()
{
	BaseExtReality::AcquireSwapChainImage();
}

void ExtRealityD3D12::EndView(int viewIndex)
{
	BaseExtReality::EndView(viewIndex);
}

void ExtRealityD3D12::EndFrame()
{
	BaseExtReality::EndFrame();
}

void ExtRealityD3D12::BeginAreaUpdate()
{
	BaseExtReality::BeginAreaUpdate();
}

void ExtRealityD3D12::EndAreaUpdate(PcVector4 pUserOffset)
{
	BaseExtReality::EndAreaUpdate(pUserOffset);
}
