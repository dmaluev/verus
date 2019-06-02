#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

static VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
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
	auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (fn)
		fn(instance, messenger, pAllocator);
}

// RendererVulkan:

CSZ RendererVulkan::s_requiredValidationLayers[] =
{
	"VK_LAYER_LUNARG_standard_validation"
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

	_vRenderPasses.reserve(20);
	_vFramebuffers.reserve(40);

	VerusCompilerInit();

	CreateInstance();
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	CreateDebugUtilsMessenger();
#endif
	CreateSurface();
	PickPhysicalDevice();
	CreateDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateCommandPools();
	CreateSyncObjects();
}

void RendererVulkan::Done()
{
	WaitIdle();
	DeleteFramebuffer(-1);
	DeleteRenderPass(-1);
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
	VERUS_VULKAN_DESTROY(_device, vkDestroyDevice(_device, GetAllocator()));
	VERUS_VULKAN_DESTROY(_surface, vkDestroySurfaceKHR(_instance, _surface, GetAllocator()));
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	VERUS_VULKAN_DESTROY(_debugUtilsMessenger, DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, GetAllocator()));
#endif
	VERUS_VULKAN_DESTROY(_instance, vkDestroyInstance(_instance, GetAllocator()));

	VerusCompilerDone();

	VERUS_DONE(RendererVulkan);
}

void RendererVulkan::VerusCompilerInit()
{
#ifdef _WIN32
	PFNVERUSCOMPILERINIT VerusCompilerInit = reinterpret_cast<PFNVERUSCOMPILERINIT>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VerusCompilerInit"));
	VerusCompilerInit();
#else
	PFNVERUSCOMPILERINIT VerusCompilerInit = reinterpret_cast<PFNVERUSCOMPILERINIT>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VerusCompilerInit"));
	VerusCompilerInit();
#endif
}

void RendererVulkan::VerusCompilerDone()
{
#ifdef _WIN32
	PFNVERUSCOMPILERDONE VerusCompilerDone = reinterpret_cast<PFNVERUSCOMPILERDONE>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VerusCompilerDone"));
	VerusCompilerDone();
#else
	PFNVERUSCOMPILERDONE VerusCompilerDone = reinterpret_cast<PFNVERUSCOMPILERDONE>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VerusCompilerDone"));
	VerusCompilerDone();
#endif
}

bool RendererVulkan::VerusCompile(CSZ source, CSZ* defines, BaseShaderInclude* pInclude,
	CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs)
{
#ifdef _WIN32
	PFNVERUSCOMPILE VerusCompile = reinterpret_cast<PFNVERUSCOMPILE>(
		GetProcAddress(LoadLibraryA("VulkanShaderCompiler.dll"), "VerusCompile"));
	return VerusCompile(source, defines, pInclude, entryPoint, target, flags, ppCode, pSize, ppErrorMsgs);
#else
	PFNVERUSCOMPILE VerusCompile = reinterpret_cast<VerusCompile>(
		dlsym(dlopen("./libVulkanShaderCompiler.so", RTLD_LAZY), "VerusCompile"));
	return VerusCompile(source, defines, pInclude, entryPoint, target, flags, ppCode, pSize, ppErrorMsgs);
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
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	vExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
	return vExtensions;
}

void RendererVulkan::CreateInstance()
{
	VkResult res = VK_SUCCESS;

#if defined(_DEBUG) || defined(VERUS_DEBUG)
	if (!CheckRequiredValidationLayers())
		throw VERUS_RUNTIME_ERROR << "CheckRequiredValidationLayers()";
#endif

	const auto vExtensions = GetRequiredExtensions();

	VkApplicationInfo vkai = {};
	vkai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkai.pApplicationName = "Game";
	vkai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkai.pEngineName = "Verus";
	vkai.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	vkai.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo vkici = {};
	vkici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkici.pApplicationInfo = &vkai;
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	vkici.enabledLayerCount = VERUS_ARRAY_LENGTH(s_requiredValidationLayers);
	vkici.ppEnabledLayerNames = s_requiredValidationLayers;
#endif
	vkici.enabledExtensionCount = Utils::Cast32(vExtensions.size());
	vkici.ppEnabledExtensionNames = vExtensions.data();
	if (VK_SUCCESS != (res = vkCreateInstance(&vkici, nullptr, &_instance)))
		throw VERUS_RUNTIME_ERROR << "vkCreateInstance(), res=" << res;
}

void RendererVulkan::CreateDebugUtilsMessenger()
{
	VkResult res = VK_SUCCESS;
	VkDebugUtilsMessengerCreateInfoEXT vkdumci = {};
	vkdumci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	vkdumci.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	vkdumci.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	vkdumci.pfnUserCallback = DebugUtilsMessengerCallback;
	if (VK_SUCCESS != (res = CreateDebugUtilsMessengerEXT(_instance, &vkdumci, nullptr, &_debugUtilsMessenger)))
		throw VERUS_RUNTIME_ERROR << "CreateDebugUtilsMessengerEXT(), res=" << res;
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
		throw VERUS_RUNTIME_ERROR << "vkEnumeratePhysicalDevices(), physicalDeviceCount=0";
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
}

void RendererVulkan::CreateDevice()
{
	VkResult res = VK_SUCCESS;

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
	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	VkDeviceCreateInfo vkdci = {};
	vkdci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	vkdci.queueCreateInfoCount = Utils::Cast32(vDeviceQueueCreateInfos.size());
	vkdci.pQueueCreateInfos = vDeviceQueueCreateInfos.data();
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	vkdci.enabledLayerCount = VERUS_ARRAY_LENGTH(s_requiredValidationLayers);
	vkdci.ppEnabledLayerNames = s_requiredValidationLayers;
#endif
	vkdci.enabledExtensionCount = VERUS_ARRAY_LENGTH(s_requiredDeviceExtensions);
	vkdci.ppEnabledExtensionNames = s_requiredDeviceExtensions;
	vkdci.pEnabledFeatures = &physicalDeviceFeatures;
	if (VK_SUCCESS != (res = vkCreateDevice(_physicalDevice, &vkdci, nullptr, &_device)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDevice(), res=" << res;

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

void RendererVulkan::CreateSwapChain()
{
	VERUS_QREF_SETTINGS;

	VkResult res = VK_SUCCESS;

	_numSwapChainBuffers = settings._screenVSync ? 3 : 2;

	const SwapChainInfo swapChainInfo = GetSwapChainInfo(_physicalDevice);

	VkSwapchainCreateInfoKHR vksci = {};
	vksci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	vksci.surface = _surface;
	vksci.minImageCount = _numSwapChainBuffers;
	vksci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	vksci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	vksci.imageExtent.width = settings._screenSizeWidth;
	vksci.imageExtent.height = settings._screenSizeHeight;
	vksci.imageArrayLayers = 1;
	vksci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	vksci.preTransform = swapChainInfo._surfaceCapabilities.currentTransform;
	vksci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	vksci.presentMode = settings._screenVSync ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
	vksci.clipped = VK_TRUE;
	vksci.oldSwapchain = VK_NULL_HANDLE;
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
		vksci.queueFamilyIndexCount = VERUS_ARRAY_LENGTH(queueFamilyIndicesArray);
		vksci.pQueueFamilyIndices = queueFamilyIndicesArray;
	}
	if (VK_SUCCESS != (res = vkCreateSwapchainKHR(_device, &vksci, nullptr, &_swapChain)))
		throw VERUS_RUNTIME_ERROR << "vkCreateSwapchainKHR(), res=" << res;

	vkGetSwapchainImagesKHR(_device, _swapChain, &_numSwapChainBuffers, nullptr);
	_vSwapChainImages.resize(_numSwapChainBuffers);
	vkGetSwapchainImagesKHR(_device, _swapChain, &_numSwapChainBuffers, _vSwapChainImages.data());
}

void RendererVulkan::CreateImageViews()
{
	VkResult res = VK_SUCCESS;
	_vSwapChainImageViews.resize(_numSwapChainBuffers);
	VERUS_U_FOR(i, _numSwapChainBuffers)
	{
		VkImageViewCreateInfo vkivci = {};
		vkivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vkivci.image = _vSwapChainImages[i];
		vkivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vkivci.format = VK_FORMAT_B8G8R8A8_UNORM;
		vkivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		vkivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkivci.subresourceRange.baseMipLevel = 0;
		vkivci.subresourceRange.levelCount = 1;
		vkivci.subresourceRange.baseArrayLayer = 0;
		vkivci.subresourceRange.layerCount = 1;
		if (VK_SUCCESS != (res = vkCreateImageView(_device, &vkivci, nullptr, &_vSwapChainImageViews[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateImageView(), res=" << res;
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
		if (VK_SUCCESS != (res = vkCreateCommandPool(_device, &vkcpci, nullptr, &_commandPools[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateCommandPool(), res=" << res;
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
	VERUS_U_FOR(i, s_ringBufferSize)
	{
		if (VK_SUCCESS != (res = vkCreateSemaphore(_device, &vksci, nullptr, &_acquireNextImageSemaphores[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateSemaphore(), res=" << res;
		if (VK_SUCCESS != (res = vkCreateSemaphore(_device, &vksci, nullptr, &_queueSubmitSemaphores[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateSemaphore(), res=" << res;
		if (VK_SUCCESS != (res = vkCreateFence(_device, &vkfci, nullptr, &_queueSubmitFences[i])))
			throw VERUS_RUNTIME_ERROR << "vkCreateFence(), res=" << res;
	}
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
		throw VERUS_RUNTIME_ERROR << "vkAllocateCommandBuffers(), res=" << res;
	return commandBuffer;
}

void RendererVulkan::BeginFrame(bool present)
{
	VERUS_QREF_RENDERER;
	VkResult res = VK_SUCCESS;

	if (VK_SUCCESS != (res = vkWaitForFences(_device, 1, &_queueSubmitFences[_ringBufferIndex], VK_TRUE, UINT64_MAX)))
		throw VERUS_RUNTIME_ERROR << "vkWaitForFences(), res=" << res;
	if (VK_SUCCESS != (res = vkResetFences(_device, 1, &_queueSubmitFences[_ringBufferIndex])))
		throw VERUS_RUNTIME_ERROR << "vkResetFences(), res=" << res;

	if (present)
	{
		if (VK_SUCCESS != (res = vkAcquireNextImageKHR(_device, _swapChain, UINT64_MAX, _acquireNextImageSemaphores[_ringBufferIndex], VK_NULL_HANDLE, &_swapChainBufferIndex)))
			throw VERUS_RUNTIME_ERROR << "vkAcquireNextImageKHR(), res=" << res;
	}

	if (VK_SUCCESS != (res = vkResetCommandPool(_device, _commandPools[_ringBufferIndex], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT)))
		throw VERUS_RUNTIME_ERROR << "vkResetCommandPool(), res=" << res;

	renderer.GetCommandBuffer()->Begin();
}

void RendererVulkan::EndFrame(bool present)
{
	VERUS_QREF_RENDERER;
	VkResult res = VK_SUCCESS;

	renderer.GetCommandBuffer()->End();

	const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	_acquireNextImageSemaphore = _acquireNextImageSemaphores[_ringBufferIndex];
	_queueSubmitSemaphore = _queueSubmitSemaphores[_ringBufferIndex];

	VkCommandBuffer commandBuffer = static_cast<CommandBufferVulkan*>(&(*renderer.GetCommandBuffer()))->GetVkCommandBuffer();
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
		throw VERUS_RUNTIME_ERROR << "vkQueueSubmit(), res=" << res;

	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;
}

void RendererVulkan::Present()
{
	VkResult res = VK_SUCCESS;

	const VkSwapchainKHR swapChains[] = { _swapChain };

	VkPresentInfoKHR vkpi = {};
	vkpi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	if (!_queueFamilyIndices.IsSameQueue())
	{
		vkpi.waitSemaphoreCount = 1;
		vkpi.pWaitSemaphores = &_queueSubmitSemaphore;
	}
	vkpi.swapchainCount = VERUS_ARRAY_LENGTH(swapChains);
	vkpi.pSwapchains = swapChains;
	vkpi.pImageIndices = &_swapChainBufferIndex;
	if (VK_SUCCESS != (res = vkQueuePresentKHR(_presentQueue, &vkpi)))
		throw VERUS_RUNTIME_ERROR << "vkQueuePresentKHR(), res=" << res;
}

void RendererVulkan::Clear(UINT32 flags)
{
	VERUS_QREF_RENDERER;

	VkImageMemoryBarrier vkimb = {};
	vkimb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	vkimb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vkimb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkimb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.image = _vSwapChainImages[_swapChainBufferIndex];
	vkimb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkimb.subresourceRange.baseMipLevel = 0;
	vkimb.subresourceRange.levelCount = 1;
	vkimb.subresourceRange.baseArrayLayer = 0;
	vkimb.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(static_cast<CommandBufferVulkan*>(&(*renderer.GetCommandBuffer()))->GetVkCommandBuffer(),
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &vkimb);

	VkClearColorValue clearColorValue;
	memcpy(&clearColorValue, renderer.GetClearColor().ToPointer(), sizeof(clearColorValue));
	VkImageSubresourceRange vkisr = {};
	vkisr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkisr.baseMipLevel = 0;
	vkisr.levelCount = 1;
	vkisr.baseArrayLayer = 0;
	vkisr.layerCount = 1;
	vkCmdClearColorImage(static_cast<CommandBufferVulkan*>(&(*renderer.GetCommandBuffer()))->GetVkCommandBuffer(),
		_vSwapChainImages[_swapChainBufferIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &vkisr);
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

int RendererVulkan::CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD)
{
	VkResult res = VK_SUCCESS;

	auto ToNativeLoadOp = [](RP::Attachment::LoadOp op)
	{
		switch (op)
		{
		case RP::Attachment::LoadOp::load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
		case RP::Attachment::LoadOp::clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case RP::Attachment::LoadOp::dontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		default: throw VERUS_RECOVERABLE << "CreateRenderPass(), LoadOp";
		};
	};
	auto ToNativeStoreOp = [](RP::Attachment::StoreOp op)
	{
		switch (op)
		{
		case RP::Attachment::StoreOp::store:    return VK_ATTACHMENT_STORE_OP_STORE;
		case RP::Attachment::StoreOp::dontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		default: throw VERUS_RECOVERABLE << "CreateRenderPass(), StoreOp";
		}
	};

	Vector<VkAttachmentDescription> vAttachmentDesc;
	vAttachmentDesc.reserve(ilA.size());
	for (auto& attachment : ilA)
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
		for (auto& attachment : ilA)
		{
			if (!strcmp(attachment._name, name))
				return index;
			index++;
		}
		throw VERUS_RECOVERABLE << "CreateRenderPass(), Attachment not found";
	};

	struct SubpassMetadata
	{
		int inputRefIndex = -1;
		int colorRefIndex = -1;
		int resolveRefIndex = -1;
		int depthStencilRefIndex = -1;
		int preserveIndex = -1;
	};

	Vector<VkAttachmentReference> vAttachmentRef;
	vAttachmentRef.reserve(20);
	Vector<uint32_t> vAttachmentIndex;
	vAttachmentIndex.reserve(20);

	Vector<SubpassMetadata> vSubpassMetadata;
	vSubpassMetadata.reserve(ilS.size());
	Vector<VkSubpassDescription> vSubpassDesc;
	vSubpassDesc.reserve(ilS.size());
	for (auto& subpass : ilS)
	{
		SubpassMetadata subpassMetadata;
		VkSubpassDescription vksd = {};
		vksd.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		vksd.inputAttachmentCount = Utils::Cast32(subpass._ilInput.size());
		subpassMetadata.inputRefIndex = Utils::Cast32(vAttachmentRef.size());
		for (auto& input : subpass._ilInput)
		{
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(input._name);
			vkar.layout = ToNativeImageLayout(input._layout);
			vAttachmentRef.push_back(vkar);
		}

		vksd.colorAttachmentCount = Utils::Cast32(subpass._ilColor.size());
		subpassMetadata.colorRefIndex = Utils::Cast32(vAttachmentRef.size());
		for (auto& color : subpass._ilColor)
		{
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(color._name);
			vkar.layout = ToNativeImageLayout(color._layout);
			vAttachmentRef.push_back(vkar);
		}

		if (subpass._depthStencil._name)
		{
			subpassMetadata.depthStencilRefIndex = Utils::Cast32(vAttachmentRef.size());
			VkAttachmentReference vkar = {};
			vkar.attachment = GetAttachmentIndexByName(subpass._depthStencil._name);
			vkar.layout = ToNativeImageLayout(subpass._depthStencil._layout);
			vAttachmentRef.push_back(vkar);
		}

		vksd.preserveAttachmentCount = Utils::Cast32(subpass._ilPreserve.size());
		subpassMetadata.preserveIndex = Utils::Cast32(vAttachmentIndex.size());
		for (auto& preserve : subpass._ilPreserve)
		{
			const uint32_t index = GetAttachmentIndexByName(preserve._name);
			vAttachmentIndex.push_back(index);
		}

		vSubpassDesc.push_back(vksd);
		vSubpassMetadata.push_back(subpassMetadata);
	}

	// vAttachmentRef is ready, convert indices to actual pointers:
	if (vAttachmentRef.empty())
		throw VERUS_RECOVERABLE << "CreateRenderPass(), No attachment references";
	int index = 0;
	for (auto& sd : vSubpassDesc)
	{
		const SubpassMetadata& sm = vSubpassMetadata[index];
		if (sd.inputAttachmentCount)
			sd.pInputAttachments = &vAttachmentRef[sm.inputRefIndex];
		if (sd.colorAttachmentCount)
			sd.pColorAttachments = &vAttachmentRef[sm.colorRefIndex];
		if (sm.depthStencilRefIndex >= 0)
			sd.pDepthStencilAttachment = &vAttachmentRef[sm.depthStencilRefIndex];
		if (sd.preserveAttachmentCount)
			sd.pPreserveAttachments = &vAttachmentIndex[sm.preserveIndex];
		index++;
	}

	Vector<VkSubpassDependency> vSubpassDependency;
	vSubpassDependency.reserve(ilD.size());
	for (auto& dependency : ilD)
	{
		VkSubpassDependency vksd = {};

		vSubpassDependency.push_back(vksd);
	}

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vSubpassDependency.push_back(dependency);

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
		throw VERUS_RUNTIME_ERROR << "vkCreateRenderPass(), res=" << res;

	const int id = GetNextRenderPassID();
	if (id >= _vRenderPasses.size())
		_vRenderPasses.push_back(renderPass);
	else
		_vRenderPasses[id] = renderPass;

	return id;
}

int RendererVulkan::CreateFramebuffer(int renderPassID, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex)
{
	VkResult res = VK_SUCCESS;

	VkImageView imageViews[VERUS_CGI_MAX_RT] = {};
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkFramebufferCreateInfo vkfci = {};
	vkfci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	vkfci.renderPass = GetRenderPassByID(renderPassID);
	int num = 0;
	if (swapChainBufferIndex >= 0)
	{
		imageViews[num] = _vSwapChainImageViews[swapChainBufferIndex];
		num++;
	}
	for (const auto& x : il)
	{
		auto& texVulkan = static_cast<RTextureVulkan>(*x);
		imageViews[num] = texVulkan.GetVkImageView();
		num++;
	}
	vkfci.attachmentCount = num;
	vkfci.pAttachments = imageViews;
	vkfci.width = w;
	vkfci.height = h;
	vkfci.layers = 1;
	if (VK_SUCCESS != (res = vkCreateFramebuffer(_device, &vkfci, GetAllocator(), &framebuffer)))
		throw VERUS_RUNTIME_ERROR << "vkCreateFramebuffer(), res=" << res;

	const int id = GetNextFramebufferID();
	if (id >= _vFramebuffers.size())
		_vFramebuffers.push_back(framebuffer);
	else
		_vFramebuffers[id] = framebuffer;

	return id;
}

void RendererVulkan::DeleteRenderPass(int id)
{
	if (id >= 0)
	{
		vkDestroyRenderPass(_device, _vRenderPasses[id], GetAllocator());
		_vRenderPasses[id] = VK_NULL_HANDLE;
	}
	else
	{
		for (auto renderPass : _vRenderPasses)
			vkDestroyRenderPass(_device, renderPass, GetAllocator());
		_vRenderPasses.clear();
	}
}

void RendererVulkan::DeleteFramebuffer(int id)
{
	if (id >= 0)
	{
		vkDestroyFramebuffer(_device, _vFramebuffers[id], GetAllocator());
		_vFramebuffers[id] = VK_NULL_HANDLE;
	}
	else
	{
		for (auto framebuffer : _vFramebuffers)
			vkDestroyFramebuffer(_device, framebuffer, GetAllocator());
		_vFramebuffers.clear();
	}
}

int RendererVulkan::GetNextRenderPassID() const
{
	const int num = Utils::Cast32(_vRenderPasses.size());
	VERUS_FOR(i, num)
	{
		if (VK_NULL_HANDLE == _vRenderPasses[i])
			return i;
	}
	return num;
}

int RendererVulkan::GetNextFramebufferID() const
{
	const int num = Utils::Cast32(_vFramebuffers.size());
	VERUS_FOR(i, num)
	{
		if (VK_NULL_HANDLE == _vFramebuffers[i])
			return i;
	}
	return num;
}

VkRenderPass RendererVulkan::GetRenderPassByID(int id) const
{
	return _vRenderPasses[id];
}

VkFramebuffer RendererVulkan::GetFramebufferByID(int id) const
{
	return _vFramebuffers[id];
}

uint32_t RendererVulkan::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties vkpdmp = {};
	vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &vkpdmp);
	for (uint32_t i = 0; i < vkpdmp.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (vkpdmp.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}
	throw VERUS_RECOVERABLE << "FindMemoryType(), Memory type not found";
}