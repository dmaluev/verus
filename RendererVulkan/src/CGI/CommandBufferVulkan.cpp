#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

CommandBufferVulkan::CommandBufferVulkan()
{
}

CommandBufferVulkan::~CommandBufferVulkan()
{
	Done();
}

void CommandBufferVulkan::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_commandBuffers[i] = pRendererVulkan->CreateCommandBuffer(pRendererVulkan->GetVkCommandPool(i));
}

void CommandBufferVulkan::Done()
{
	VERUS_DONE(CommandBufferVulkan);
}

void CommandBufferVulkan::Begin()
{
	VkResult res = VK_SUCCESS;
	VkCommandBufferBeginInfo vkcbbi = {};
	vkcbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (_oneTimeSubmit)
		vkcbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (VK_SUCCESS != (res = vkBeginCommandBuffer(GetVkCommandBuffer(), &vkcbbi)))
		throw VERUS_RUNTIME_ERROR << "vkBeginCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::End()
{
	VkResult res = VK_SUCCESS;
	if (VK_SUCCESS != (res = vkEndCommandBuffer(GetVkCommandBuffer())))
		throw VERUS_RUNTIME_ERROR << "vkEndCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, bool setViewportAndScissor)
{
	VERUS_QREF_RENDERER_VULKAN;
	VERUS_QREF_CONST_SETTINGS;
	RendererVulkan::RcFramebuffer framebuffer = pRendererVulkan->GetFramebufferByID(framebufferID);
	VkRenderPassBeginInfo vkrpbi = {};
	vkrpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	vkrpbi.renderPass = pRendererVulkan->GetRenderPassByID(renderPassID);
	vkrpbi.framebuffer = framebuffer._framebuffer;
	vkrpbi.renderArea.extent.width = framebuffer._width;
	vkrpbi.renderArea.extent.height = framebuffer._height;
	VkClearValue clearValue[VERUS_MAX_NUM_FB_ATTACH] = {};
	int num = 0;
	for (const auto& x : ilClearValues)
	{
		memcpy(clearValue[num].color.float32, x.ToPointer(), sizeof(clearValue[num].color.float32));
		num++;
	}
	vkrpbi.clearValueCount = num;
	vkrpbi.pClearValues = clearValue;
	vkCmdBeginRenderPass(GetVkCommandBuffer(), &vkrpbi, VK_SUBPASS_CONTENTS_INLINE);
	if (setViewportAndScissor)
	{
		const Vector4 rc(0, 0, static_cast<float>(framebuffer._width), static_cast<float>(framebuffer._height));
		SetViewport({ rc }, 0, 1);
		SetScissor({ rc });
	}
}

void CommandBufferVulkan::NextSubpass()
{
	vkCmdNextSubpass(GetVkCommandBuffer(), VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBufferVulkan::EndRenderPass()
{
	vkCmdEndRenderPass(GetVkCommandBuffer());
}

void CommandBufferVulkan::BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter)
{
	auto& geoVulkan = static_cast<RGeometryVulkan>(*geo);
	geoVulkan.DestroyStagingBuffers();
	VkBuffer buffers[VERUS_MAX_NUM_VB];
	VkDeviceSize offsets[VERUS_MAX_NUM_VB];
	const int num = geoVulkan.GetNumVertexBuffers();
	int at = 0;
	VERUS_FOR(i, num)
	{
		if ((bindingsFilter >> i) & 0x1)
		{
			buffers[at] = geoVulkan.GetVkVertexBuffer(i);
			offsets[at] = geoVulkan.GetVkVertexBufferOffset(i);
			at++;
		}
	}
	vkCmdBindVertexBuffers(GetVkCommandBuffer(), 0, at, buffers, offsets);
}

void CommandBufferVulkan::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoVulkan = static_cast<RGeometryVulkan>(*geo);
	VkBuffer buffer = geoVulkan.GetVkIndexBuffer();
	VkDeviceSize offset = 0;
	vkCmdBindIndexBuffer(GetVkCommandBuffer(), buffer, offset, geoVulkan.Has32BitIndices() ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void CommandBufferVulkan::BindPipeline(PipelinePtr pipe)
{
	auto& pipeVulkan = static_cast<RPipelineVulkan>(*pipe);
	vkCmdBindPipeline(GetVkCommandBuffer(),
		pipeVulkan.IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeVulkan.GetVkPipeline());
}

void CommandBufferVulkan::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	VkViewport vpVulkan[VERUS_MAX_NUM_RT];
	int num = 0;
	for (const auto& rc : il)
	{
		vpVulkan[num].x = rc.getX();
		vpVulkan[num].y = rc.getY() + rc.Height();
		vpVulkan[num].width = rc.Width();
		vpVulkan[num].height = -rc.Height();
		vpVulkan[num].minDepth = minDepth;
		vpVulkan[num].maxDepth = maxDepth;
		num++;
	}
	vkCmdSetViewport(GetVkCommandBuffer(), 0, num, vpVulkan);
}

void CommandBufferVulkan::SetScissor(std::initializer_list<Vector4> il)
{
	VkRect2D rcVulkan[VERUS_MAX_NUM_RT];
	int num = 0;
	for (const auto& rc : il)
	{
		rcVulkan[num].offset.x = static_cast<int32_t>(rc.getX());
		rcVulkan[num].offset.y = static_cast<int32_t>(rc.getY());
		rcVulkan[num].extent.width = static_cast<uint32_t>(rc.Width());
		rcVulkan[num].extent.height = static_cast<uint32_t>(rc.Height());
		num++;
	}
	vkCmdSetScissor(GetVkCommandBuffer(), 0, num, rcVulkan);
}

void CommandBufferVulkan::SetBlendConstants(const float* p)
{
	vkCmdSetBlendConstants(GetVkCommandBuffer(), p);
}

bool CommandBufferVulkan::BindDescriptors(ShaderPtr shader, int setNumber, int complexSetID)
{
	if (setNumber < 0)
		return true;

	auto& shaderVulkan = static_cast<RShaderVulkan>(*shader);
	if (shaderVulkan.TryPushConstants(setNumber, *this))
		return true;
	const int offset = shaderVulkan.UpdateUniformBuffer(setNumber);
	if (offset < 0)
		return false;

	const VkDescriptorSet descriptorSet = (complexSetID >= 0) ?
		shaderVulkan.GetComplexVkDescriptorSet(complexSetID) : shaderVulkan.GetVkDescriptorSet(setNumber);
	const uint32_t dynamicOffset = offset;
	const uint32_t dynamicOffsetCount = 1;
	vkCmdBindDescriptorSets(GetVkCommandBuffer(),
		shaderVulkan.IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		shaderVulkan.GetVkPipelineLayout(),
		setNumber, 1, &descriptorSet, dynamicOffsetCount, &dynamicOffset);
	return true;
}

void CommandBufferVulkan::PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags)
{
	auto& shaderVulkan = static_cast<RShaderVulkan>(*shader);
	const VkShaderStageFlags vkssf = ToNativeStageFlags(stageFlags);
	vkCmdPushConstants(GetVkCommandBuffer(), shaderVulkan.GetVkPipelineLayout(), vkssf, offset << 2, size << 2, p);
}

void CommandBufferVulkan::PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range<int> mipLevels, int arrayLayer)
{
	auto& texVulkan = static_cast<RTextureVulkan>(*tex);
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Waiting for this stage to finish (TOP_OF_PIPE means wait for nothing).
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Which stage is waiting to start (BOTTOM_OF_PIPE means nothing is waiting).
	const VkPipelineStageFlags dstStageMaskVS = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	VkImageMemoryBarrier vkimb[16];
	VERUS_RT_ASSERT(mipLevels.GetRange() < VERUS_ARRAY_LENGTH(vkimb));
	int index = 0;
	for (int mip : mipLevels)
	{
		vkimb[index].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		vkimb[index].pNext = nullptr;
		vkimb[index].oldLayout = ToNativeImageLayout(oldLayout);
		vkimb[index].newLayout = ToNativeImageLayout(newLayout);
		vkimb[index].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkimb[index].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkimb[index].image = texVulkan.GetVkImage();
		vkimb[index].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkimb[index].subresourceRange.baseMipLevel = mip;
		vkimb[index].subresourceRange.levelCount = 1;
		vkimb[index].subresourceRange.baseArrayLayer = arrayLayer;
		vkimb[index].subresourceRange.layerCount = 1;

		// See: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
		if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			// Storage image (UAV) initialization.
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			// Depth texture initialization.
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			vkimb[index].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (Format::unormD24uintS8 == tex->GetFormat())
				vkimb[index].subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		{
			// Shadow map initialization.
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkimb[index].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (Format::unormD24uintS8 == tex->GetFormat())
				vkimb[index].subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			// Render target initialization.
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = (ImageLayout::vsReadOnly == newLayout) ? dstStageMaskVS : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			// Regular texture's first and only update (before transfer).
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_GENERAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			// To transfer data from storage image during mipmap generation.
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			// To support vertex texture fetch.
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = (ImageLayout::vsReadOnly == newLayout) ? dstStageMaskVS : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			// To update regular texture during mipmap generation.
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].srcAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			// Regular texture's first and only update (after transfer).
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb[index].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStageMask = (ImageLayout::vsReadOnly == newLayout) ? dstStageMaskVS : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else
		{
			VERUS_RT_FAIL("");
		}

		index++;
	}
	vkCmdPipelineBarrier(GetVkCommandBuffer(), srcStageMask, dstStageMask, 0,
		0, nullptr,
		0, nullptr,
		index, vkimb);
}

void CommandBufferVulkan::Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
	vkCmdDraw(GetVkCommandBuffer(), vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferVulkan::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
	vkCmdDrawIndexed(GetVkCommandBuffer(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferVulkan::Dispatch(int groupCountX, int groupCountY, int groupCountZ)
{
	vkCmdDispatch(GetVkCommandBuffer(), groupCountX, groupCountY, groupCountZ);
}

VkCommandBuffer CommandBufferVulkan::GetVkCommandBuffer() const
{
	VERUS_QREF_RENDERER;
	return _commandBuffers[renderer->GetRingBufferIndex()];
}

void CommandBufferVulkan::InitOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	_oneTimeSubmit = true;
	auto commandPool = pRendererVulkan->GetVkCommandPool(renderer->GetRingBufferIndex());
	auto commandBuffer = pRendererVulkan->CreateCommandBuffer(commandPool);
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_commandBuffers[i] = commandBuffer;
	Begin();
}

void CommandBufferVulkan::DoneOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	End();
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = _commandBuffers;
	vkQueueSubmit(pRendererVulkan->GetVkGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(pRendererVulkan->GetVkGraphicsQueue());
	auto commandPool = pRendererVulkan->GetVkCommandPool(renderer->GetRingBufferIndex());
	vkFreeCommandBuffers(pRendererVulkan->GetVkDevice(), commandPool, 1, _commandBuffers);
	_oneTimeSubmit = false;
}
