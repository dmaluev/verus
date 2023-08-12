// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class CommandBufferVulkan : public BaseCommandBuffer
	{
		VkCommandBuffer _commandBuffers[BaseRenderer::s_ringBufferSize] = {};
		bool            _oneTimeSubmit = false;

	public:
		CommandBufferVulkan();
		virtual ~CommandBufferVulkan() override;

		virtual void Init() override;
		virtual void Done() override;

		virtual void InitOneTimeSubmit() override;
		virtual void DoneOneTimeSubmit() override;

		virtual void Begin() override;
		virtual void End() override;

		virtual void PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range mipLevels, Range arrayLayers) override;

		virtual void BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle,
			std::initializer_list<Vector4> ilClearValues, ViewportScissorFlags vsf) override;
		virtual void NextSubpass() override;
		virtual void EndRenderPass() override;

		virtual void BindPipeline(PipelinePtr pipe) override;
		virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth) override;
		virtual void SetScissor(std::initializer_list<Vector4> il) override;
		virtual void SetBlendConstants(const float* p) override;

		virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter) override;
		virtual void BindIndexBuffer(GeometryPtr geo) override;

		virtual bool BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle) override;
		virtual bool BindDescriptors(ShaderPtr shader, int setNumber, GeometryPtr geo, int sbIndex) override;
		virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags) override;

		virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override;
		virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override;
		virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ) override;
		virtual void TraceRays(int width, int height, int depth) override;

		//
		// Vulkan
		//

		VkCommandBuffer GetVkCommandBuffer() const;
	};
	VERUS_TYPEDEFS(CommandBufferVulkan);
}
