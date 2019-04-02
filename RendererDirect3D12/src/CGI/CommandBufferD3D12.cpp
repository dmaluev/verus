#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

ID3D12GraphicsCommandList* CommandBufferD3D12::GetGraphicsCommandList() const
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
