#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferD3D12 : public BaseCommandBuffer
		{
			ComPtr<ID3D12GraphicsCommandList> _pCommandLists[BaseRenderer::ringBufferSize];

			ID3D12GraphicsCommandList* GetGraphicsCommandList() const;

		public:
			CommandBufferD3D12();
			virtual ~CommandBufferD3D12() override;

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
