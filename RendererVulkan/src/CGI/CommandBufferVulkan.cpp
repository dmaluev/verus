#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

VkCommandBuffer CommandBufferVulkan::GetCommandBuffer() const
{
	VERUS_QREF_RENDERER;
	return _commandBuffers[renderer->GetRingBufferIndex()];
}

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
	if (VK_SUCCESS != (res = vkBeginCommandBuffer(GetCommandBuffer(), &vkcbbi)))
		throw VERUS_RUNTIME_ERROR << "vkBeginCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::End()
{
	VkResult res = VK_SUCCESS;
	if (VK_SUCCESS != (res = vkEndCommandBuffer(GetCommandBuffer())))
		throw VERUS_RUNTIME_ERROR << "vkEndCommandBuffer(), res=" << res;
}

void CommandBufferVulkan::BeginRenderPass()
{
	VERUS_QREF_RENDERER;

	VkClearValue clearValue;
	memcpy(&clearValue, renderer.GetClearColor().ToPointer(), sizeof(clearValue));

	VkRenderPassBeginInfo vkrpbi = {};
	vkrpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	//vkrpbi.renderPass = renderPass;
	//vkrpbi.framebuffer = swapChainFramebuffers[i];
	vkrpbi.renderArea.offset = { 0, 0 };
	//vkrpbi.renderArea.extent = swapChainExtent;
	vkrpbi.clearValueCount = 1;
	vkrpbi.pClearValues = &clearValue;
	vkCmdBeginRenderPass(GetCommandBuffer(), &vkrpbi, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBufferVulkan::EndRenderPass()
{
	vkCmdEndRenderPass(GetCommandBuffer());
}

void CommandBufferVulkan::BindVertexBuffers()
{
	vkCmdBindVertexBuffers(GetCommandBuffer(), 0, 1, nullptr, nullptr);
}

void CommandBufferVulkan::BindIndexBuffer()
{
	vkCmdBindIndexBuffer(GetCommandBuffer(), nullptr, 0, VK_INDEX_TYPE_UINT16);
}

void CommandBufferVulkan::SetScissor()
{
	vkCmdSetScissor(GetCommandBuffer(), 0, 1, nullptr);
}

void CommandBufferVulkan::SetViewport()
{
	vkCmdSetViewport(GetCommandBuffer(), 0, 1, nullptr);
}

void CommandBufferVulkan::BindPipeline()
{
	vkCmdBindPipeline(GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, nullptr);
}

void CommandBufferVulkan::PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout)
{
	PTextureVulkan pTex = static_cast<PTextureVulkan>(&(*tex));
	VkImageMemoryBarrier vkimb = {};
	vkimb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	vkimb.oldLayout = ToNativeImageLayout(oldLayout);
	vkimb.newLayout = ToNativeImageLayout(newLayout);
	vkimb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.image = pTex->GetVkImage();
	vkimb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkimb.subresourceRange.baseMipLevel = 0;
	vkimb.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	vkimb.subresourceRange.baseArrayLayer = 0;
	vkimb.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdPipelineBarrier(GetCommandBuffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &vkimb);
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
		vkCmdClearColorImage(GetCommandBuffer(), nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &vkisr);
	}
}

void CommandBufferVulkan::Draw()
{
	vkCmdDraw(GetCommandBuffer(), 0, 0, 0, 0);
}

void CommandBufferVulkan::DrawIndexed()
{
	vkCmdDrawIndexed(GetCommandBuffer(), 0, 0, 0, 0, 0);
}
