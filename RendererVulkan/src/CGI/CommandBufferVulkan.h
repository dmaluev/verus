#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferVulkan : public BaseCommandBuffer
		{
			VkCommandBuffer _commandBuffers[BaseRenderer::ringBufferSize] = { VK_NULL_HANDLE };

			VkCommandBuffer GetCommandBuffer() const;

		public:
			CommandBufferVulkan();
			virtual ~CommandBufferVulkan() override;

			virtual void Init() override;
			virtual void Done() override;

			virtual void Begin() override;
			virtual void End() override;

			virtual void BindVertexBuffers() override;
			virtual void BindIndexBuffer() override;

			virtual void SetScissor() override;
			virtual void SetViewport() override;

			virtual void BindPipeline() override;
			virtual void Clear(ClearFlags clearFlags) override;
			virtual void Draw() override;
			virtual void DrawIndexed() override;
		};
	}
}
