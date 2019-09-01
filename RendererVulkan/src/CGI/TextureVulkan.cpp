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
	_desc._mipLevels = desc._mipLevels ? desc._mipLevels : Math::ComputeMipLevels(desc._width, desc._height, desc._depth);
	_bytesPerPixel = FormatToBytesPerPixel(desc._format);
	const bool depthFormat = IsDepthFormat(desc._format);

	VkImageCreateInfo vkici = {};
	vkici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	vkici.imageType = _desc._depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	vkici.format = ToNativeFormat(_desc._format);
	vkici.extent.width = _desc._width;
	vkici.extent.height = _desc._height;
	vkici.extent.depth = _desc._depth;
	vkici.mipLevels = _desc._mipLevels;
	vkici.arrayLayers = _desc._arrayLayers;
	vkici.samples = ToNativeSampleCount(_desc._sampleCount);
	vkici.tiling = VK_IMAGE_TILING_OPTIMAL;
	vkici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	vkici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vkici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (depthFormat)
		vkici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	pRendererVulkan->CreateImage(&vkici, VMA_MEMORY_USAGE_GPU_ONLY, _image, _vmaAllocation);

	if (_desc._generateMips)
	{
		vkici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		pRendererVulkan->CreateImage(&vkici, VMA_MEMORY_USAGE_GPU_ONLY, _imageStorage, _vmaAllocationStorage);

		_vImageViewsStorage.resize(_desc._mipLevels);
		VERUS_FOR(mip, _desc._mipLevels)
		{
			VkImageViewCreateInfo vkivci = {};
			vkivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			vkivci.image = _imageStorage;
			vkivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			vkivci.format = vkici.format;
			vkivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkivci.subresourceRange.baseMipLevel = mip;
			vkivci.subresourceRange.levelCount = 1;
			vkivci.subresourceRange.baseArrayLayer = 0;
			vkivci.subresourceRange.layerCount = 1;
			if (VK_SUCCESS != (res = vkCreateImageView(pRendererVulkan->GetVkDevice(), &vkivci, pRendererVulkan->GetAllocator(), &_vImageViewsStorage[mip])))
				throw VERUS_RUNTIME_ERROR << "vkCreateImageView(), res=" << res;
		}
	}

	VkImageViewCreateInfo vkivci = {};
	vkivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vkivci.image = _image;
	vkivci.viewType = _desc._depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
	if (_desc._arrayLayers > 1)
		vkivci.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
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
		commandBuffer.PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::depthStencilAttachmentOptimal, 0, 0);
		commandBuffer.DoneSingleTimeCommands();
	}

	_vStagingBuffers.reserve(_desc._mipLevels*_desc._arrayLayers);
}

void TextureVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	_destroyStagingBuffers.Allow();
	DestroyStagingBuffers();
	VERUS_VULKAN_DESTROY(_sampler, vkDestroySampler(pRendererVulkan->GetVkDevice(), _sampler, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_imageView, vkDestroyImageView(pRendererVulkan->GetVkDevice(), _imageView, pRendererVulkan->GetAllocator()));
	for (auto view : _vImageViewsStorage)
		VERUS_VULKAN_DESTROY(view, vkDestroyImageView(pRendererVulkan->GetVkDevice(), view, pRendererVulkan->GetAllocator()));
	_vImageViewsStorage.clear();
	VERUS_VULKAN_DESTROY(_imageStorage, vmaDestroyImage(pRendererVulkan->GetVmaAllocator(), _imageStorage, _vmaAllocationStorage));
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
	VkDeviceSize bufferSize = _bytesPerPixel * w * h;
	if (IsBC(_desc._format))
		bufferSize = IO::DDSHeader::ComputeBcLevelSize(w, h, IsBC1(_desc._format));

	const int sbIndex = arrayLayer * _desc._mipLevels + mipLevel;
	if (_vStagingBuffers.size() <= sbIndex)
		_vStagingBuffers.resize(sbIndex + 1);

	auto& sb = _vStagingBuffers[sbIndex];
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
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::transferDstOptimal, mipLevel, arrayLayer);
	pRendererVulkan->CopyBufferToImage(sb._buffer, _image, iw, ih, mipLevel, arrayLayer, pCB);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, mipLevel, arrayLayer);

	_destroyStagingBuffers.Schedule();
}

void TextureVulkan::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._generateMips);
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());

	auto tex = TexturePtr::From(this);

	std::swap(_image, _imageStorage);
	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::undefined, ImageLayout::general, Range<int>(0, _desc._mipLevels - 1));
	std::swap(_image, _imageStorage);
	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::undefined, ImageLayout::shaderReadOnlyOptimal, Range<int>(1, _desc._mipLevels - 1));

	pCB->BindPipeline(renderer.GetPipelineGenerateMips());

	auto shader = renderer.GetShaderGenerateMips();
	shader->BeginBindDescriptors();

	auto& ub = renderer.GetUbGenerateMips();
	ub._isSRGB = false;

	for (int srcMip = 0; srcMip < _desc._mipLevels - 1;)
	{
		const int srcWidth = _desc._width >> srcMip;
		const int srcHeight = _desc._height >> srcMip;
		const int dstWidth = Math::Max(1, srcWidth >> 1);
		const int dstHeight = Math::Max(1, srcHeight >> 1);

		ub._srcDimensionCase = (srcHeight & 1) << 1 | (srcWidth & 1);

		int numMips = 4;
		numMips = Math::Min(4, numMips + 1);
		numMips = ((srcMip + numMips) >= _desc._mipLevels) ? _desc._mipLevels - srcMip - 1 : numMips;

		ub._srcMipLevel = srcMip;
		ub._numMipLevels = numMips;
		ub._texelSize.x = 1.f / dstWidth;
		ub._texelSize.y = 1.f / dstHeight;

		int mips[5] = {};
		VERUS_FOR(mip, numMips)
			mips[mip + 1] = srcMip + mip + 1;

		const int complexDescSetID = shader->BindDescriptorSetTextures(0, { tex, tex, tex, tex, tex }, mips);

		pCB->BindDescriptors(shader, 0, complexDescSetID);

		pCB->Dispatch(Math::DivideByMultiple(dstWidth, 8), Math::DivideByMultiple(dstHeight, 8));

		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::shaderReadOnlyOptimal, ImageLayout::transferDstOptimal, Range<int>(srcMip + 1, srcMip + numMips));
		std::swap(_image, _imageStorage);
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::general, ImageLayout::transferSrcOptimal, Range<int>(srcMip + 1, srcMip + numMips));
		std::swap(_image, _imageStorage);
		VERUS_FOR(mip, numMips)
		{
			const int dstMip = srcMip + 1 + mip;

			const int w = _desc._width >> dstMip;
			const int h = _desc._height >> dstMip;
			pRendererVulkan->CopyImage(_imageStorage, _image, w, h, dstMip, 0, pCB);
		}
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDstOptimal, ImageLayout::shaderReadOnlyOptimal, Range<int>(srcMip + 1, srcMip + numMips));

		srcMip += numMips;
	}

	shader->EndBindDescriptors();
}

void TextureVulkan::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	VERUS_QREF_RENDERER_VULKAN;
	for (auto& x : _vStagingBuffers)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));
}
