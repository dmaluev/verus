#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferVulkan : public BaseCommandBuffer
		{
			VkCommandBuffer _commandBuffers[BaseRenderer::s_ringBufferSize] = {};

		public:
			CommandBufferVulkan();
			virtual ~CommandBufferVulkan() override;

			virtual void Init() override;
			virtual void Done() override;

			virtual void Begin() override;
			virtual void End() override;

			virtual void BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, PcVector4 pRenderArea) override;
			virtual void EndRenderPass() override;

			virtual void BindVertexBuffers(GeometryPtr geo) override;
			virtual void BindIndexBuffer(GeometryPtr geo) override;

			virtual void SetScissor(std::initializer_list<Vector4> il) override;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth) override;

			virtual void BindPipeline(PipelinePtr pipe) override;
			virtual void PushConstant(PipelinePtr pipe, int offset, int size, const void* p) override;
			virtual void PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout) override;
			virtual void Clear(ClearFlags clearFlags) override;

			virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override;
			virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override;

			//
			// Vulkan
			//

			VkCommandBuffer GetVkCommandBuffer() const;
		};
		VERUS_TYPEDEFS(CommandBufferVulkan);
	}
}
