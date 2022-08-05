// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureVulkan : public BaseTexture
		{
			struct BufferEx
			{
				VkBuffer      _buffer = VK_NULL_HANDLE;
				VmaAllocation _vmaAllocation = VK_NULL_HANDLE;
			};

			VkImage             _image = VK_NULL_HANDLE;
			VmaAllocation       _vmaAllocation = VK_NULL_HANDLE;
			VkImage             _storageImage = VK_NULL_HANDLE;
			VmaAllocation       _storageVmaAllocation = VK_NULL_HANDLE;
			VkImageView         _imageView = VK_NULL_HANDLE;
			VkImageView         _imageViewForFramebuffer[+CubeMapFace::count] = {};
			VkSampler           _sampler = VK_NULL_HANDLE;
			Vector<UINT32>      _vDefinedSubresources;
			Vector<VkImageView> _vStorageImageViews;
			Vector<BufferEx>    _vStagingBuffers;
			Vector<BufferEx>    _vReadbackBuffers;
			Vector<CSHandle>    _vCshGenerateMips;
			bool                _definedStorage = false;

		public:
			TextureVulkan();
			virtual ~TextureVulkan() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			virtual void UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB) override;
			virtual bool ReadbackSubresource(void* p, bool recordCopyCommand, PBaseCommandBuffer pCB) override;

			virtual void GenerateMips(PBaseCommandBuffer pCB) override;
			void GenerateCubeMapMips(PBaseCommandBuffer pCB);

			virtual Continue Scheduled_Update() override;

			//
			// Vulkan
			//

			void CreateSampler();

			VkImage GetVkImage() const { return _image; }
			VkImageView GetVkImageView() const { return _imageView; }
			VkImageView GetVkImageViewForFramebuffer(CubeMapFace face) const;
			VkImageView GetStorageVkImageView(int mipLevel, int arrayLayer) const { return _vStorageImageViews[mipLevel + arrayLayer * (_desc._mipLevels - 1)]; }
			VkSampler GetVkSampler() const { return _sampler; }
			ImageLayout GetSubresourceMainLayout(int mipLevel, int arrayLayer) const;
			void MarkSubresourceDefined(int mipLevel, int arrayLayer);

			static VkFormat RemoveSRGB(VkFormat format);
		};
		VERUS_TYPEDEFS(TextureVulkan);
	}
}
