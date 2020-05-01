#pragma once

namespace verus
{
	namespace CGI
	{
		struct BaseShaderInclude
		{
			virtual void Open(CSZ filename, void** ppData, UINT32* pBytes) = 0;
			virtual void Close(void* pData) = 0;
		};

		typedef Store<CommandBufferVulkan> TStoreCommandBuffers;
		typedef Store<GeometryVulkan>      TStoreGeometry;
		typedef Store<PipelineVulkan>      TStorePipelines;
		typedef Store<ShaderVulkan>        TStoreShaders;
		typedef Store<TextureVulkan>       TStoreTextures;
		class RendererVulkan : public Singleton<RendererVulkan>, public BaseRenderer,
			private TStoreCommandBuffers, private TStoreGeometry, private TStorePipelines, private TStoreShaders, private TStoreTextures
		{
		public:
			struct Framebuffer
			{
				VkFramebuffer _framebuffer = VK_NULL_HANDLE;
				int           _width = 0;
				int           _height = 0;
			};
			VERUS_TYPEDEFS(Framebuffer);

		private:
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
			VmaAllocator             _vmaAllocator = VK_NULL_HANDLE;
			VkSwapchainKHR           _swapChain = VK_NULL_HANDLE;
			Vector<VkImage>          _vSwapChainImages;
			Vector<VkImageView>      _vSwapChainImageViews;
			VkCommandPool            _commandPools[s_ringBufferSize] = {};
			VkSemaphore              _acquireNextImageSemaphores[s_ringBufferSize] = {};
			VkSemaphore              _queueSubmitSemaphores[s_ringBufferSize] = {};
			VkSemaphore              _acquireNextImageSemaphore = VK_NULL_HANDLE;
			VkSemaphore              _queueSubmitSemaphore = VK_NULL_HANDLE;
			VkFence                  _queueSubmitFences[s_ringBufferSize] = {};
			QueueFamilyIndices       _queueFamilyIndices;
			VkDescriptorPool         _descriptorPoolImGui = VK_NULL_HANDLE;
			Vector<VkSampler>        _vSamplers;
			Vector<VkRenderPass>     _vRenderPasses;
			Vector<Framebuffer>      _vFramebuffers;

		public:
			RendererVulkan();
			~RendererVulkan();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

			static void VulkanCompilerInit();
			static void VulkanCompilerDone();
			static bool VulkanCompile(CSZ source, CSZ sourceName, CSZ* defines, BaseShaderInclude* pInclude,
				CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs);

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
			void CreateSwapChain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
			void CreateImageViews();
			void CreateCommandPools();
			void CreateSyncObjects();
			void CreateSamplers();

		public:
			VkCommandBuffer CreateCommandBuffer(VkCommandPool commandPool);

			VkDevice GetVkDevice() const { return _device; }
			VkQueue GetVkGraphicsQueue() const { return _graphicsQueue; }
			const VkAllocationCallbacks* GetAllocator() const { return nullptr; }
			VmaAllocator GetVmaAllocator() const { return _vmaAllocator; }
			VkCommandPool GetVkCommandPool(int ringBufferIndex) const { return _commandPools[ringBufferIndex]; }
			const VkSampler* GetImmutableSampler(Sampler s) const;

			static void ImGuiCheckVkResultFn(VkResult res);
			virtual void ImGuiInit(RPHandle renderPassHandle) override;
			virtual void ImGuiRenderDrawData() override;

			virtual void ResizeSwapChain() override;

			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::vulkan; }

			// Frame cycle:
			virtual void BeginFrame(bool present) override;
			virtual void EndFrame(bool present) override;
			virtual void Present() override;
			virtual void WaitIdle() override;

			// Resources:
			virtual PBaseCommandBuffer InsertCommandBuffer() override;
			virtual PBaseGeometry      InsertGeometry() override;
			virtual PBasePipeline      InsertPipeline() override;
			virtual PBaseShader        InsertShader() override;
			virtual PBaseTexture       InsertTexture() override;

			virtual void DeleteCommandBuffer(PBaseCommandBuffer p) override;
			virtual void DeleteGeometry(PBaseGeometry p) override;
			virtual void DeletePipeline(PBasePipeline p) override;
			virtual void DeleteShader(PBaseShader p) override;
			virtual void DeleteTexture(PBaseTexture p) override;

			virtual RPHandle CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) override;
			virtual FBHandle CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex) override;
			virtual void DeleteRenderPass(RPHandle handle) override;
			virtual void DeleteFramebuffer(FBHandle handle) override;
			int GetNextRenderPassIndex() const;
			int GetNextFramebufferIndex() const;
			VkRenderPass GetRenderPass(RPHandle handle) const;
			RcFramebuffer GetFramebuffer(FBHandle handle) const;

			void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocation& vmaAllocation);
			void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, PBaseCommandBuffer pCB = nullptr);
			void CreateImage(const VkImageCreateInfo* pImageCreateInfo, VmaMemoryUsage vmaUsage, VkImage& image, VmaAllocation& vmaAllocation);
			void CopyImage(
				VkImage srcImage, uint32_t srcMipLevel, uint32_t srcArrayLayer,
				VkImage dstImage, uint32_t dstMipLevel, uint32_t dstArrayLayer,
				uint32_t width, uint32_t height,
				PBaseCommandBuffer pCB = nullptr);
			void CopyBufferToImage(
				VkBuffer buffer,
				VkImage image, uint32_t mipLevel, uint32_t arrayLayer,
				uint32_t width, uint32_t height,
				PBaseCommandBuffer pCB = nullptr);
		};
		VERUS_TYPEDEFS(RendererVulkan);
	}

	extern "C"
	{
		typedef void(*PFNVULKANCOMPILERINIT)();
		typedef void(*PFNVULKANCOMPILERDONE)();
		typedef bool(*PFNVULKANCOMPILE)(CSZ source, CSZ sourceName, CSZ* defines, CGI::BaseShaderInclude* pInclude,
			CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs);
	}
}

#define VERUS_QREF_RENDERER_VULKAN CGI::PRendererVulkan pRendererVulkan = CGI::RendererVulkan::P()
