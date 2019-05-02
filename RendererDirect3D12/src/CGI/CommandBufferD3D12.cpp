#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

ID3D12GraphicsCommandList4* CommandBufferD3D12::GetGraphicsCommandList() const
{
	VERUS_QREF_RENDERER;
	return _pCommandLists[renderer->GetRingBufferIndex()].Get();
}

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
}

void CommandBufferD3D12::Done()
{
	VERUS_DONE(CommandBufferD3D12);
}

void CommandBufferD3D12::Begin()
{
	HRESULT hr = 0;
	if (FAILED(hr = GetGraphicsCommandList()->Reset(nullptr, nullptr)))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::End()
{
	HRESULT hr = 0;
	if (FAILED(hr = GetGraphicsCommandList()->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);
}

void CommandBufferD3D12::BeginRenderPass()
{
	D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc = {};
	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc = {};
	GetGraphicsCommandList()->BeginRenderPass(0, &rtDesc, &dsDesc, D3D12_RENDER_PASS_FLAG_NONE);
}

void CommandBufferD3D12::EndRenderPass()
{
	GetGraphicsCommandList()->EndRenderPass();
}

void CommandBufferD3D12::BindVertexBuffers()
{
	GetGraphicsCommandList()->IASetVertexBuffers(0, 1, nullptr);
}

void CommandBufferD3D12::BindIndexBuffer()
{
	GetGraphicsCommandList()->IASetIndexBuffer(nullptr);
}

void CommandBufferD3D12::SetScissor()
{
	GetGraphicsCommandList()->RSSetScissorRects(1, nullptr);
}

void CommandBufferD3D12::SetViewport()
{
	GetGraphicsCommandList()->RSSetViewports(1, nullptr);
}

void CommandBufferD3D12::BindPipeline()
{
	GetGraphicsCommandList()->SetPipelineState(nullptr);
}

void CommandBufferD3D12::PipelineBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout)
{
	PTextureD3D12 pTex = static_cast<PTextureD3D12>(&(*tex));
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = pTex->GetID3D12Resource();
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.StateBefore = ToNativeImageLayout(oldLayout);
	rb.Transition.StateAfter = ToNativeImageLayout(newLayout);
	GetGraphicsCommandList()->ResourceBarrier(1, &rb);
}

void CommandBufferD3D12::Clear(ClearFlags clearFlags)
{
	VERUS_QREF_RENDERER;

	if (clearFlags & ClearFlags::color)
		GetGraphicsCommandList()->ClearRenderTargetView({}, renderer.GetClearColor().ToPointer(), 0, nullptr);

	UINT cf = 0;
	if (clearFlags & ClearFlags::depth) cf |= D3D12_CLEAR_FLAG_DEPTH;
	if (clearFlags & ClearFlags::stencil) cf |= D3D12_CLEAR_FLAG_STENCIL;
	if (cf)
		GetGraphicsCommandList()->ClearDepthStencilView({}, static_cast<D3D12_CLEAR_FLAGS>(cf), 0, 0, 0, nullptr);
}

void CommandBufferD3D12::Draw()
{
	GetGraphicsCommandList()->DrawInstanced(0, 0, 0, 0);
}

void CommandBufferD3D12::DrawIndexed()
{
	GetGraphicsCommandList()->DrawIndexedInstanced(0, 0, 0, 0, 0);
}
