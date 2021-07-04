// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class CommandBufferD3D12 : public BaseCommandBuffer
		{
			ComPtr<ID3D12CommandAllocator>     _pOneTimeCommandAllocator;
			ComPtr<ID3D12GraphicsCommandList3> _pCommandLists[BaseRenderer::s_ringBufferSize];
			RP::PcD3DRenderPass                _pRenderPass = nullptr;
			RP::PcD3DFramebuffer               _pFramebuffer = nullptr;
			Vector<FLOAT>                      _vClearValues;
			Vector<D3D12_RESOURCE_STATES>      _vAttachmentStates;
			int                                _subpassIndex = 0;

		public:
			CommandBufferD3D12();
			virtual ~CommandBufferD3D12() override;

			virtual void Init() override;
			virtual void Done() override;

			virtual void InitOneTimeSubmit() override;
			virtual void DoneOneTimeSubmit() override;

			virtual void Begin() override;
			virtual void End() override;

			virtual void BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle, std::initializer_list<Vector4> ilClearValues, bool setViewportAndScissor) override;
			virtual void NextSubpass() override;
			virtual void EndRenderPass() override;

			virtual void BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter) override;
			virtual void BindIndexBuffer(GeometryPtr geo) override;

			virtual void BindPipeline(PipelinePtr pipe) override;
			virtual void SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth) override;
			virtual void SetScissor(std::initializer_list<Vector4> il) override;
			virtual void SetBlendConstants(const float* p) override;

			virtual bool BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle) override;
			virtual void PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags) override;

			virtual void PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range<int> mipLevels, int arrayLayer) override;

			virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override;
			virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override;
			virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ) override;
			virtual void DispatchMesh(int groupCountX, int groupCountY, int groupCountZ) override;
			virtual void TraceRays(int width, int height, int depth) override;

			//
			// D3D12
			//

			ID3D12GraphicsCommandList3* GetD3DGraphicsCommandList() const;

			void PrepareSubpass();
		};
		VERUS_TYPEDEFS(CommandBufferD3D12);
	}
}
