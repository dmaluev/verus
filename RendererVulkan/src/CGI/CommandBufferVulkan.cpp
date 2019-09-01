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
	vkcbbi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if (VK_SUCCESS != (res = vkBeginCommandBuffer(GetVkCommandBuffer(), &vkcbbi)))
		throw VERUS_RUNTIME_ERROR << "vkBeginCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::End()
{
	VkResult res = VK_SUCCESS;
	if (VK_SUCCESS != (res = vkEndCommandBuffer(GetVkCommandBuffer())))
		throw VERUS_RUNTIME_ERROR << "vkEndCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, PcVector4 pRenderArea)
{
	VERUS_QREF_RENDERER_VULKAN;
	VERUS_QREF_CONST_SETTINGS;
	VkRenderPassBeginInfo vkrpbi = {};
	vkrpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	vkrpbi.renderPass = pRendererVulkan->GetRenderPassByID(renderPassID);
	vkrpbi.framebuffer = pRendererVulkan->GetFramebufferByID(framebufferID);
	if (pRenderArea)
	{
		vkrpbi.renderArea.offset.x = static_cast<int32_t>(pRenderArea->getX());
		vkrpbi.renderArea.offset.y = static_cast<int32_t>(pRenderArea->getY());
		vkrpbi.renderArea.extent.width = static_cast<uint32_t>(pRenderArea->Width());
		vkrpbi.renderArea.extent.height = static_cast<uint32_t>(pRenderArea->Height());
	}
	else
	{
		vkrpbi.renderArea.extent.width = settings._screenSizeWidth;
		vkrpbi.renderArea.extent.height = settings._screenSizeHeight;
	}
	VkClearValue clearValue[VERUS_MAX_NUM_RT] = {};
	int num = 0;
	for (const auto& x : ilClearValues)
	{
		memcpy(clearValue[num].color.float32, x.ToPointer(), sizeof(clearValue[num].color.float32));
		num++;
	}
	vkrpbi.clearValueCount = num;
	vkrpbi.pClearValues = clearValue;
	vkCmdBeginRenderPass(GetVkCommandBuffer(), &vkrpbi, VK_SUBPASS_CONTENTS_INLINE);
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
		vpVulkan[num].y = rc.getY();
		vpVulkan[num].width = rc.Width();
		vpVulkan[num].height = rc.Height();
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
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
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
			vkimb[index].srcAccessMask = 0;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			vkimb[index].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			vkimb[index].srcAccessMask = 0;
			vkimb[index].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			vkimb[index].srcAccessMask = 0;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			vkimb[index].srcAccessMask = 0;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_GENERAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			vkimb[index].srcAccessMask = 0;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			vkimb[index].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (vkimb[index].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && vkimb[index].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			vkimb[index].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			vkimb[index].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
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

void CommandBufferVulkan::Clear(ClearFlags clearFlags)
{
	VERUS_QREF_RENDERER;
	if (clearFlags & ClearFlags::color)
	{
		VkClearColorValue clearColorValue;
		memcpy(&clearColorValue, renderer.GetClearColor().ToPointer(), sizeof(clearColorValue));
		VkImageSubresourceRange vkisr = {};
		vkisr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkisr.baseMipLevel = 0;
		vkisr.levelCount = 1;
		vkisr.baseArrayLayer = 0;
		vkisr.layerCount = 1;
		vkCmdClearColorImage(GetVkCommandBuffer(), nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &vkisr);
	}
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

void CommandBufferVulkan::InitSingleTimeCommands()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	auto commandPool = pRendererVulkan->GetVkCommandPool(renderer->GetRingBufferIndex());
	auto commandBuffer = pRendererVulkan->CreateCommandBuffer(commandPool);
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_commandBuffers[i] = commandBuffer;
	Begin();
}

void CommandBufferVulkan::DoneSingleTimeCommands()
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
}
