#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureVulkan : public BaseTexture
		{
			VkImage     _image = VK_NULL_HANDLE;
			VkImageView _imageView = VK_NULL_HANDLE;

		public:
			TextureVulkan();
			virtual ~TextureVulkan() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			//
			// Vulkan
			//

			VkImage GetVkImage() const { return _image; }
			VkImageView GetVkImageView() const { return _imageView; }
		};
		VERUS_TYPEDEFS(TextureVulkan);
	}
}
