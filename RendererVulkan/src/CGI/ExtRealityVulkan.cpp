// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

static XrResult GetVulkanGraphicsRequirements2KHR(XrInstance instance, XrSystemId systemId, XrGraphicsRequirementsVulkan2KHR* pGraphicsRequirements)
{
	PFN_xrGetVulkanGraphicsRequirements2KHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, systemId, pGraphicsRequirements) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

static XrResult CreateVulkanInstanceKHR(XrInstance instance, const XrVulkanInstanceCreateInfoKHR* pCreateInfo, VkInstance* pVulkanInstance, VkResult* pVulkanResult)
{
	PFN_xrCreateVulkanInstanceKHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, pCreateInfo, pVulkanInstance, pVulkanResult) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

static XrResult GetVulkanGraphicsDevice2KHR(XrInstance instance, const XrVulkanGraphicsDeviceGetInfoKHR* pGetInfo, VkPhysicalDevice* pVulkanPhysicalDevice)
{
	PFN_xrGetVulkanGraphicsDevice2KHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, pGetInfo, pVulkanPhysicalDevice) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

static XrResult CreateVulkanDeviceKHR(XrInstance instance, const XrVulkanDeviceCreateInfoKHR* pCreateInfo, VkDevice* pVulkanDevice, VkResult* pVulkanResult)
{
	PFN_xrCreateVulkanDeviceKHR fn = nullptr;
	xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&fn));
	return fn ? fn(instance, pCreateInfo, pVulkanDevice, pVulkanResult) : XR_ERROR_EXTENSION_NOT_PRESENT;
}

// ExtRealityVulkan:

ExtRealityVulkan::ExtRealityVulkan()
{
}

ExtRealityVulkan::~ExtRealityVulkan()
{
	Done();
}

void ExtRealityVulkan::Init()
{
	VERUS_INIT();

	BaseExtReality::Init();

	_vRequiredExtensions.reserve(8);
	_vRequiredExtensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
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

void ExtRealityVulkan::Done()
{
	for (auto& swapChainEx : _vSwapChains)
	{
		for (auto swapChainImageView : swapChainEx._vImageViews)
		{
			VERUS_QREF_RENDERER_VULKAN;
			vkDestroyImageView(pRendererVulkan->GetVkDevice(), swapChainImageView, pRendererVulkan->GetAllocator());
		}
		swapChainEx._vImageViews.clear();
		VERUS_XR_DESTROY(swapChainEx._handle, xrDestroySwapchain(swapChainEx._handle));
	}
	_vSwapChains.clear();

	VERUS_DONE(ExtRealityVulkan);
}

VkResult ExtRealityVulkan::CreateVulkanInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	XrResult res = XR_SUCCESS;
	VkResult vkRes = VK_SUCCESS;
	XrVulkanInstanceCreateInfoKHR xrvici = { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
	xrvici.systemId = _systemId;
	xrvici.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
	xrvici.vulkanCreateInfo = pCreateInfo;
	xrvici.vulkanAllocator = pAllocator;
	if (XR_SUCCESS != (res = CreateVulkanInstanceKHR(_instance, &xrvici, pInstance, &vkRes)))
		throw VERUS_RUNTIME_ERROR << "CreateVulkanInstanceKHR(); res=" << res;
	return vkRes;
}

VkPhysicalDevice ExtRealityVulkan::GetVulkanGraphicsDevice(VkInstance vulkanInstance)
{
	XrResult res = XR_SUCCESS;
	XrVulkanGraphicsDeviceGetInfoKHR xrvgdgi = { XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR };
	xrvgdgi.systemId = _systemId;
	xrvgdgi.vulkanInstance = vulkanInstance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	if (XR_SUCCESS != (res = GetVulkanGraphicsDevice2KHR(_instance, &xrvgdgi, &physicalDevice)))
		throw VERUS_RUNTIME_ERROR << "GetVulkanGraphicsDevice2KHR(); res=" << res;
	return physicalDevice;
}

VkResult ExtRealityVulkan::CreateVulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
	XrResult res = XR_SUCCESS;
	VkResult vkRes = VK_SUCCESS;
	XrVulkanDeviceCreateInfoKHR xrvdci{ XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR };
	xrvdci.systemId = _systemId;
	xrvdci.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
	xrvdci.vulkanPhysicalDevice = physicalDevice;
	xrvdci.vulkanCreateInfo = pCreateInfo;
	xrvdci.vulkanAllocator = pAllocator;
	if (XR_SUCCESS != (res = CreateVulkanDeviceKHR(_instance, &xrvdci, pDevice, &vkRes)))
		throw VERUS_RUNTIME_ERROR << "CreateVulkanDeviceKHR(); res=" << res;
	return vkRes;
}

void ExtRealityVulkan::InitByRenderer(PRendererVulkan pRenderer)
{
	VERUS_QREF_CONST_SETTINGS;

	XrResult res = XR_SUCCESS;
	XrGraphicsBindingVulkan2KHR xrgb = { XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR };
	xrgb.instance = pRenderer->GetVkInstance();
	xrgb.physicalDevice = pRenderer->GetVkPhysicalDevice();
	xrgb.device = pRenderer->GetVkDevice();
	xrgb.queueFamilyIndex = pRenderer->GetGraphicsQueueFamilyIndex();
	xrgb.queueIndex = 0;
	XrSessionCreateInfo xrsci = { XR_TYPE_SESSION_CREATE_INFO };
	xrsci.next = &xrgb;
	xrsci.systemId = _systemId;
	if (XR_SUCCESS != (res = xrCreateSession(_instance, &xrsci, &_session)))
		throw VERUS_RUNTIME_ERROR << "xrCreateSession(); res=" << res;

	CreateReferenceSpace();
	CreateHeadSpace();
	CreateSwapChains(pRenderer, ToNativeFormat(Renderer::GetSwapChainFormat()));

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

void ExtRealityVulkan::GetSystem()
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

	XrGraphicsRequirementsVulkan2KHR xrgr = { XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR };
	if (XR_SUCCESS != (res = GetVulkanGraphicsRequirements2KHR(_instance, _systemId, &xrgr)))
		throw VERUS_RUNTIME_ERROR << "GetVulkanGraphicsRequirements2KHR(); res=" << res;
}

void ExtRealityVulkan::CreateSwapChains(PRendererVulkan pRenderer, int64_t format)
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
		swapChainEx._vImages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR });
		swapChainEx._vImageViews.resize(imageCount);
		xrEnumerateSwapchainImages(swapChainEx._handle, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(swapChainEx._vImages.data()));

		VERUS_U_FOR(imageIndex, imageCount)
			swapChainEx._vImageViews[imageIndex] = CreateImageView(pRenderer, format, swapChainEx._vImages[imageIndex]);

		_combinedSwapChainWidth = Math::Max(_combinedSwapChainWidth, swapChainEx._width);
		_combinedSwapChainHeight = Math::Max(_combinedSwapChainHeight, swapChainEx._height);

		_swapChainBufferCount = imageCount;

		_vSwapChains.push_back(swapChainEx);
	}
}

VkImageView ExtRealityVulkan::CreateImageView(PRendererVulkan pRenderer, int64_t format, XrSwapchainImageVulkan2KHR& image)
{
	VkResult res = VK_SUCCESS;
	VkImageViewCreateInfo vkivci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vkivci.image = image.image;
	vkivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vkivci.format = static_cast<VkFormat>(format);
	vkivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkivci.subresourceRange.baseMipLevel = 0;
	vkivci.subresourceRange.levelCount = 1;
	vkivci.subresourceRange.baseArrayLayer = 0;
	vkivci.subresourceRange.layerCount = 1;
	VkImageView imageView = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkCreateImageView(pRenderer->GetVkDevice(), &vkivci, pRenderer->GetAllocator(), &imageView)))
		throw VERUS_RUNTIME_ERROR << "vkCreateImageView(); res=" << res;
	return imageView;
}

XrSwapchain ExtRealityVulkan::GetSwapChain(int viewIndex)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
		return _vSwapChains[viewIndex]._handle;
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityVulkan::GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h)
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
	{
		w = _vSwapChains[viewIndex]._width;
		h = _vSwapChains[viewIndex]._height;
	}
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

VkImageView ExtRealityVulkan::GetVkImageView(int viewIndex, int imageIndex) const
{
	if (viewIndex >= 0 && viewIndex < _vSwapChains.size())
	{
		return _vSwapChains[viewIndex]._vImageViews[imageIndex];
	}
	else
		throw VERUS_RUNTIME_ERROR << "Invalid view index";
}

void ExtRealityVulkan::CreateActions()
{
	BaseExtReality::CreateActions();
}

void ExtRealityVulkan::PollEvents()
{
	BaseExtReality::PollEvents();
}

void ExtRealityVulkan::SyncActions(UINT32 activeActionSetsMask)
{
	BaseExtReality::SyncActions(activeActionSetsMask);
}

bool ExtRealityVulkan::GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateBoolean(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityVulkan::GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction)
{
	return BaseExtReality::GetActionStateFloat(actionIndex, currentState, pChangedState, subaction);
}

bool ExtRealityVulkan::GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction)
{
	return BaseExtReality::GetActionStatePose(actionIndex, currentState, pose, subaction);
}

void ExtRealityVulkan::BeginFrame()
{
	BaseExtReality::BeginFrame();
}

int ExtRealityVulkan::LocateViews()
{
	return BaseExtReality::LocateViews();
}

void ExtRealityVulkan::BeginView(int viewIndex, RViewDesc viewDesc)
{
	BaseExtReality::BeginView(viewIndex, viewDesc);
}

void ExtRealityVulkan::AcquireSwapChainImage()
{
	BaseExtReality::AcquireSwapChainImage();
}

void ExtRealityVulkan::EndView(int viewIndex)
{
	BaseExtReality::EndView(viewIndex);
}

void ExtRealityVulkan::EndFrame()
{
	BaseExtReality::EndFrame();
}

void ExtRealityVulkan::BeginAreaUpdate()
{
	BaseExtReality::BeginAreaUpdate();
}

void ExtRealityVulkan::EndAreaUpdate(PcVector4 pUserOffset)
{
	BaseExtReality::EndAreaUpdate(pUserOffset);
}
