#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererVulkan : public Singleton<RendererVulkan>, public BaseRenderer
		{
			struct QueueFamilyIndices
			{
				int _graphicsFamilyIndex = -1;
				int _presentFamilyIndex = -1;

				bool IsComplete() const
				{
					return _graphicsFamilyIndex >= 0 && _presentFamilyIndex >= 0;
				}

				bool IsSameQueue() const
				{
					return _graphicsFamilyIndex == _presentFamilyIndex;
				}
			};

			struct SwapChainInfo
			{
				VkSurfaceCapabilitiesKHR   _surfaceCapabilities = {};
				Vector<VkSurfaceFormatKHR> _vSurfaceFormats;
				Vector<VkPresentModeKHR>   _vSurfacePresentModes;
			};

			static CSZ s_requiredValidationLayers[];
			static CSZ s_requiredDeviceExtensions[];

			VkInstance               _instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT _debugUtilsMessenger = VK_NULL_HANDLE;
			VkSurfaceKHR             _surface = VK_NULL_HANDLE;
			VkPhysicalDevice         _physicalDevice = VK_NULL_HANDLE;
			VkDevice                 _device = VK_NULL_HANDLE;
			VkQueue                  _graphicsQueue = VK_NULL_HANDLE;
			VkQueue                  _presentQueue = VK_NULL_HANDLE;
			VkSwapchainKHR           _swapChain = VK_NULL_HANDLE;
			Vector<VkImage>          _vSwapChainImages;
			Vector<VkImageView>      _vSwapChainImageViews;
			VkCommandPool            _commandPools[ringBufferSize] = { VK_NULL_HANDLE };
			VkCommandBuffer          _commandBuffers[ringBufferSize] = { VK_NULL_HANDLE };
			VkSemaphore              _acquireNextImageSemaphores[ringBufferSize] = { VK_NULL_HANDLE };
			VkSemaphore              _queueSubmitSemaphores[ringBufferSize] = { VK_NULL_HANDLE };
			VkFence                  _queueSubmitFences[ringBufferSize] = { VK_NULL_HANDLE };
			QueueFamilyIndices       _queueFamilyIndices;

		public:
			RendererVulkan();
			~RendererVulkan();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

		private:
			static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageTypes,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData);
			bool CheckRequiredValidationLayers();
			Vector<CSZ> GetRequiredExtensions();
			void CreateInstance();
			void CreateDebugUtilsMessenger();
			void CreateSurface();
			bool CheckRequiredDeviceExtensions(VkPhysicalDevice device);
			QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice device);
			bool IsDeviceSuitable(VkPhysicalDevice device);
			void PickPhysicalDevice();
			void CreateDevice();
			SwapChainInfo GetSwapChainInfo(VkPhysicalDevice device);
			void CreateSwapChain();
			void CreateImageViews();
			void CreateCommandPools();
			void CreateCommandBuffers();
			void CreateSyncObjects();

		public:
			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::vulkan; }

			// Frame cycle:
			virtual void BeginFrame() override;
			virtual void EndFrame() override;
			virtual void Present() override;
			virtual void Clear(UINT32 flags) override;
		};
		VERUS_TYPEDEFS(RendererVulkan);
	}
}

#define VERUS_QREF_RENDERER_VULKAN CGI::PRendererVulkan pRendererVulkan = CGI::RendererVulkan::P()
