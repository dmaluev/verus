#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferD3D12 : public BaseCommandBuffer
		{
			ComPtr<ID3D12GraphicsCommandList4> _pCommandLists[BaseRenderer::s_ringBufferSize];

			ID3D12GraphicsCommandList4* GetGraphicsCommandList() const;

		public:
			CommandBufferD3D12();
			virtual ~CommandBufferD3D12() override;

			virtual void Init() override;
			virtual void Done() override;

			virtual void Begin() override;
			virtual void End() override;

			virtual void BeginRenderPass() override;
			virtual void EndRenderPass() override;

			virtual void BindVertexBuffers() override;
			virtual void BindIndexBuffer() override;

			virtual void SetScissor() override;
			virtual void SetViewport() override;

			virtual void BindPipeline() override;
			virtual void PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout) override;
			virtual void Clear(ClearFlags clearFlags) override;
			virtual void Draw() override;
			virtual void DrawIndexed() override;
		};
		VERUS_TYPEDEFS(CommandBufferD3D12);
	}
}
