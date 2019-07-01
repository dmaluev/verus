#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferD3D12 : public BaseCommandBuffer
		{
			ComPtr<ID3D12GraphicsCommandList3> _pCommandLists[BaseRenderer::s_ringBufferSize];

		public:
			CommandBufferD3D12();
			virtual ~CommandBufferD3D12() override;

			virtual void Init() override;
			virtual void Done() override;

			virtual void Begin() override;
			virtual void End() override;

			virtual void BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, PcVector4 pRenderArea) override;
			virtual void EndRenderPass() override;

			virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter) override;
			virtual void BindIndexBuffer(GeometryPtr geo) override;

			virtual void BindPipeline(PipelinePtr pipe) override;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth) override;
			virtual void SetScissor(std::initializer_list<Vector4> il) override;
			virtual void SetBlendConstants(const float* p) override;

			virtual bool BindDescriptors(ShaderPtr shader, int descSet) override;
			virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags) override;

			virtual void PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout) override;
			virtual void Clear(ClearFlags clearFlags) override;

			virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override;
			virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override;

			//
			// D3D12
			//

			ID3D12GraphicsCommandList3* GetD3DGraphicsCommandList() const;
		};
		VERUS_TYPEDEFS(CommandBufferD3D12);
	}
}
