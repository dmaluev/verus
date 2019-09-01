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

	_vClearValues.reserve(16);
	_vAttachmentStates.reserve(4);
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		_pCommandLists[i] = pRendererD3D12->CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, pRendererD3D12->GetD3DCommandAllocator(i));
}

void CommandBufferD3D12::Done()
{
	VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
	{
		VERUS_COM_RELEASE_CHECK(_pCommandLists[i].Get());
		_pCommandLists[i].Reset();
	}
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
	VERUS_QREF_RENDERER_D3D12;

	_pRenderPass = &pRendererD3D12->GetRenderPassByID(renderPassID);
	_pFramebuffer = &pRendererD3D12->GetFramebufferByID(framebufferID);

	_vClearValues.clear();
	for (auto x : ilClearValues)
	{
		auto pColor = x.ToPointer();
		_vClearValues.push_back(pColor[0]);
		_vClearValues.push_back(pColor[1]);
		_vClearValues.push_back(pColor[2]);
		_vClearValues.push_back(pColor[3]);
	}
	_vAttachmentStates.clear();
	for (const auto& attachment : _pRenderPass->_vAttachments)
		_vAttachmentStates.push_back(attachment._initialState);

	_subpassIndex = 0;
	PrepareSubpass();
}

void CommandBufferD3D12::NextSubpass()
{
	_subpassIndex++;
	PrepareSubpass();
}

void CommandBufferD3D12::EndRenderPass()
{
	auto pCommandList = GetD3DGraphicsCommandList();

	CD3DX12_RESOURCE_BARRIER barriers[VERUS_MAX_NUM_RT];
	int numBarriers = 0;
	int index = 0;
	for (const auto& attachment : _pRenderPass->_vAttachments)
	{
		if (_vAttachmentStates[index] != attachment._finalState)
		{
			const auto& resources = _pFramebuffer->_vResources[index];
			barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(resources.Get(), _vAttachmentStates[index], attachment._finalState);
		}
		index++;
		if (VERUS_MAX_NUM_RT == numBarriers)
		{
			pCommandList->ResourceBarrier(numBarriers, barriers);
			numBarriers = 0;
		}
	}
	if (numBarriers)
		pCommandList->ResourceBarrier(numBarriers, barriers);

	_pRenderPass = nullptr;
	_pFramebuffer = nullptr;
	_subpassIndex = 0;
}

void CommandBufferD3D12::BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	geoD3D12.DestroyStagingBuffers();
	D3D12_VERTEX_BUFFER_VIEW views[VERUS_MAX_NUM_VB];
	const int num = geoD3D12.GetNumVertexBuffers();
	int at = 0;
	VERUS_FOR(i, num)
	{
		if ((bindingsFilter >> i) & 0x1)
		{
			views[at] = *geoD3D12.GetD3DVertexBufferView(i);
			at++;
		}
	}
	GetD3DGraphicsCommandList()->IASetVertexBuffers(0, at, views);
}

void CommandBufferD3D12::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoD3D12 = static_cast<RGeometryD3D12>(*geo);
	GetD3DGraphicsCommandList()->IASetIndexBuffer(geoD3D12.GetD3DIndexBufferView());
}

void CommandBufferD3D12::BindPipeline(PipelinePtr pipe)
{
	auto pCommandList = GetD3DGraphicsCommandList();
	auto& pipeD3D12 = static_cast<RPipelineD3D12>(*pipe);
	pCommandList->SetPipelineState(pipeD3D12.GetD3DPipelineState());
	if (pipeD3D12.IsCompute())
	{
		pCommandList->SetComputeRootSignature(pipeD3D12.GetD3DRootSignature());
	}
	else
	{
		pCommandList->SetGraphicsRootSignature(pipeD3D12.GetD3DRootSignature());
		pCommandList->IASetPrimitiveTopology(pipeD3D12.GetPrimitiveTopology());
	}
}

void CommandBufferD3D12::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	CD3DX12_VIEWPORT vpD3D12[VERUS_MAX_NUM_RT];
	int num = 0;
	for (const auto& rc : il)
		vpD3D12[num++] = CD3DX12_VIEWPORT(rc.getX(), rc.getY(), rc.Width(), rc.Height(), minDepth, maxDepth);
	GetD3DGraphicsCommandList()->RSSetViewports(num, vpD3D12);
}

void CommandBufferD3D12::SetScissor(std::initializer_list<Vector4> il)
{
	CD3DX12_RECT rcD3D12[VERUS_MAX_NUM_RT];
	int num = 0;
	for (const auto& rc : il)
		rcD3D12[num++] = CD3DX12_RECT(
			static_cast<LONG>(rc.getX()),
			static_cast<LONG>(rc.getY()),
			static_cast<LONG>(rc.getZ()),
			static_cast<LONG>(rc.getW()));
	GetD3DGraphicsCommandList()->RSSetScissorRects(num, rcD3D12);
}

void CommandBufferD3D12::SetBlendConstants(const float* p)
{
	GetD3DGraphicsCommandList()->OMSetBlendFactor(p);
}

bool CommandBufferD3D12::BindDescriptors(ShaderPtr shader, int setNumber, int complexSetID)
{
	bool copyDescOnly = false;
	if (setNumber < 0)
	{
		// On a Resource Binding Tier 2 hardware, all descriptor tables of type CBV and UAV declared in the currently set Root Signature
		// must be populated, even if the shaders do not need the descriptor.
		setNumber = -setNumber;
		copyDescOnly = true;
	}

	auto& shaderD3D12 = static_cast<RShaderD3D12>(*shader);
	if (shaderD3D12.TryRootConstants(setNumber, *this))
		return true;

	const D3D12_GPU_DESCRIPTOR_HANDLE hGPU = shaderD3D12.UpdateUniformBuffer(setNumber, complexSetID, copyDescOnly);
	if (!hGPU.ptr)
		return false;

	auto pCommandList = GetD3DGraphicsCommandList();
	const UINT rootParameterIndex = shaderD3D12.ToRootParameterIndex(setNumber);
	if (shaderD3D12.IsCompute())
		pCommandList->SetComputeRootDescriptorTable(rootParameterIndex, hGPU);
	else
		pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, hGPU);

	if (complexSetID >= 0 && !shaderD3D12.IsCompute())
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE hGPU = shaderD3D12.UpdateSamplers(setNumber, complexSetID);
		if (hGPU.ptr)
		{
			const UINT rootParameterIndex = shaderD3D12.GetNumDescriptorSets();
			pCommandList->SetGraphicsRootDescriptorTable(rootParameterIndex, hGPU);
		}
	}

	return true;
}

void CommandBufferD3D12::PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags)
{
	auto& shaderD3D12 = static_cast<RShaderD3D12>(*shader);
	if (shaderD3D12.IsCompute())
		GetD3DGraphicsCommandList()->SetComputeRoot32BitConstants(0, size, p, offset);
	else
		GetD3DGraphicsCommandList()->SetGraphicsRoot32BitConstants(0, size, p, offset);
}

void CommandBufferD3D12::PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range<int> mipLevels, int arrayLayer)
{
	auto& texD3D12 = static_cast<RTextureD3D12>(*tex);
	CD3DX12_RESOURCE_BARRIER rb[16];
	VERUS_RT_ASSERT(mipLevels.GetRange() < VERUS_ARRAY_LENGTH(rb));
	int index = 0;
	for (int mip : mipLevels)
	{
		const UINT subresource = D3D12CalcSubresource(mip, arrayLayer, 0, texD3D12.GetNumMipLevels(), texD3D12.GetNumArrayLayers());
		rb[index++] = CD3DX12_RESOURCE_BARRIER::Transition(
			texD3D12.GetD3DResource(), ToNativeImageLayout(oldLayout), ToNativeImageLayout(newLayout), subresource);
	}
	GetD3DGraphicsCommandList()->ResourceBarrier(index, rb);
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
		GetD3DGraphicsCommandList()->ClearDepthStencilView({}, static_cast<D3D12_CLEAR_FLAGS>(cf), 1, 0, 0, nullptr);
}

void CommandBufferD3D12::Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferD3D12::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
	GetD3DGraphicsCommandList()->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferD3D12::Dispatch(int groupCountX, int groupCountY, int groupCountZ)
{
	GetD3DGraphicsCommandList()->Dispatch(groupCountX, groupCountY, groupCountZ);
}

ID3D12GraphicsCommandList3* CommandBufferD3D12::GetD3DGraphicsCommandList() const
{
	VERUS_QREF_RENDERER;
	return _pCommandLists[renderer->GetRingBufferIndex()].Get();
}

void CommandBufferD3D12::PrepareSubpass()
{
	auto pCommandList = GetD3DGraphicsCommandList();

	RP::RcD3DSubpass subpass = _pRenderPass->_vSubpasses[_subpassIndex];
	RP::RcD3DFramebufferSubpass fs = _pFramebuffer->_vSubpasses[_subpassIndex];

	// Resource transitions for this subpass:
	CD3DX12_RESOURCE_BARRIER barriers[VERUS_MAX_NUM_RT + 1];
	int numBarriers = 0;
	VERUS_FOR(i, subpass._vColor.size())
	{
		const auto& ref = subpass._vColor[i];
		if (_vAttachmentStates[ref._index] != ref._state)
		{
			const auto& resources = fs._vResources[i];
			barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(resources.Get(), _vAttachmentStates[ref._index], ref._state);
			_vAttachmentStates[ref._index] = ref._state;
		}
	}
	if (subpass._depthStencil._index >= 0)
	{
		const auto& ref = subpass._depthStencil;
		if (_vAttachmentStates[ref._index] != ref._state)
		{
			barriers[numBarriers++] = CD3DX12_RESOURCE_BARRIER::Transition(fs._vResources.back().Get(), _vAttachmentStates[ref._index], ref._state);
			_vAttachmentStates[ref._index] = ref._state;
		}
	}
	pCommandList->ResourceBarrier(numBarriers, barriers);

	// Clear attachments for this subpass:
	int index = 0;
	for (const auto& ref : subpass._vColor)
	{
		const auto& attachment = _pRenderPass->_vAttachments[ref._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
			pCommandList->ClearRenderTargetView(fs._dhRTVs.AtCPU(index), &_vClearValues[ref._index << 2], 0, nullptr);
		index++;
	}
	if (subpass._depthStencil._index >= 0)
	{
		const auto& attachment = _pRenderPass->_vAttachments[subpass._depthStencil._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
		{
			D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
			if (RP::Attachment::LoadOp::clear == attachment._stencilLoadOp)
				clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
			pCommandList->ClearDepthStencilView(fs._dhDSV.AtCPU(0), clearFlags, _vClearValues[subpass._depthStencil._index << 2], 0, 0, nullptr);
		}
	}

	auto hRTV = fs._dhRTVs.AtCPU(0);
	auto hDSV = fs._dhDSV.AtCPU(0);
	pCommandList->OMSetRenderTargets(
		Utils::Cast32(subpass._vColor.size()),
		fs._dhRTVs.GetD3DDescriptorHeap() ? &hRTV : nullptr,
		TRUE,
		fs._dhDSV.GetD3DDescriptorHeap() ? &hDSV : nullptr);
}
