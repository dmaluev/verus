#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureVulkan : public BaseTexture
		{
			struct VkBufferEx
			{
				VkBuffer      _buffer = VK_NULL_HANDLE;
				VmaAllocation _vmaAllocation = VK_NULL_HANDLE;
			};

			VkImage             _image = VK_NULL_HANDLE;
			VmaAllocation       _vmaAllocation = VK_NULL_HANDLE;
			VkImage             _storageImage = VK_NULL_HANDLE;
			VmaAllocation       _storageVmaAllocation = VK_NULL_HANDLE;
			VkImageView         _imageView = VK_NULL_HANDLE;
			VkSampler           _sampler = VK_NULL_HANDLE;
			Vector<VkImageView> _vStorageImageViews;
			Vector<VkBufferEx>  _vStagingBuffers;
			DestroyStaging      _destroyStagingBuffers;

		public:
			TextureVulkan();
			virtual ~TextureVulkan() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			virtual void UpdateImage(int mipLevel, const void* p, int arrayLayer, BaseCommandBuffer* pCB) override;

			virtual void GenerateMips(PBaseCommandBuffer pCB) override;

			//
			// Vulkan
			//

			void DestroyStagingBuffers();

			void CreateSampler();

			VkImage GetVkImage() const { return _image; }
			VkImageView GetVkImageView() const { return _imageView; }
			VkImageView GetStorageVkImageView(int mip) const { return _vStorageImageViews[mip]; }
			VkSampler GetVkSampler() const { return _sampler; }
		};
		VERUS_TYPEDEFS(TextureVulkan);
	}
}
