#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

TextureVulkan::TextureVulkan()
{
}

TextureVulkan::~TextureVulkan()
{
	Done();
}

void TextureVulkan::Init(RcTextureDesc desc)
{
	VERUS_INIT();
}

void TextureVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_imageView, vkDestroyImageView(pRendererVulkan->GetVkDevice(), _imageView, pRendererVulkan->GetAllocator()));

	VERUS_DONE(TextureVulkan);
}
