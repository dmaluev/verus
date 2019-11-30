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
	const bool renderTarget = (_desc._flags & TextureDesc::Flags::colorAttachment);
	const bool depthFormat = IsDepthFormat(desc._format);
	const bool inputAttach = (_desc._flags & TextureDesc::Flags::inputAttachment);

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
	if (renderTarget)
		vkici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (depthFormat)
	{
		vkici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		if (_desc._flags & TextureDesc::Flags::depthSampled)
			vkici.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (inputAttach)
		vkici.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	pRendererVulkan->CreateImage(&vkici, VMA_MEMORY_USAGE_GPU_ONLY, _image, _vmaAllocation);

	if (_desc._flags & TextureDesc::Flags::generateMips)
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

	if (_desc._pSamplerDesc)
		CreateSampler();

	if (renderTarget)
	{
		CommandBufferVulkan commandBuffer;
		commandBuffer.InitOneTimeSubmit();
		commandBuffer.PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::fsReadOnly, 0, 0);
		commandBuffer.DoneOneTimeSubmit();
	}
	if (depthFormat)
	{
		const ImageLayout layout = (_desc._flags & TextureDesc::Flags::depthSampled) ?
			ImageLayout::depthStencilReadOnly :
			ImageLayout::depthStencilAttachment;
		CommandBufferVulkan commandBuffer;
		commandBuffer.InitOneTimeSubmit();
		commandBuffer.PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, layout, 0, 0);
		commandBuffer.DoneOneTimeSubmit();
	}

	_vStagingBuffers.reserve(_desc._mipLevels * _desc._arrayLayers);
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

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::undefined, ImageLayout::transferDst, mipLevel, arrayLayer);
	pRendererVulkan->CopyBufferToImage(sb._buffer, _image, w, h, mipLevel, arrayLayer, pCB);
	pCB->PipelineImageMemoryBarrier(TexturePtr::From(this), ImageLayout::transferDst, ImageLayout::fsReadOnly, mipLevel, arrayLayer);

	_destroyStagingBuffers.Schedule();
}

void TextureVulkan::GenerateMips(PBaseCommandBuffer pCB)
{
	VERUS_RT_ASSERT(_desc._flags & TextureDesc::Flags::generateMips);
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());

	auto tex = TexturePtr::From(this);

	std::swap(_image, _imageStorage);
	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::undefined, ImageLayout::general, Range<int>(0, _desc._mipLevels - 1));
	std::swap(_image, _imageStorage);
	pCB->PipelineImageMemoryBarrier(tex, ImageLayout::undefined, ImageLayout::vsReadOnly, Range<int>(1, _desc._mipLevels - 1));

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

		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::vsReadOnly, ImageLayout::transferDst, Range<int>(srcMip + 1, srcMip + numMips));
		std::swap(_image, _imageStorage);
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::general, ImageLayout::transferSrc, Range<int>(srcMip + 1, srcMip + numMips));
		std::swap(_image, _imageStorage);
		VERUS_FOR(mip, numMips)
		{
			const int dstMip = srcMip + 1 + mip;

			const int w = _desc._width >> dstMip;
			const int h = _desc._height >> dstMip;
			pRendererVulkan->CopyImage(_imageStorage, _image, w, h, dstMip, 0, pCB);
		}
		pCB->PipelineImageMemoryBarrier(tex, ImageLayout::transferDst, ImageLayout::fsReadOnly, Range<int>(srcMip + 1, srcMip + numMips));

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

void TextureVulkan::CreateSampler()
{
	VERUS_QREF_RENDERER_VULKAN;
	VERUS_QREF_CONST_SETTINGS;
	VkResult res = VK_SUCCESS;

	const bool tf = settings._gpuTrilinearFilter;

	VkSamplerCreateInfo vksci = {};
	vksci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	if ('a' == _desc._pSamplerDesc->_filterMagMinMip[0])
	{
		if (settings._gpuAnisotropyLevel > 0)
			vksci.anisotropyEnable = VK_TRUE;
		vksci.magFilter = VK_FILTER_LINEAR;
		vksci.minFilter = VK_FILTER_LINEAR;
		vksci.mipmapMode = tf ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
	else if ('s' == _desc._pSamplerDesc->_filterMagMinMip[0]) // Shadow map:
	{
		vksci.compareEnable = VK_TRUE;
		vksci.compareOp = VK_COMPARE_OP_LESS;
		if (settings._sceneShadowQuality <= App::Settings::ShadowQuality::nearest)
		{
			vksci.magFilter = VK_FILTER_NEAREST;
			vksci.minFilter = VK_FILTER_NEAREST;
		}
		else
		{
			vksci.magFilter = VK_FILTER_LINEAR;
			vksci.minFilter = VK_FILTER_LINEAR;
		}
	}
	else
	{
		VERUS_FOR(i, 2)
		{
			VkFilter filter = VK_FILTER_NEAREST;
			switch (_desc._pSamplerDesc->_filterMagMinMip[i])
			{
			case 'n': filter = VK_FILTER_NEAREST; break;
			case 'l': filter = VK_FILTER_LINEAR; break;
			}
			switch (i)
			{
			case 0: vksci.magFilter = filter; break;
			case 1: vksci.minFilter = filter; break;
			}
		}
		vksci.mipmapMode = tf ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	}
	vksci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vksci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	vksci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VERUS_FOR(i, 3)
	{
		if (!_desc._pSamplerDesc->_addressModeUVW[i])
			break;
		VkSamplerAddressMode sam = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		switch (_desc._pSamplerDesc->_addressModeUVW[i])
		{
		case 'r': sam = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
		case 'm': sam = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; break;
		case 'c': sam = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
		case 'b': sam = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
		}
		switch (i)
		{
		case 0: vksci.addressModeU = sam; break;
		case 1: vksci.addressModeV = sam; break;
		case 2: vksci.addressModeW = sam; break;
		}
	}
	vksci.mipLodBias = _desc._pSamplerDesc->_mipLodBias;
	vksci.maxAnisotropy = static_cast<float>(settings._gpuAnisotropyLevel);
	vksci.minLod = _desc._pSamplerDesc->_minLod;
	vksci.maxLod = _desc._pSamplerDesc->_maxLod;
	if (_desc._pSamplerDesc->_borderColor.getX() < 0.5f)
	{
		if (_desc._pSamplerDesc->_borderColor.getW() < 0.5f)
			vksci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
		else
			vksci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	}
	else
		vksci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	if (VK_SUCCESS != (res = vkCreateSampler(pRendererVulkan->GetVkDevice(), &vksci, pRendererVulkan->GetAllocator(), &_sampler)))
		throw VERUS_RUNTIME_ERROR << "vkCreateSampler(), res=" << res;

	_desc._pSamplerDesc = nullptr;
}
