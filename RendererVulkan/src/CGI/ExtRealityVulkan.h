// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererVulkan;

		class ExtRealityVulkan : public BaseExtReality
		{
			struct SwapChainEx
			{
				XrSwapchain                        _handle = XR_NULL_HANDLE;
				int32_t                            _width = 0;
				int32_t                            _height = 0;
				Vector<XrSwapchainImageVulkan2KHR> _vImages;
				Vector<VkImageView>                _vImageViews;
			};

			Vector<SwapChainEx> _vSwapChains;

		public:
			ExtRealityVulkan();
			virtual ~ExtRealityVulkan() override;

			virtual void Init() override;
			virtual void Done() override;

			VkResult CreateVulkanInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
			VkPhysicalDevice GetVulkanGraphicsDevice(VkInstance vulkanInstance);
			VkResult CreateVulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
			void InitByRenderer(RendererVulkan* pRenderer);

		private:
			void GetSystem();
			void CreateSwapChains(RendererVulkan* pRenderer, int64_t format);
			VkImageView CreateImageView(RendererVulkan* pRenderer, int64_t format, XrSwapchainImageVulkan2KHR& image);
			virtual XrSwapchain GetSwapChain(int viewIndex) override;
			virtual void GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h) override;

		public:
			VkImageView GetVkImageView(int viewIndex, int imageIndex) const;

			virtual void CreateActions() override;
			virtual void PollEvents() override;
			virtual void SyncActions(UINT32 activeActionSetsMask) override;

			virtual bool GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction) override;
			virtual bool GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction) override;
			virtual bool GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction) override;

			virtual void BeginFrame() override;
			virtual int LocateViews() override;
			virtual void BeginView(int viewIndex, RViewDesc viewDesc) override;
			virtual void AcquireSwapChainImage() override;
			virtual void EndView(int viewIndex) override;
			virtual void EndFrame() override;
		};
		VERUS_TYPEDEFS(ExtRealityVulkan);
	}
}
