// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

static VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (fn)
		return fn(instance, pCreateInfo, pAllocator, pMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT messenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (fn)
		fn(instance, messenger, pAllocator);
}

// RendererVulkan:

CSZ RendererVulkan::s_requiredValidationLayers[] =
{
	"VK_LAYER_KHRONOS_validation"
};

CSZ RendererVulkan::s_requiredDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

RendererVulkan::RendererVulkan()
{
}

RendererVulkan::~RendererVulkan()
{
	Done();
}

void RendererVulkan::ReleaseMe()
{
	Free();
	TestAllocCount();
}

void RendererVulkan::Init()
{
	VERUS_INIT();
	VkResult res = VK_SUCCESS;

	_vRenderPasses.reserve(20);
	_vFramebuffers.reserve(40);

	VulkanCompilerInit();

	CreateInstance();
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	CreateDebugUtilsMessenger();
#endif
	CreateSurface();
	PickPhysicalDevice();
	CreateDevice();

	VmaAllocatorCreateInfo vmaaci = {};
	vmaaci.physicalDevice = _physicalDevice;
	vmaaci.device = _device;
	vmaaci.pAllocationCallbacks = GetAllocator();
	vmaaci.instance = _instance;
	vmaaci.vulkanApiVersion = s_apiVersion;
	if (VK_SUCCESS != (res = vmaCreateAllocator(&vmaaci, &_vmaAllocator)))
		throw VERUS_RUNTIME_ERROR << "vmaCreateAllocator(); res=" << res;

	CreateSwapChain();
	CreateImageViews();
	CreateCommandPools();
	CreateSyncObjects();
	CreateSamplers();
}

void RendererVulkan::Done()
{
	WaitIdle();

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		Renderer::I().ImGuiSetCurrentContext(nullptr);
	}

	DeleteFramebuffer(FBHandle::Make(-2));
	DeleteRenderPass(RPHandle::Make(-2));

	for (auto sampler : _vSamplers)
		VERUS_VULKAN_DESTROY(sampler, vkDestroySampler(_device, sampler, GetAllocator()));
	_vSamplers.clear();
	VERUS_VULKAN_DESTROY(_descriptorPoolImGui, vkDestroyDescriptorPool(_device, _descriptorPoolImGui, GetAllocator()));
	VERUS_FOR(i, s_ringBufferSize)
	{
		VERUS_VULKAN_DESTROY(_acquireNextImageSemaphores[i], vkDestroySemaphore(_device, _acquireNextImageSemaphores[i], GetAllocator()));
		VERUS_VULKAN_DESTROY(_queueSubmitSemaphores[i], vkDestroySemaphore(_device, _queueSubmitSemaphores[i], GetAllocator()));
		VERUS_VULKAN_DESTROY(_queueSubmitFences[i], vkDestroyFence(_device, _queueSubmitFences[i], GetAllocator()));
	}
	VERUS_FOR(i, s_ringBufferSize)
		VERUS_VULKAN_DESTROY(_commandPools[i], vkDestroyCommandPool(_device, _commandPools[i], GetAllocator()));
	for (auto swapChainImageViews : _vSwapChainImageViews)
		vkDestroyImageView(_device, swapChainImageViews, GetAllocator());
	_vSwapChainImageViews.clear();
	_vSwapChainImages.clear();
	VERUS_VULKAN_DESTROY(_swapChain, vkDestroySwapchainKHR(_device, _swapChain, GetAllocator()));

	VERUS_VULKAN_DESTROY(_vmaAllocator, vmaDestroyAllocator(_vmaAllocator));
	VERUS_VULKAN_DESTROY(_device, vkDestroyDevice(_device, GetAllocator()));
	VERUS_VULKAN_DESTROY(_surface, vkDestroySurfaceKHR(_instance, _surface, GetAllocator()));
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	VERUS_VULKAN_DESTROY(_debugUtilsMessenger, DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, GetAllocator()));
#endif
	VERUS_VULKAN_DESTROY(_instance, vkDestroyInstance(_instance, GetAllocator()));

	VulkanCompilerDone();

	VERUS_DONE(RendererVulkan);
}

void RendererVulkan::VulkanCompilerInit()
{
#ifdef _WIN32
	PFNVULKANCOMPILERINIT VulkanCompilerInit = reinterpret_cast<PFNVULKANCOMPILERINIT>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VulkanCompilerInit"));
	VulkanCompilerInit();
#else
	PFNVULKANCOMPILERINIT VulkanCompilerInit = reinterpret_cast<PFNVULKANCOMPILERINIT>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VulkanCompilerInit"));
	VulkanCompilerInit();
#endif
}

void RendererVulkan::VulkanCompilerDone()
{
#ifdef _WIN32
	PFNVULKANCOMPILERDONE VulkanCompilerDone = reinterpret_cast<PFNVULKANCOMPILERDONE>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VulkanCompilerDone"));
	VulkanCompilerDone();
#else
	PFNVULKANCOMPILERDONE VulkanCompilerDone = reinterpret_cast<PFNVULKANCOMPILERDONE>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VulkanCompilerDone"));
	VulkanCompilerDone();
#endif
}

bool RendererVulkan::VulkanCompile(CSZ source, CSZ sourceName, CSZ* defines, BaseShaderInclude* pInclude,
	CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs)
{
#ifdef _WIN32
	PFNVULKANCOMPILE VulkanCompile = reinterpret_cast<PFNVULKANCOMPILE>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VulkanCompile"));
	return VulkanCompile(source, sourceName, defines, pInclude, entryPoint, target, flags, ppCode, pSize, ppErrorMsgs);
#else
	PFNVULKANCOMPILE VulkanCompile = reinterpret_cast<VulkanCompile>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VulkanCompile"));
	return VulkanCompile(source, sourceName, defines, pInclude, entryPoint, target, flags, ppCode, pSize, ppErrorMsgs);
#endif
}

VKAPI_ATTR VkBool32 VKAPI_CALL RendererVulkan::DebugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	D::Log::Severity severity = D::Log::Severity::error;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		severity = D::Log::Severity::debug;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		severity = D::Log::Severity::info;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		severity = D::Log::Severity::warning;
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		severity = D::Log::Severity::error;
	D::Log::I().Write(pCallbackData->pMessage, std::this_thread::get_id(), __FILE__, __LINE__, severity);
	return VK_FALSE;
}

bool RendererVulkan::CheckRequiredValidationLayers()
{
	uint32_t propertyCount = 0;
	vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
	Vector<VkLayerProperties> vLayerProperties(propertyCount);
	vkEnumerateInstanceLayerProperties(&propertyCount, vLayerProperties.data());
	for (CSZ layerName : s_requiredValidationLayers)
	{
		bool layerFound = false;
		for (const auto& layerProperties : vLayerProperties)
		{
			if (!strcmp(layerName, layerProperties.layerName))
			{
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;
	}
	return true;
}

Vector<CSZ> RendererVulkan::GetRequiredExtensions()
{
	VERUS_QREF_RENDERER;
	SDL_Window* pWnd = renderer.GetMainWindow()->GetSDL();
	VERUS_RT_ASSERT(pWnd);
	Vector<CSZ> vExtensions;
	unsigned int count = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(pWnd, &count, nullptr))
		throw VERUS_RUNTIME_ERROR << "SDL_Vulkan_GetInstanceExtensions(nullptr)";
	vExtensions.reserve(count + 1);
	vExtensions.resize(count);
	if (!SDL_Vulkan_GetInstanceExtensions(pWnd, &count, vExtensions.data()))
		throw VERUS_RUNTIME_ERROR << "SDL_Vulkan_GetInstanceExtensions()";
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	vExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
	return vExtensions;
}

void RendererVulkan::CreateInstance()
{
	VkResult res = VK_SUCCESS;

#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	if (!CheckRequiredValidationLayers())
		throw VERUS_RUNTIME_ERROR << "CheckRequiredValidationLayers()";
#endif

	const auto vExtensions = GetRequiredExtensions();

	const int major = (VERUS_SDK_VERSION >> 24) & 0xFF;
	const int minor = (VERUS_SDK_VERSION >> 16) & 0xFF;
	const int patch = (VERUS_SDK_VERSION) & 0xFFFF;

	VkValidationFeatureEnableEXT enabledValidationFeatures[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT };
	VkValidationFeaturesEXT vkvf = {};
	vkvf.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
	vkvf.enabledValidationFeatureCount = VERUS_COUNT_OF(enabledValidationFeatures);
	vkvf.pEnabledValidationFeatures = enabledValidationFeatures;

	VkApplicationInfo vkai = {};
	vkai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkai.pApplicationName = "Game";
	vkai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkai.pEngineName = "Verus";
	vkai.engineVersion = VK_MAKE_VERSION(major, minor, patch);
	vkai.apiVersion = s_apiVersion;

	VkInstanceCreateInfo vkici = {};
	vkici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkici.pApplicationInfo = &vkai;
	vkici.enabledExtensionCount = Utils::Cast32(vExtensions.size());
	vkici.ppEnabledExtensionNames = vExtensions.data();
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	vkici.pNext = &vkvf;
	VkDebugUtilsMessengerCreateInfoEXT vkdumci = {};
	FillDebugUtilsMessengerCreateInfo(vkdumci);
	vkvf.pNext = &vkdumci;
	vkici.enabledLayerCount = VERUS_COUNT_OF(s_requiredValidationLayers);
	vkici.ppEnabledLayerNames = s_requiredValidationLayers;
#endif
	if (VK_SUCCESS != (res = vkCreateInstance(&vkici, GetAllocator(), &_instance)))
		throw VERUS_RUNTIME_ERROR << "vkCreateInstance(); res=" << res;
}

void RendererVulkan::FillDebugUtilsMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& vkdumci)
{
	vkdumci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	vkdumci.messageSeverity =
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
#endif
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	vkdumci.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	vkdumci.pfnUserCallback = DebugUtilsMessengerCallback;
}

void RendererVulkan::CreateDebugUtilsMessenger()
{
	VkResult res = VK_SUCCESS;
	VkDebugUtilsMessengerCreateInfoEXT vkdumci = {};
	FillDebugUtilsMessengerCreateInfo(vkdumci);
	if (VK_SUCCESS != (res = CreateDebugUtilsMessengerEXT(_instance, &vkdumci, GetAllocator(), &_debugUtilsMessenger)))
		throw VERUS_RUNTIME_ERROR << "CreateDebugUtilsMessengerEXT(); res=" << res;
}

void RendererVulkan::CreateSurface()
{
	VERUS_QREF_RENDERER;
	SDL_Window* pWnd = renderer.GetMainWindow()->GetSDL();
	VERUS_RT_ASSERT(pWnd);
	if (!SDL_Vulkan_CreateSurface(pWnd, _instance, &_surface))
		throw VERUS_RUNTIME_ERROR << "SDL_Vulkan_CreateSurface()";
}

bool RendererVulkan::CheckRequiredDeviceExtensions(VkPhysicalDevice device)
{
	uint32_t propertyCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &propertyCount, nullptr);
	Vector<VkExtensionProperties> vExtensionProperties(propertyCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &propertyCount, vExtensionProperties.data());
	Set<String> setRequiredDeviceExtensions;
	for (CSZ extensionName : s_requiredDeviceExtensions)
		setRequiredDeviceExtensions.insert(extensionName);
	for (const auto& extensionProperties : vExtensionProperties)
		setRequiredDeviceExtensions.erase(extensionProperties.extensionName);
	return setRequiredDeviceExtensions.empty();
}

RendererVulkan::QueueFamilyIndices RendererVulkan::FindQueueFamilyIndices(VkPhysicalDevice device)
{
	QueueFamilyIndices ret;
	uint32_t queueFamilyPropertyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount, nullptr);
	Vector<VkQueueFamilyProperties> vQueueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount, vQueueFamilyProperties.data());
	int i = 0;
	for (const auto& queueFamilyProperties : vQueueFamilyProperties)
	{
		if (queueFamilyProperties.queueCount > 0 && (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			ret._graphicsFamilyIndex = i;

		VkBool32 supported = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &supported);
		if (queueFamilyProperties.queueCount > 0 && supported)
			ret._presentFamilyIndex = i;

		if (ret.IsComplete())
			break;

		i++;
	}
	return ret;
}

bool RendererVulkan::IsDeviceSuitable(VkPhysicalDevice device)
{
	const QueueFamilyIndices queueFamilyIndices = FindQueueFamilyIndices(device);
	if (!queueFamilyIndices.IsComplete())
		return false;

	const bool extensionsSupported = CheckRequiredDeviceExtensions(device);
	bool swapChainOK = false;
	if (extensionsSupported)
	{
		const SwapChainInfo swapChainInfo = GetSwapChainInfo(device);
		swapChainOK = !swapChainInfo._vSurfaceFormats.empty() && !swapChainInfo._vSurfacePresentModes.empty();
	}
	return extensionsSupported && swapChainOK;
}

void RendererVulkan::PickPhysicalDevice()
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);
	if (!physicalDeviceCount)
		throw VERUS_RUNTIME_ERROR << "vkEnumeratePhysicalDevices(); physicalDeviceCount=0";
	Vector<VkPhysicalDevice> vPhysicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, vPhysicalDevices.data());
	for (const auto& physicalDevice : vPhysicalDevices)
	{
		if (IsDeviceSuitable(physicalDevice))
		{
			_physicalDevice = physicalDevice;
			break;
		}
	}
	if (VK_NULL_HANDLE == _physicalDevice)
		throw VERUS_RUNTIME_ERROR << "PhysicalDevice not found";

	VkPhysicalDeviceProperties vkpdp = {};
	vkGetPhysicalDeviceProperties(_physicalDevice, &vkpdp);
	const uint32_t major = VK_VERSION_MAJOR(vkpdp.apiVersion);
	const uint32_t minor = VK_VERSION_MINOR(vkpdp.apiVersion);
	const uint32_t patch = VK_VERSION_PATCH(vkpdp.apiVersion);
	VERUS_LOG_INFO("Vulkan version supported: " << major << "." << minor << "." << patch);
	VERUS_LOG_INFO("Device name: " << vkpdp.deviceName);
}

void RendererVulkan::CreateDevice()
{
	VERUS_QREF_CONST_SETTINGS;
	VkResult res = VK_SUCCESS;

	VkPhysicalDeviceLineRasterizationFeaturesEXT lineRasterizationFeatures = {};
	lineRasterizationFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
	VkPhysicalDeviceFeatures2 vkpdf2 = {};
	vkpdf2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	vkpdf2.pNext = &lineRasterizationFeatures;
	vkGetPhysicalDeviceFeatures2(_physicalDevice, &vkpdf2);

	lineRasterizationFeatures.stippledRectangularLines = VK_FALSE;
	lineRasterizationFeatures.stippledBresenhamLines = VK_FALSE;
	lineRasterizationFeatures.stippledSmoothLines = VK_FALSE;
	_advancedLineRasterization =
		lineRasterizationFeatures.rectangularLines &&
		lineRasterizationFeatures.bresenhamLines &&
		lineRasterizationFeatures.smoothLines;

	_queueFamilyIndices = FindQueueFamilyIndices(_physicalDevice);
	VERUS_RT_ASSERT(_queueFamilyIndices.IsComplete());
	Set<int> setUniqueQueueFamilies = { _queueFamilyIndices._graphicsFamilyIndex, _queueFamilyIndices._presentFamilyIndex };
	Vector<VkDeviceQueueCreateInfo> vDeviceQueueCreateInfos;
	vDeviceQueueCreateInfos.reserve(setUniqueQueueFamilies.size());
	const float queuePriorities[] = { 1.0f };
	for (int queueFamilyIndex : setUniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo vkdqci = {};
		vkdqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		vkdqci.queueFamilyIndex = queueFamilyIndex;
		vkdqci.queueCount = 1;
		vkdqci.pQueuePriorities = queuePriorities;
		vDeviceQueueCreateInfos.push_back(vkdqci);
	}

	// Check https://vulkan.gpuinfo.org/ for device coverage:
	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.fillModeNonSolid = VK_TRUE; // 79%
	physicalDeviceFeatures.geometryShader = VK_TRUE; // 80%
	physicalDeviceFeatures.independentBlend = VK_TRUE; // 99%
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE; // 90%
	physicalDeviceFeatures.shaderClipDistance = VK_TRUE; // 85%
	physicalDeviceFeatures.shaderImageGatherExtended = VK_TRUE; // 94%
	physicalDeviceFeatures.tessellationShader = settings._gpuTessellation ? VK_TRUE : VK_FALSE; // 81%

	VkDeviceCreateInfo vkdci = {};
	vkdci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vkdci.pNext = &lineRasterizationFeatures;
	vkdci.queueCreateInfoCount = Utils::Cast32(vDeviceQueueCreateInfos.size());
	vkdci.pQueueCreateInfos = vDeviceQueueCreateInfos.data();
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	vkdci.enabledLayerCount = VERUS_COUNT_OF(s_requiredValidationLayers);
	vkdci.ppEnabledLayerNames = s_requiredValidationLayers;
#endif
	vkdci.enabledExtensionCount = VERUS_COUNT_OF(s_requiredDeviceExtensions);
	vkdci.ppEnabledExtensionNames = s_requiredDeviceExtensions;
	vkdci.pEnabledFeatures = &physicalDeviceFeatures;
	if (VK_SUCCESS != (res = vkCreateDevice(_physicalDevice, &vkdci, GetAllocator(), &_device)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDevice(); res=" << res;

	vkGetDeviceQueue(_device, _queueFamilyIndices._graphicsFamilyIndex, 0, &_graphicsQueue);
	vkGetDeviceQueue(_device, _queueFamilyIndices._presentFamilyIndex, 0, &_presentQueue);
}

RendererVulkan::SwapChainInfo RendererVulkan::GetSwapChainInfo(VkPhysicalDevice device)
{
	SwapChainInfo swapChainInfo;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &swapChainInfo._surfaceCapabilities);

	uint32_t surfaceFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, nullptr);
	if (surfaceFormatCount)
	{
		swapChainInfo._vSurfaceFormats.resize(surfaceFormatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, swapChainInfo._vSurfaceFormats.data());
	}

	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
	if (presentModeCount)
	{
		swapChainInfo._vSurfacePresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, swapChainInfo._vSurfacePresentModes.data());
	}

	return swapChainInfo;
}

void RendererVulkan::CreateSwapChain(VkSwapchainKHR oldSwapchain)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	VkResult res = VK_SUCCESS;

	_swapChainBufferCount = settings._displayVSync ? 3 : 2;

	const SwapChainInfo swapChainInfo = GetSwapChainInfo(_physicalDevice);

	// Useful for maximized windows:
	const uint32_t width = Math::Clamp<uint32_t>(renderer.GetSwapChainWidth(),
		swapChainInfo._surfaceCapabilities.minImageExtent.width, swapChainInfo._surfaceCapabilities.maxImageExtent.width);
	const uint32_t height = Math::Clamp<uint32_t>(renderer.GetSwapChainHeight(),
		swapChainInfo._surfaceCapabilities.minImageExtent.height, swapChainInfo._surfaceCapabilities.maxImageExtent.height);

	VkSwapchainCreateInfoKHR vksci = {};
	vksci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	vksci.surface = _surface;
	vksci.minImageCount = _swapChainBufferCount;
	vksci.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	vksci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	vksci.imageExtent.width = width;
	vksci.imageExtent.height = height;
	vksci.imageArrayLayers = 1;
	vksci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	vksci.preTransform = swapChainInfo._surfaceCapabilities.currentTransform;
	vksci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	vksci.presentMode = settings._displayVSync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
	vksci.clipped = VK_TRUE;
	vksci.oldSwapchain = oldSwapchain;
	const uint32_t queueFamilyIndicesArray[] =
	{
		static_cast<uint32_t>(_queueFamilyIndices._graphicsFamilyIndex),
		static_cast<uint32_t>(_queueFamilyIndices._presentFamilyIndex)
	};
	if (_queueFamilyIndices.IsSameQueue())
	{
		vksci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	else
	{
		vksci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		vksci.queueFamilyIndexCount = VERUS_COUNT_OF(queueFamilyIndicesArray);
		vksci.pQueueFamilyIndices = queueFamilyIndicesArray;
	}
	if (VK_SUCCESS != (res = vkCreateSwapchainKHR(_device, &vksci, GetAllocator(), &_swapChain)))
		throw VERUS_RUNTIME_ERROR << "vkCreateSwapchainKHR(); res=" << res;

	uint32_t swapchainImageCount = _swapChainBufferCount;
	vkGetSwapchainImagesKHR(_device, _swapChain, &swapchainImageCount, nullptr);
	_vSwapChainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(_device, _swapChain, &swapchainImageCount, _vSwapChainImages.data());
	_swapChainBufferCount = swapchainImageCount;
}

void RendererVulkan::CreateImageViews()
{
	VkResult res = VK_SUCCESS;
	_vSwapChainImageViews.resize(_swapChainBufferCount);
	VERUS_FOR(i, _swapChainBufferCount)
	{
		VkImageViewCreateInfo vkivci = {};
		vkivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vkivci.image = _vSwapChainImages[i];
		vkivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vkivci.format = VK_FORMAT_B8G8R8A8_SRGB;
		vkivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkivci.subresourceRange.baseMipLevel = 0;
		vkivci.subresourceRange.levelCount = 1;
		vkivci.subresourceRange.baseArrayLayer = 0;
		vkivci.subresourceRange.layerCount = 1;
		if (VK_SUCCESS != (res = vkCreateImageView(_device, &vkivci, GetAllocator(), &_vSwapChainImageViews[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateImageView(); res=" << res;
	}
}

void RendererVulkan::CreateCommandPools()
{
	VkResult res = VK_SUCCESS;
	VERUS_FOR(i, s_ringBufferSize)
	{
		VkCommandPoolCreateInfo vkcpci = {};
		vkcpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		vkcpci.queueFamilyIndex = _queueFamilyIndices._graphicsFamilyIndex;
		if (VK_SUCCESS != (res = vkCreateCommandPool(_device, &vkcpci, GetAllocator(), &_commandPools[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateCommandPool(); res=" << res;
	}
}

void RendererVulkan::CreateSyncObjects()
{
	VkResult res = VK_SUCCESS;
	VkSemaphoreCreateInfo vksci = {};
	vksci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo vkfci = {};
	vkfci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkfci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VERUS_FOR(i, s_ringBufferSize)
	{
		if (VK_SUCCESS != (res = vkCreateSemaphore(_device, &vksci, GetAllocator(), &_acquireNextImageSemaphores[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateSemaphore(); res=" << res;
		if (VK_SUCCESS != (res = vkCreateSemaphore(_device, &vksci, GetAllocator(), &_queueSubmitSemaphores[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateSemaphore(); res=" << res;
		if (VK_SUCCESS != (res = vkCreateFence(_device, &vkfci, GetAllocator(), &_queueSubmitFences[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateFence(); res=" << res;
	}
}

void RendererVulkan::CreateSamplers()
{
	VERUS_QREF_CONST_SETTINGS;

	const bool tf = settings._gpuTrilinearFilter;

	_vSamplers.resize(+Sampler::count);

	VkSamplerCreateInfo vksci = {};
	VkSamplerCreateInfo init = {};
	init.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	init.magFilter = VK_FILTER_LINEAR;
	init.minFilter = VK_FILTER_LINEAR;
	init.mipmapMode = tf ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	init.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	init.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	init.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	init.mipLodBias = 0;
	init.anisotropyEnable = VK_FALSE;
	init.maxAnisotropy = 0;
	init.compareEnable = VK_FALSE;
	init.compareOp = VK_COMPARE_OP_ALWAYS;
	init.minLod = 0;
	init.maxLod = FLT_MAX;
	init.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	auto Create = [this](const VkSamplerCreateInfo& vksci)
	{
		VkResult res = VK_SUCCESS;
		VkSampler sampler = VK_NULL_HANDLE;
		if (VK_SUCCESS != (res = vkCreateSampler(_device, &vksci, GetAllocator(), &sampler)))
			throw VERUS_RUNTIME_ERROR << "vkCreateSampler(); res=" << res;
		return sampler;
	};

	vksci = init;
	vksci.mipLodBias = -2;
	vksci.anisotropyEnable = (settings._gpuAnisotropyLevel > 0) ? VK_TRUE : VK_FALSE;
	vksci.maxAnisotropy = static_cast<float>(settings._gpuAnisotropyLevel);
	_vSamplers[+Sampler::lodBias] = Create(vksci);

	vksci = init;
	if (settings._sceneShadowQuality <= App::Settings::Quality::low)
	{
		vksci.magFilter = VK_FILTER_NEAREST;
		vksci.minFilter = VK_FILTER_NEAREST;
	}
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vksci.compareEnable = VK_TRUE;
	vksci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	_vSamplers[+Sampler::shadow] = Create(vksci);

	vksci = init;
	vksci.anisotropyEnable = (settings._gpuAnisotropyLevel > 0) ? VK_TRUE : VK_FALSE;
	vksci.maxAnisotropy = static_cast<float>(settings._gpuAnisotropyLevel);
	_vSamplers[+Sampler::aniso] = Create(vksci);

	vksci = init;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vksci.anisotropyEnable = (settings._gpuAnisotropyLevel > 0) ? VK_TRUE : VK_FALSE;
	vksci.maxAnisotropy = static_cast<float>(settings._gpuAnisotropyLevel);
	_vSamplers[+Sampler::anisoClamp] = Create(vksci);

	vksci = init;
	vksci.mipLodBias = -0.5f;
	vksci.anisotropyEnable = (settings._gpuAnisotropyLevel > 0) ? VK_TRUE : VK_FALSE;
	vksci.maxAnisotropy = static_cast<float>(settings._gpuAnisotropyLevel);
	_vSamplers[+Sampler::anisoSharp] = Create(vksci);

	// <Repeat>
	vksci = init;
	_vSamplers[+Sampler::linearMipL] = Create(vksci);

	vksci = init;
	vksci.magFilter = VK_FILTER_NEAREST;
	vksci.minFilter = VK_FILTER_NEAREST;
	_vSamplers[+Sampler::nearestMipL] = Create(vksci);

	vksci = init;
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	_vSamplers[+Sampler::linearMipN] = Create(vksci);

	vksci = init;
	vksci.magFilter = VK_FILTER_NEAREST;
	vksci.minFilter = VK_FILTER_NEAREST;
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	_vSamplers[+Sampler::nearestMipN] = Create(vksci);
	// </Repeat>

	// <Clamp>
	vksci = init;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	_vSamplers[+Sampler::linearClampMipL] = Create(vksci);

	vksci = init;
	vksci.magFilter = VK_FILTER_NEAREST;
	vksci.minFilter = VK_FILTER_NEAREST;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	_vSamplers[+Sampler::nearestClampMipL] = Create(vksci);

	vksci = init;
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	_vSamplers[+Sampler::linearClampMipN] = Create(vksci);

	vksci = init;
	vksci.magFilter = VK_FILTER_NEAREST;
	vksci.minFilter = VK_FILTER_NEAREST;
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	vksci.addressModeU = vksci.addressModeV = vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	_vSamplers[+Sampler::nearestClampMipN] = Create(vksci);
	// </Clamp>
}

VkCommandBuffer RendererVulkan::CreateCommandBuffer(VkCommandPool commandPool)
{
	VkResult res = VK_SUCCESS;
	VkCommandBufferAllocateInfo vkcbai = {};
	vkcbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	vkcbai.commandPool = commandPool;
	vkcbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkcbai.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkAllocateCommandBuffers(_device, &vkcbai, &commandBuffer)))
		throw VERUS_RUNTIME_ERROR << "vkAllocateCommandBuffers(); res=" << res;
	return commandBuffer;
}

const VkSampler* RendererVulkan::GetImmutableSampler(Sampler s) const
{
	return &_vSamplers[+s];
}

void RendererVulkan::ImGuiCheckVkResultFn(VkResult res)
{
	if (VK_SUCCESS != res)
		throw VERUS_RUNTIME_ERROR << "ImGuiCheckVkResultFn(); res=" << res;
}

void RendererVulkan::ImGuiInit(RPHandle renderPassHandle)
{
	VkResult res = VK_SUCCESS;
	VkDescriptorPoolSize vkdps = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
	VkDescriptorPoolCreateInfo vkdpci = {};
	vkdpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	vkdpci.maxSets = 1;
	vkdpci.poolSizeCount = 1;
	vkdpci.pPoolSizes = &vkdps;
	if (VK_SUCCESS != (res = vkCreateDescriptorPool(_device, &vkdpci, GetAllocator(), &_descriptorPoolImGui)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorPool(); res=" << res;

	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	IMGUI_CHECKVERSION();
	ImGuiContext* pContext = ImGui::CreateContext();
	renderer.ImGuiSetCurrentContext(pContext);
	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	io.IniFilename = nullptr;
	if (!settings._imguiFont.empty())
	{
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(_C(settings._imguiFont), vData);
		void* pFontData = IM_ALLOC(vData.size());
		memcpy(pFontData, vData.data(), vData.size());
		io.Fonts->AddFontFromMemoryTTF(pFontData, Utils::Cast32(vData.size()), static_cast<float>(settings.GetFontSize()), nullptr, io.Fonts->GetGlyphRangesCyrillic());
	}

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForVulkan(renderer.GetMainWindow()->GetSDL());
	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance = _instance;
	info.PhysicalDevice = _physicalDevice;
	info.Device = _device;
	info.QueueFamily = _queueFamilyIndices._graphicsFamilyIndex;
	info.Queue = _graphicsQueue;
	info.PipelineCache = nullptr;
	info.DescriptorPool = _descriptorPoolImGui;
	info.Allocator = GetAllocator();
	info.MinImageCount = settings._displayVSync ? 3 : 2;
	info.ImageCount = s_ringBufferSize;
	info.CheckVkResultFn = ImGuiCheckVkResultFn;
	ImGui_ImplVulkan_Init(&info, _vRenderPasses[renderPassHandle.Get()]);

	CommandBufferVulkan commandBuffer;
	commandBuffer.InitOneTimeSubmit();
	ImGui_ImplVulkan_CreateFontsTexture(commandBuffer.GetVkCommandBuffer());
	commandBuffer.DoneOneTimeSubmit();
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RendererVulkan::ImGuiRenderDrawData()
{
	VERUS_QREF_RENDERER;
	renderer.UpdateUtilization();
	ImGui::Render();
	VkCommandBuffer commandBuffer = static_cast<CommandBufferVulkan*>(renderer.GetCommandBuffer().Get())->GetVkCommandBuffer();
	if (ImGui::GetDrawData())
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void RendererVulkan::ResizeSwapChain()
{
	for (auto swapChainImageViews : _vSwapChainImageViews)
		vkDestroyImageView(_device, swapChainImageViews, GetAllocator());
	_vSwapChainImageViews.clear();
	_vSwapChainImages.clear();

	VkSwapchainKHR oldSwapchain = _swapChain;
	CreateSwapChain(oldSwapchain);
	CreateImageViews();
	VERUS_VULKAN_DESTROY(oldSwapchain, vkDestroySwapchainKHR(_device, oldSwapchain, GetAllocator()));
}

void RendererVulkan::BeginFrame(bool present)
{
	VERUS_QREF_RENDERER;
	VkResult res = VK_SUCCESS;

	if (VK_SUCCESS != (res = vkResetFences(_device, 1, &_queueSubmitFences[_ringBufferIndex])))
		throw VERUS_RUNTIME_ERROR << "vkResetFences(); res=" << res;
	if (present)
	{
		uint32_t imageIndex = _swapChainBufferIndex;
		if (VK_SUCCESS != (res = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _acquireNextImageSemaphores[_ringBufferIndex], VK_NULL_HANDLE, &imageIndex)))
			throw VERUS_RUNTIME_ERROR << "vkAcquireNextImageKHR(); res=" << res;
		_swapChainBufferIndex = imageIndex;
	}

	vmaSetCurrentFrameIndex(_vmaAllocator, static_cast<uint32_t>(renderer.GetFrameCount()));

	if (VK_SUCCESS != (res = vkResetCommandPool(_device, _commandPools[_ringBufferIndex], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT)))
		throw VERUS_RUNTIME_ERROR << "vkResetCommandPool(); res=" << res;

	renderer.GetCommandBuffer()->Begin();

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void RendererVulkan::EndFrame(bool present)
{
	VERUS_QREF_RENDERER;
	VkResult res = VK_SUCCESS;

	UpdateScheduled();

	ImGui::EndFrame();

	auto cb = renderer.GetCommandBuffer();
	cb->End();

	const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	_acquireNextImageSemaphore = _acquireNextImageSemaphores[_ringBufferIndex];
	_queueSubmitSemaphore = _queueSubmitSemaphores[_ringBufferIndex];
	VkCommandBuffer commandBuffer = static_cast<CommandBufferVulkan*>(cb.Get())->GetVkCommandBuffer();
	VkSubmitInfo vksi = {};
	vksi.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	if (present)
	{
		vksi.waitSemaphoreCount = 1;
		vksi.pWaitSemaphores = &_acquireNextImageSemaphore;
		vksi.pWaitDstStageMask = waitStages;
	}
	vksi.commandBufferCount = 1;
	vksi.pCommandBuffers = &commandBuffer;
	if (!_queueFamilyIndices.IsSameQueue())
	{
		vksi.signalSemaphoreCount = 1;
		vksi.pSignalSemaphores = &_queueSubmitSemaphore;
	}
	if (VK_SUCCESS != (res = vkQueueSubmit(_graphicsQueue, 1, &vksi, _queueSubmitFences[_ringBufferIndex])))
		throw VERUS_RUNTIME_ERROR << "vkQueueSubmit(); res=" << res;
}

void RendererVulkan::Present()
{
	VkResult res = VK_SUCCESS;

	const VkSwapchainKHR swapChains[] = { _swapChain };

	const uint32_t imageIndex = _swapChainBufferIndex;
	VkPresentInfoKHR vkpi = {};
	vkpi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	if (!_queueFamilyIndices.IsSameQueue())
	{
		vkpi.waitSemaphoreCount = 1;
		vkpi.pWaitSemaphores = &_queueSubmitSemaphore;
	}
	vkpi.swapchainCount = VERUS_COUNT_OF(swapChains);
	vkpi.pSwapchains = swapChains;
	vkpi.pImageIndices = &imageIndex;
	if (VK_SUCCESS != (res = vkQueuePresentKHR(_presentQueue, &vkpi)))
	{
		if (res != VK_ERROR_OUT_OF_DATE_KHR)
			throw VERUS_RUNTIME_ERROR << "vkQueuePresentKHR(); res=" << res;
	}
}

void RendererVulkan::Sync(bool present)
{
	VkResult res = VK_SUCCESS;

	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;

	if (VK_SUCCESS != (res = vkWaitForFences(_device, 1, &_queueSubmitFences[_ringBufferIndex], VK_TRUE, UINT64_MAX)))
		throw VERUS_RUNTIME_ERROR << "vkWaitForFences(); res=" << res;
}

void RendererVulkan::WaitIdle()
{
	if (_device)
		vkDeviceWaitIdle(_device);
}

// Resources:

PBaseCommandBuffer RendererVulkan::InsertCommandBuffer()
{
	return TStoreCommandBuffers::Insert();
}

PBaseGeometry RendererVulkan::InsertGeometry()
{
	return TStoreGeometry::Insert();
}

PBasePipeline RendererVulkan::InsertPipeline()
{
	return TStorePipelines::Insert();
}

PBaseShader RendererVulkan::InsertShader()
{
	return TStoreShaders::Insert();
}

PBaseTexture RendererVulkan::InsertTexture()
{
	return TStoreTextures::Insert();
}

void RendererVulkan::DeleteCommandBuffer(PBaseCommandBuffer p)
{
	TStoreCommandBuffers::Delete(static_cast<PCommandBufferVulkan>(p));
}

void RendererVulkan::DeleteGeometry(PBaseGeometry p)
{
	TStoreGeometry::Delete(static_cast<PGeometryVulkan>(p));
}

void RendererVulkan::DeletePipeline(PBasePipeline p)
{
	TStorePipelines::Delete(static_cast<PPipelineVulkan>(p));
}

void RendererVulkan::DeleteShader(PBaseShader p)
{
	TStoreShaders::Delete(static_cast<PShaderVulkan>(p));
}

void RendererVulkan::DeleteTexture(PBaseTexture p)
{
	TStoreTextures::Delete(static_cast<PTextureVulkan>(p));
}

RPHandle RendererVulkan::CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD)
{
	VkResult res = VK_SUCCESS;

	auto ToNativeLoadOp = [](RP::Attachment::LoadOp op)
	{
		switch (op)
		{
		case RP::Attachment::LoadOp::load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
		case RP::Attachment::LoadOp::clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case RP::Attachment::LoadOp::dontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		default: throw VERUS_RECOVERABLE << "CreateRenderPass(); LoadOp";
		};
	};
	auto ToNativeStoreOp = [](RP::Attachment::StoreOp op)
	{
		switch (op)
		{
		case RP::Attachment::StoreOp::store:    return VK_ATTACHMENT_STORE_OP_STORE;
		case RP::Attachment::StoreOp::dontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		default: throw VERUS_RECOVERABLE << "CreateRenderPass(); StoreOp";
		}
	};

	Vector<VkAttachmentDescription> vAttachmentDesc;
	vAttachmentDesc.reserve(ilA.size());
	for (const auto& attachment : ilA)
	{
		VkAttachmentDescription vkad = {};
		vkad.format = ToNativeFormat(attachment._format);
		vkad.samples = ToNativeSampleCount(attachment._sampleCount);
		vkad.loadOp = ToNativeLoadOp(attachment._loadOp);
		vkad.storeOp = ToNativeStoreOp(attachment._storeOp);
		vkad.stencilLoadOp = ToNativeLoadOp(attachment._stencilLoadOp);
		vkad.stencilStoreOp = ToNativeStoreOp(attachment._stencilStoreOp);
		vkad.initialLayout = ToNativeImageLayout(attachment._initialLayout);
		vkad.finalLayout = ToNativeImageLayout(attachment._finalLayout);
		vAttachmentDesc.push_back(vkad);
	}

	auto GetAttachmentIndexByName = [&ilA](CSZ name) -> uint32_t
	{
		if (!name)
			return VK_ATTACHMENT_UNUSED;
		uint32_t index = 0;
		for (const auto& attachment : ilA)
		{
			if (!strcmp(attachment._name, name))
				return index;
			index++;
		}
		throw VERUS_RECOVERABLE << "CreateRenderPass(); Attachment not found";
	};

	struct SubpassMetadata
	{
		int inputRefsOffset = -1;
		int colorRefsOffset = -1;
		int resolveRefsOffset = -1;
		int depthStencilRefOffset = -1;
		int preserveOffset = -1;
	};

	Vector<VkAttachmentReference> vAttachmentRefs;
	vAttachmentRefs.reserve(20);
	Vector<uint32_t> vAttachmentIndices;
	vAttachmentIndices.reserve(20);

	Vector<SubpassMetadata> vSubpassMetadata;
	vSubpassMetadata.reserve(ilS.size());
	Vector<VkSubpassDescription> vSubpassDesc;
	vSubpassDesc.reserve(ilS.size());
	for (const auto& subpass : ilS)
	{
		SubpassMetadata subpassMetadata;
		VkSubpassDescription vksd = {};
		vksd.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		vksd.inputAttachmentCount = Utils::Cast32(subpass._ilInput.size());
		subpassMetadata.inputRefsOffset = Utils::Cast32(vAttachmentRefs.size());
		for (const auto& input : subpass._ilInput)
		{
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(input._name);
			vkar.layout = ToNativeImageLayout(input._layout);
			vAttachmentRefs.push_back(vkar);
		}

		vksd.colorAttachmentCount = Utils::Cast32(subpass._ilColor.size());
		subpassMetadata.colorRefsOffset = Utils::Cast32(vAttachmentRefs.size());
		for (const auto& color : subpass._ilColor)
		{
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(color._name);
			vkar.layout = ToNativeImageLayout(color._layout);
			vAttachmentRefs.push_back(vkar);
		}

		if (subpass._depthStencil._name)
		{
			subpassMetadata.depthStencilRefOffset = Utils::Cast32(vAttachmentRefs.size());
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(subpass._depthStencil._name);
			vkar.layout = ToNativeImageLayout(subpass._depthStencil._layout);
			vAttachmentRefs.push_back(vkar);
		}

		vksd.preserveAttachmentCount = Utils::Cast32(subpass._ilPreserve.size());
		subpassMetadata.preserveOffset = Utils::Cast32(vAttachmentIndices.size());
		for (const auto& preserve : subpass._ilPreserve)
		{
			const uint32_t index = GetAttachmentIndexByName(preserve._name);
			vAttachmentIndices.push_back(index);
		}

		vSubpassDesc.push_back(vksd);
		vSubpassMetadata.push_back(subpassMetadata);
	}

	// vAttachmentRefs is ready, convert offsets to actual pointers:
	if (vAttachmentRefs.empty())
		throw VERUS_RECOVERABLE << "CreateRenderPass(); No attachment references";
	int index = 0;
	for (auto& sd : vSubpassDesc)
	{
		const SubpassMetadata& sm = vSubpassMetadata[index];
		if (sd.inputAttachmentCount)
			sd.pInputAttachments = &vAttachmentRefs[sm.inputRefsOffset];
		if (sd.colorAttachmentCount)
			sd.pColorAttachments = &vAttachmentRefs[sm.colorRefsOffset];
		if (sm.depthStencilRefOffset >= 0)
			sd.pDepthStencilAttachment = &vAttachmentRefs[sm.depthStencilRefOffset];
		if (sd.preserveAttachmentCount)
			sd.pPreserveAttachments = &vAttachmentIndices[sm.preserveOffset];
		index++;
	}

	auto GetSubpassIndexByName = [&ilS](CSZ name) -> uint32_t
	{
		if (!name)
			return VK_SUBPASS_EXTERNAL;
		uint32_t index = 0;
		for (const auto& subpass : ilS)
		{
			if (!strcmp(subpass._name, name))
				return index;
			index++;
		}
		throw VERUS_RECOVERABLE << "CreateRenderPass(); Subpass not found";
	};

	Vector<VkSubpassDependency> vSubpassDependency;
	vSubpassDependency.reserve(ilD.size());
	for (const auto& dependency : ilD)
	{
		VkSubpassDependency vksd = {};
		vksd.srcSubpass = GetSubpassIndexByName(dependency._srcSubpass);
		vksd.dstSubpass = GetSubpassIndexByName(dependency._dstSubpass);
		switch (dependency._mode)
		{
		case 0:
		{
			vksd.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vksd.srcAccessMask = 0;
			vksd.dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			vksd.dstAccessMask =
				VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			vksd.dependencyFlags = 0;
		}
		break;
		case 1:
		{
			vksd.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			vksd.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			vksd.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vksd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vksd.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		}
		break;
		}
		vSubpassDependency.push_back(vksd);
	}

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkRenderPassCreateInfo vkrpci = {};
	vkrpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vkrpci.attachmentCount = Utils::Cast32(vAttachmentDesc.size());
	vkrpci.pAttachments = vAttachmentDesc.data();
	vkrpci.subpassCount = Utils::Cast32(vSubpassDesc.size());
	vkrpci.pSubpasses = vSubpassDesc.data();
	vkrpci.dependencyCount = Utils::Cast32(vSubpassDependency.size());
	vkrpci.pDependencies = vSubpassDependency.data();
	if (VK_SUCCESS != (res = vkCreateRenderPass(_device, &vkrpci, GetAllocator(), &renderPass)))
		throw VERUS_RUNTIME_ERROR << "vkCreateRenderPass(); res=" << res;

	const int nextIndex = GetNextRenderPassIndex();
	if (nextIndex >= _vRenderPasses.size())
		_vRenderPasses.push_back(renderPass);
	else
		_vRenderPasses[nextIndex] = renderPass;

	return RPHandle::Make(nextIndex);
}

FBHandle RendererVulkan::CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex, CubeMapFace cubeMapFace)
{
	VkResult res = VK_SUCCESS;

	VkImageView imageViews[VERUS_MAX_FB_ATTACH] = {};
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkFramebufferCreateInfo vkfci = {};
	vkfci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	vkfci.renderPass = GetRenderPass(renderPassHandle);
	int count = 0;
	if (swapChainBufferIndex >= 0)
	{
		imageViews[count] = _vSwapChainImageViews[swapChainBufferIndex];
		count++;
	}
	for (const auto& x : il)
	{
		VERUS_RT_ASSERT(count < VERUS_MAX_FB_ATTACH);
		auto& texVulkan = static_cast<RTextureVulkan>(*x);
		switch (cubeMapFace)
		{
		case CubeMapFace::all:  imageViews[count] = texVulkan.GetVkImageViewForFramebuffer(static_cast<CubeMapFace>(count)); break;
		case CubeMapFace::none: imageViews[count] = texVulkan.GetVkImageViewForFramebuffer(CubeMapFace::none); break;
		default:                imageViews[count] = texVulkan.GetVkImageViewForFramebuffer(!count ? cubeMapFace : CubeMapFace::none);
		}
		count++;
	}
	vkfci.attachmentCount = count;
	vkfci.pAttachments = imageViews;
	vkfci.width = w;
	vkfci.height = h;
	vkfci.layers = 1;
	if (VK_SUCCESS != (res = vkCreateFramebuffer(_device, &vkfci, GetAllocator(), &framebuffer)))
		throw VERUS_RUNTIME_ERROR << "vkCreateFramebuffer(); res=" << res;

	const int nextIndex = GetNextFramebufferIndex();
	Framebuffer fb;
	fb._framebuffer = framebuffer;
	fb._width = w;
	fb._height = h;
	if (nextIndex >= _vFramebuffers.size())
		_vFramebuffers.push_back(fb);
	else
		_vFramebuffers[nextIndex] = fb;

	return FBHandle::Make(nextIndex);
}

void RendererVulkan::DeleteRenderPass(RPHandle handle)
{
	if (handle.IsSet())
	{
		vkDestroyRenderPass(_device, _vRenderPasses[handle.Get()], GetAllocator());
		_vRenderPasses[handle.Get()] = VK_NULL_HANDLE;
	}
	else if (-2 == handle.Get())
	{
		for (auto renderPass : _vRenderPasses)
			vkDestroyRenderPass(_device, renderPass, GetAllocator());
		_vRenderPasses.clear();
	}
}

void RendererVulkan::DeleteFramebuffer(FBHandle handle)
{
	if (handle.IsSet())
	{
		vkDestroyFramebuffer(_device, _vFramebuffers[handle.Get()]._framebuffer, GetAllocator());
		_vFramebuffers[handle.Get()]._framebuffer = VK_NULL_HANDLE;
	}
	else if (-2 == handle.Get())
	{
		for (auto framebuffer : _vFramebuffers)
			vkDestroyFramebuffer(_device, framebuffer._framebuffer, GetAllocator());
		_vFramebuffers.clear();
	}
}

int RendererVulkan::GetNextRenderPassIndex() const
{
	const int count = Utils::Cast32(_vRenderPasses.size());
	VERUS_FOR(i, count)
	{
		if (VK_NULL_HANDLE == _vRenderPasses[i])
			return i;
	}
	return count;
}

int RendererVulkan::GetNextFramebufferIndex() const
{
	const int count = Utils::Cast32(_vFramebuffers.size());
	VERUS_FOR(i, count)
	{
		if (VK_NULL_HANDLE == _vFramebuffers[i]._framebuffer)
			return i;
	}
	return count;
}

VkRenderPass RendererVulkan::GetRenderPass(RPHandle handle) const
{
	return _vRenderPasses[handle.Get()];
}

RendererVulkan::RcFramebuffer RendererVulkan::GetFramebuffer(FBHandle handle) const
{
	return _vFramebuffers[handle.Get()];
}

void RendererVulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, HostAccess hostAccess, VkBuffer& buffer, VmaAllocation& vmaAllocation)
{
	VkResult res = VK_SUCCESS;

	VkBufferCreateInfo vkbci = {};
	vkbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vkbci.size = size;
	vkbci.usage = usage;
	VmaAllocationCreateInfo vmaaci = {};
	vmaaci.usage = VMA_MEMORY_USAGE_AUTO;
	switch (hostAccess)
	{
	case HostAccess::sequentialWrite: vmaaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; break;
	case HostAccess::random: vmaaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; break;
	}
	if (VK_SUCCESS != (res = vmaCreateBuffer(_vmaAllocator, &vkbci, &vmaaci, &buffer, &vmaAllocation, nullptr)))
		throw VERUS_RECOVERABLE << "vmaCreateBuffer(); res=" << res;
}

void RendererVulkan::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto commandBuffer = static_cast<PCommandBufferVulkan>(pCB)->GetVkCommandBuffer();

	VkBufferCopy region = {};
	region.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &region);
}

void RendererVulkan::CreateImage(const VkImageCreateInfo* pImageCreateInfo, HostAccess hostAccess, VkImage& image, VmaAllocation& vmaAllocation)
{
	VkResult res = VK_SUCCESS;

	VmaAllocationCreateInfo vmaaci = {};
	vmaaci.usage = VMA_MEMORY_USAGE_AUTO;
	switch (hostAccess)
	{
	case HostAccess::sequentialWrite: vmaaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; break;
	case HostAccess::random: vmaaci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; break;
	}
	if (VK_SUCCESS != (res = vmaCreateImage(_vmaAllocator, pImageCreateInfo, &vmaaci, &image, &vmaAllocation, nullptr)))
		throw VERUS_RECOVERABLE << "vmaCreateImage(); res=" << res;
}

void RendererVulkan::CopyImage(
	VkImage srcImage, uint32_t srcMipLevel, uint32_t srcArrayLayer,
	VkImage dstImage, uint32_t dstMipLevel, uint32_t dstArrayLayer,
	uint32_t width, uint32_t height,
	PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto commandBuffer = static_cast<PCommandBufferVulkan>(pCB)->GetVkCommandBuffer();

	VkImageCopy region = {};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.mipLevel = srcMipLevel;
	region.srcSubresource.baseArrayLayer = srcArrayLayer;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.mipLevel = dstMipLevel;
	region.dstSubresource.baseArrayLayer = dstArrayLayer;
	region.dstSubresource.layerCount = 1;
	region.extent = { width, height, 1 };

	vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void RendererVulkan::CopyBufferToImage(
	VkBuffer buffer,
	VkImage image, uint32_t mipLevel, uint32_t arrayLayer,
	uint32_t width, uint32_t height,
	PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto commandBuffer = static_cast<PCommandBufferVulkan>(pCB)->GetVkCommandBuffer();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = mipLevel;
	region.imageSubresource.baseArrayLayer = arrayLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void RendererVulkan::CopyImageToBuffer(
	VkImage image, uint32_t mipLevel, uint32_t arrayLayer,
	uint32_t width, uint32_t height,
	VkBuffer buffer,
	PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto commandBuffer = static_cast<PCommandBufferVulkan>(pCB)->GetVkCommandBuffer();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = mipLevel;
	region.imageSubresource.baseArrayLayer = arrayLayer;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
}

void RendererVulkan::UpdateUtilization()
{
	for (auto& x : TStoreGeometry::_list)
		x.UpdateUtilization();
	for (auto& x : TStoreShaders::_list)
		x.UpdateUtilization();
}
