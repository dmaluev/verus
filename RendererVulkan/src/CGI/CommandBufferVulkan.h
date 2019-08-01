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
			virtual void NextSubpass() override;
			virtual void EndRenderPass() override;

			virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter) override;
			virtual void BindIndexBuffer(GeometryPtr geo) override;

			virtual void BindPipeline(PipelinePtr pipe) override;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth) override;
			virtual void SetScissor(std::initializer_list<Vector4> il) override;
			virtual void SetBlendConstants(const float* p) override;

			virtual bool BindDescriptors(ShaderPtr shader, int setNumber, int complexSetID) override;
			virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags) override;

			virtual void PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, int mipLevel) override;
			virtual void Clear(ClearFlags clearFlags) override;

			virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override;
			virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override;

			//
			// Vulkan
			//

			VkCommandBuffer GetVkCommandBuffer() const;

			void InitSingleTimeCommands();
			void DoneSingleTimeCommands();
		};
		VERUS_TYPEDEFS(CommandBufferVulkan);
	}
}
