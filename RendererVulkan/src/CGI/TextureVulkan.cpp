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
	VERUS_RT_ASSERT(desc._width > 0 && desc._height > 0);
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	_size = Vector4(
		float(desc._width),
		float(desc._height),
		1.f / desc._width,
		1.f / desc._height);
	_desc = desc;
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);
	const bool depthFormat = IsDepthFormat(desc._format);

	VkImageCreateInfo vkici = {};
	vkici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	vkici.imageType = desc._depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	vkici.format = ToNativeFormat(desc._format);
	vkici.extent.width = desc._width;
	vkici.extent.height = desc._height;
	vkici.extent.depth = desc._depth;
	vkici.mipLevels = desc._mipLevels;
	vkici.arrayLayers = desc._arrayLayers;
	vkici.samples = ToNativeSampleCount(desc._sampleCount);
	vkici.tiling = VK_IMAGE_TILING_OPTIMAL;
	vkici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	vkici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vkici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (depthFormat)
		vkici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	pRendererVulkan->CreateImage(&vkici, VMA_MEMORY_USAGE_GPU_ONLY, _image, _vmaAllocation);

	VkImageViewCreateInfo vkivci = {};
	vkivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vkivci.image = _image;
	vkivci.viewType = desc._depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
	vkivci.format = vkici.format;
	vkivci.subresourceRange.aspectMask = depthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	vkivci.subresourceRange.baseMipLevel = 0;
	vkivci.subresourceRange.levelCount = vkici.mipLevels;
	vkivci.subresourceRange.baseArrayLayer = 0;
	vkivci.subresourceRange.layerCount = vkici.arrayLayers;
	if (VK_SUCCESS != (res = vkCreateImageView(pRendererVulkan->GetVkDevice(), &vkivci, pRendererVulkan->GetAllocator(), &_imageView)))
		throw VERUS_RUNTIME_ERROR << "vkCreateImageView(), res=" << res;

	VkSamplerCreateInfo vksci = {};
	vksci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	vksci.magFilter = VK_FILTER_LINEAR;
	vksci.minFilter = VK_FILTER_LINEAR;
	vksci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	vksci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vksci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	vksci.anisotropyEnable = VK_TRUE;
	vksci.maxAnisotropy = 16;
	vksci.compareEnable = VK_FALSE;
	vksci.compareOp = VK_COMPARE_OP_ALWAYS;
	vksci.minLod = 0;
	vksci.maxLod = FLT_MAX;
	vksci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	if (VK_SUCCESS != (res = vkCreateSampler(pRendererVulkan->GetVkDevice(), &vksci, pRendererVulkan->GetAllocator(), &_sampler)))
		throw VERUS_RUNTIME_ERROR << "vkCreateSampler(), res=" << res;

	if (depthFormat)
	{
		CommandBufferVulkan commandBuffer;
		commandBuffer.InitSingleTimeCommands();
		commandBuffer.PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::depthStencilAttachmentOptimal, 0);
		commandBuffer.DoneSingleTimeCommands();
	}

	_vStagingBuffers.reserve(desc._mipLevels);
}

void TextureVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	_destroyStagingBuffers.Allow();
	DestroyStagingBuffers();
	VERUS_VULKAN_DESTROY(_sampler, vkDestroySampler(pRendererVulkan->GetVkDevice(), _sampler, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_imageView, vkDestroyImageView(pRendererVulkan->GetVkDevice(), _imageView, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_image, vmaDestroyImage(pRendererVulkan->GetVmaAllocator(), _image, _vmaAllocation));

	VERUS_DONE(TextureVulkan);
}

void TextureVulkan::UpdateImage(int mipLevel, const void* p, int arrayLayer, BaseCommandBuffer* pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const int w = Math::Max(1, _desc._width >> mipLevel);
	const int h = Math::Max(1, _desc._height >> mipLevel);
	VkDeviceSize bufferSize = w * h * _bytesPerPixel;
	if (IsBC(_desc._format))
		bufferSize = IO::DDSHeader::ComputeBcLevelSize(w, h, IsBC1(_desc._format));

	if (_vStagingBuffers.size() <= mipLevel)
		_vStagingBuffers.resize(mipLevel + 1);

	auto& sb = _vStagingBuffers[mipLevel];
	pRendererVulkan->CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
		sb._buffer, sb._vmaAllocation);

	void* pData = nullptr;
	if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), sb._vmaAllocation, &pData)))
		throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
	memcpy(pData, p, bufferSize);
	vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), sb._vmaAllocation);

	int iw = w;
	int ih = h;
	if (IsBC(_desc._format))
	{
		iw = Math::Max(iw, 4);
		ih = Math::Max(ih, 4);
	}
	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::transferDstOptimal, mipLevel);
	pRendererVulkan->CopyBufferToImage(sb._buffer, _image, iw, ih, mipLevel, pCB);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, mipLevel);

	_destroyStagingBuffers.Schedule();
}

void TextureVulkan::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	VERUS_QREF_RENDERER_VULKAN;
	for (auto& x : _vStagingBuffers)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));
}
