#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

CommandBufferD3D12::CommandBufferD3D12()
{
}

CommandBufferD3D12::~CommandBufferD3D12()
{
	Done();
}

void CommandBufferD3D12::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_D3D12;

	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i] = pRendererD3D12->CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, pRendererD3D12->GetD3DCommandAllocator(i));
}

void CommandBufferD3D12::Done()
{
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i].Reset();
	VERUS_DONE(CommandBufferD3D12);
}

void CommandBufferD3D12::Begin()
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;
	if (FAILED(hr = GetD3DGraphicsCommandList()->Reset(pRendererD3D12->GetD3DCommandAllocator(pRendererD3D12->GetRingBufferIndex()), nullptr)))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::End()
{
	HRESULT hr = 0;
	if (FAILED(hr = GetD3DGraphicsCommandList()->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::BeginRenderPass(int renderPassID, int framebufferID, std::initializer_list<Vector4> ilClearValues, PcVector4 pRenderArea)
{
	D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc = {};
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc = {};
	GetD3DGraphicsCommandList()->BeginRenderPass(0, &rtDesc, &dsDesc, D3D12_RENDER_PASS_FLAG_NONE);
}

void CommandBufferD3D12::EndRenderPass()
{
	GetD3DGraphicsCommandList()->EndRenderPass();
}

void CommandBufferD3D12::BindVertexBuffers(GeometryPtr geo)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	GetD3DGraphicsCommandList()->IASetVertexBuffers(0, 1, geoD3D12.GetD3DVertexBufferView());
}

void CommandBufferD3D12::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	GetD3DGraphicsCommandList()->IASetIndexBuffer(geoD3D12.GetD3DIndexBufferView());
}

void CommandBufferD3D12::SetScissor(std::initializer_list<Vector4> il)
{
	CD3DX12_RECT rcD3D12[VERUS_CGI_MAX_RT];
	int num = 0;
	for (auto& rc : il)
		rcD3D12[num++] = CD3DX12_RECT(
			static_cast<LONG>(rc.getX()),
			static_cast<LONG>(rc.getY()),
			static_cast<LONG>(rc.getZ()),
			static_cast<LONG>(rc.getW()));
	GetD3DGraphicsCommandList()->RSSetScissorRects(num, rcD3D12);
}

void CommandBufferD3D12::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	CD3DX12_VIEWPORT vpD3D12[VERUS_CGI_MAX_RT];
	int num = 0;
	for (auto& rc : il)
		vpD3D12[num++] = CD3DX12_VIEWPORT(rc.getX(), rc.getY(), rc.Width(), rc.Height(), minDepth, maxDepth);
	GetD3DGraphicsCommandList()->RSSetViewports(num, vpD3D12);
}

void CommandBufferD3D12::BindPipeline(PipelinePtr pipe)
{
	auto& pipeD3D12 = static_cast<RPipelineD3D12>(*pipe);
	GetD3DGraphicsCommandList()->SetPipelineState(pipeD3D12.GetD3DPipelineState());
	GetD3DGraphicsCommandList()->SetGraphicsRootSignature(pipeD3D12.GetD3DRootSignature());
	GetD3DGraphicsCommandList()->IASetPrimitiveTopology(pipeD3D12.GetPrimitiveTopology());
}

void CommandBufferD3D12::PushConstant(PipelinePtr pipe, int offset, int size, const void* p)
{
	GetD3DGraphicsCommandList()->SetGraphicsRoot32BitConstants(0, size, p, offset);
}

void CommandBufferD3D12::PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout)
{
	auto& texD3D12 = static_cast<RTextureD3D12>(*tex);
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = texD3D12.GetD3DResource();
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.StateBefore = ToNativeImageLayout(oldLayout);
	rb.Transition.StateAfter = ToNativeImageLayout(newLayout);
	GetD3DGraphicsCommandList()->ResourceBarrier(1, &rb);
}

void CommandBufferD3D12::Clear(ClearFlags clearFlags)
{
	VERUS_QREF_RENDERER;

	if (clearFlags & ClearFlags::color)
		GetD3DGraphicsCommandList()->ClearRenderTargetView({}, renderer.GetClearColor().ToPointer(), 0, nullptr);

	UINT cf = 0;
	if (clearFlags & ClearFlags::depth) cf |= D3D12_CLEAR_FLAG_DEPTH;
	if (clearFlags & ClearFlags::stencil) cf |= D3D12_CLEAR_FLAG_STENCIL;
	if (cf)
		GetD3DGraphicsCommandList()->ClearDepthStencilView({}, static_cast<D3D12_CLEAR_FLAGS>(cf), 0, 0, 0, nullptr);
}

void CommandBufferD3D12::Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferD3D12::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

ID3D12GraphicsCommandList4* CommandBufferD3D12::GetD3DGraphicsCommandList() const
{
	VERUS_QREF_RENDERER;
	return _pCommandLists[renderer->GetRingBufferIndex()].Get();
}
