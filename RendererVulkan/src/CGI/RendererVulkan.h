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

			typedef Map<String, VkRenderPass> TMapRenderPasses;

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
			VkCommandPool            _commandPools[s_ringBufferSize] = { VK_NULL_HANDLE };
			VkCommandBuffer          _commandBuffers[s_ringBufferSize] = { VK_NULL_HANDLE };
			VkSemaphore              _acquireNextImageSemaphores[s_ringBufferSize] = { VK_NULL_HANDLE };
			VkSemaphore              _queueSubmitSemaphores[s_ringBufferSize] = { VK_NULL_HANDLE };
			VkFence                  _queueSubmitFences[s_ringBufferSize] = { VK_NULL_HANDLE };
			QueueFamilyIndices       _queueFamilyIndices;
			TMapRenderPasses         _mapRenderPasses;

		public:
			RendererVulkan();
			~RendererVulkan();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

			static void VerusCompilerInit();
			static void VerusCompilerDone();
			static bool VerusCompile(CSZ source, CSZ* defines, CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs);

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
			VkDevice GetDevice() const { return _device; }
			const VkAllocationCallbacks* GetAllocator() const { return nullptr; }

			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::vulkan; }

			// Frame cycle:
			virtual void BeginFrame() override;
			virtual void EndFrame() override;
			virtual void Present() override;
			virtual void Clear(UINT32 flags) override;

			// Resources:
			virtual PBaseCommandBuffer InsertCommandBuffer() override { return nullptr; }
			virtual PBaseGeometry      InsertGeometry() override { return nullptr; }
			virtual PBasePipeline      InsertPipeline() override { return nullptr; }
			virtual PBaseShader        InsertShader() override { return nullptr; }
			virtual PBaseTexture       InsertTexture() override { return nullptr; }

			virtual void DeleteCommandBuffer(PBaseCommandBuffer p) override {}
			virtual void DeleteGeometry(PBaseGeometry p) override {}
			virtual void DeletePipeline(PBasePipeline p) override {}
			virtual void DeleteShader(PBaseShader p) override {}
			virtual void DeleteTexture(PBaseTexture p) override {}

			void CreateRenderPass(CSZ name, std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD);
		};
		VERUS_TYPEDEFS(RendererVulkan);
	}

	extern "C"
	{
		typedef void(*PFNVERUSCOMPILERINIT)();
		typedef void(*PFNVERUSCOMPILERDONE)();
		typedef bool(*PFNVERUSCOMPILE)(CSZ source, CSZ* defines, CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs);
	}
}

#define VERUS_QREF_RENDERER_VULKAN CGI::PRendererVulkan pRendererVulkan = CGI::RendererVulkan::P()
