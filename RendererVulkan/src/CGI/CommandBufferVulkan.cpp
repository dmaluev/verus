// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

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

void CommandBufferVulkan::Begin()
{
	VkResult res = VK_SUCCESS;
	VkCommandBufferBeginInfo vkcbbi = {};
	vkcbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (_oneTimeSubmit)
		vkcbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (VK_SUCCESS != (res = vkBeginCommandBuffer(GetVkCommandBuffer(), &vkcbbi)))
		throw VERUS_RUNTIME_ERROR << "vkBeginCommandBuffer(); res=" << res;
}

void CommandBufferVulkan::End()
{
	VkResult res = VK_SUCCESS;
	if (VK_SUCCESS != (res = vkEndCommandBuffer(GetVkCommandBuffer())))
		throw VERUS_RUNTIME_ERROR << "vkEndCommandBuffer(); res=" << res;
}

void CommandBufferVulkan::PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range mipLevels, Range arrayLayers)
{
	auto& texVulkan = static_cast<RTextureVulkan>(*tex);
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Waiting for this stage to finish (TOP_OF_PIPE means wait for nothing).
	VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Which stage is waiting to start (BOTTOM_OF_PIPE means nothing is waiting).
	const VkPipelineStageFlags dstStageMaskXS = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	VkImageMemoryBarrier vkimb = {};
	vkimb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	vkimb.oldLayout = ToNativeImageLayout(oldLayout);
	vkimb.newLayout = ToNativeImageLayout(newLayout);
	vkimb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	vkimb.image = texVulkan.GetVkImage();
	vkimb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vkimb.subresourceRange.baseMipLevel = mipLevels._begin;
	vkimb.subresourceRange.levelCount = mipLevels.GetCount();
	vkimb.subresourceRange.baseArrayLayer = arrayLayers._begin;
	vkimb.subresourceRange.layerCount = arrayLayers.GetCount();

	// See: https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

	auto UseDefaultsForOldLayout = [&vkimb, &srcStageMask]()
	{
		vkimb.srcAccessMask = 0;
		switch (vkimb.oldLayout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_GENERAL:
			srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		default:
			VERUS_RT_FAIL("Unknown oldLayout");
		}
	};

	auto UseDefaultsForNewLayout = [&vkimb, &dstStageMask, dstStageMaskXS, newLayout, tex]()
	{
		switch (vkimb.newLayout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_GENERAL:
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			vkimb.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dstStageMask = (ImageLayout::xsReadOnly == newLayout) ? dstStageMaskXS : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			vkimb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			vkimb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
		default:
			VERUS_RT_FAIL("Unknown newLayout");
		}

		if (vkimb.newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
			vkimb.newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
		{
			vkimb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (Format::unormD24uintS8 == tex->GetFormat())
				vkimb.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	};

	if (vkimb.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && vkimb.newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		UseDefaultsForOldLayout();
		dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		vkimb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}
	else
	{
		UseDefaultsForOldLayout();
		UseDefaultsForNewLayout();
	}

	vkCmdPipelineBarrier(GetVkCommandBuffer(), srcStageMask, dstStageMask, 0,
		0, nullptr,
		0, nullptr,
		1, &vkimb);
}

void CommandBufferVulkan::BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle, std::initializer_list<Vector4> ilClearValues, bool setViewportAndScissor)
{
	VERUS_QREF_RENDERER_VULKAN;
	RendererVulkan::RcFramebuffer framebuffer = pRendererVulkan->GetFramebuffer(framebufferHandle);
	// The application must ensure (using scissor if necessary) that all rendering is contained within the render area.
	VkRenderPassBeginInfo vkrpbi = {};
	vkrpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	vkrpbi.renderPass = pRendererVulkan->GetRenderPass(renderPassHandle);
	vkrpbi.framebuffer = framebuffer._framebuffer;
	vkrpbi.renderArea.extent.width = framebuffer._width;
	vkrpbi.renderArea.extent.height = framebuffer._height;
	VkClearValue clearValues[VERUS_MAX_FB_ATTACH] = {};
	VERUS_RT_ASSERT(ilClearValues.size() <= VERUS_MAX_FB_ATTACH);
	int count = 0;
	for (const auto& x : ilClearValues)
	{
		memcpy(clearValues[count].color.float32, x.ToPointer(), sizeof(clearValues[count].color.float32));
		count++;
	}
	vkrpbi.clearValueCount = count;
	vkrpbi.pClearValues = clearValues;
	// There may be a performance cost for using a render area smaller than the framebuffer, unless it matches the render area granularity for the render pass.
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

void CommandBufferVulkan::BindPipeline(PipelinePtr pipe)
{
	auto& pipeVulkan = static_cast<RPipelineVulkan>(*pipe);
	vkCmdBindPipeline(GetVkCommandBuffer(),
		pipeVulkan.IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeVulkan.GetVkPipeline());
}

void CommandBufferVulkan::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	if (il.size() > 0)
	{
		const float w = il.begin()->Width();
		const float h = il.begin()->Height();
		_viewportSize = Vector4(w, h, 1 / w, 1 / h);
	}

	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	VkViewport vpVulkan[VERUS_MAX_CA];
	int count = 0;
	for (const auto& rc : il)
	{
		vpVulkan[count].x = rc.getX();
		vpVulkan[count].y = rc.getY() + rc.Height();
		vpVulkan[count].width = rc.Width();
		vpVulkan[count].height = -rc.Height();
		vpVulkan[count].minDepth = minDepth;
		vpVulkan[count].maxDepth = maxDepth;
		count++;
	}
	vkCmdSetViewport(GetVkCommandBuffer(), 0, count, vpVulkan);
}

void CommandBufferVulkan::SetScissor(std::initializer_list<Vector4> il)
{
	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	VkRect2D rcVulkan[VERUS_MAX_CA];
	int count = 0;
	for (const auto& rc : il)
	{
		rcVulkan[count].offset.x = static_cast<int32_t>(rc.getX());
		rcVulkan[count].offset.y = static_cast<int32_t>(rc.getY());
		rcVulkan[count].extent.width = static_cast<uint32_t>(rc.Width());
		rcVulkan[count].extent.height = static_cast<uint32_t>(rc.Height());
		count++;
	}
	vkCmdSetScissor(GetVkCommandBuffer(), 0, count, rcVulkan);
}

void CommandBufferVulkan::SetBlendConstants(const float* p)
{
	vkCmdSetBlendConstants(GetVkCommandBuffer(), p);
}

void CommandBufferVulkan::BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter)
{
	auto& geoVulkan = static_cast<RGeometryVulkan>(*geo);
	VkBuffer buffers[VERUS_MAX_VB];
	VkDeviceSize offsets[VERUS_MAX_VB];
	const int count = geoVulkan.GetVertexBufferCount();
	int filteredCount = 0;
	VERUS_FOR(i, count)
	{
		if ((bindingsFilter >> i) & 0x1)
		{
			VERUS_RT_ASSERT(filteredCount < VERUS_MAX_VB);
			buffers[filteredCount] = geoVulkan.GetVkVertexBuffer(i);
			offsets[filteredCount] = geoVulkan.GetVkVertexBufferOffset(i);
			filteredCount++;
		}
	}
	vkCmdBindVertexBuffers(GetVkCommandBuffer(), 0, filteredCount, buffers, offsets);
}

void CommandBufferVulkan::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoVulkan = static_cast<RGeometryVulkan>(*geo);
	VkBuffer buffer = geoVulkan.GetVkIndexBuffer();
	VkDeviceSize offset = geoVulkan.GetVkIndexBufferOffset();
	vkCmdBindIndexBuffer(GetVkCommandBuffer(), buffer, offset, geoVulkan.Has32BitIndices() ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

bool CommandBufferVulkan::BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle)
{
	if (setNumber < 0)
		return true;

	auto& shaderVulkan = static_cast<RShaderVulkan>(*shader);
	if (shaderVulkan.TryPushConstants(setNumber, *this))
		return true;
	const int offset = shaderVulkan.UpdateUniformBuffer(setNumber);
	if (offset < 0)
		return false;

	const VkDescriptorSet descriptorSet = complexSetHandle.IsSet() ?
		shaderVulkan.GetComplexVkDescriptorSet(complexSetHandle) : shaderVulkan.GetVkDescriptorSet(setNumber);
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

void CommandBufferVulkan::TraceRays(int width, int height, int depth)
{
	//vkCmdTraceRaysKHR(GetVkCommandBuffer(), nullptr, nullptr, nullptr, nullptr, width, height, depth);
}

VkCommandBuffer CommandBufferVulkan::GetVkCommandBuffer() const
{
	VERUS_QREF_RENDERER;
	return _commandBuffers[renderer->GetRingBufferIndex()];
}
