// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

CommandBufferD3D11::CommandBufferD3D11()
{
	VERUS_ZERO_MEM(_blendFactor);
}

CommandBufferD3D11::~CommandBufferD3D11()
{
	Done();
}

void CommandBufferD3D11::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_D3D11;

	_vClearValues.reserve(16);
	_pDeviceContext = pRendererD3D11->GetD3DDeviceContext();
}

void CommandBufferD3D11::Done()
{
	_pDeviceContext.Reset();

	VERUS_DONE(CommandBufferD3D11);
}

void CommandBufferD3D11::InitOneTimeSubmit()
{
	Init();
}

void CommandBufferD3D11::DoneOneTimeSubmit()
{
	Done();
}

void CommandBufferD3D11::Begin()
{
}

void CommandBufferD3D11::End()
{
}

void CommandBufferD3D11::PipelineImageMemoryBarrier(TexturePtr tex, ImageLayout oldLayout, ImageLayout newLayout, Range mipLevels, Range arrayLayers)
{
}

void CommandBufferD3D11::BeginRenderPass(RPHandle renderPassHandle, FBHandle framebufferHandle, std::initializer_list<Vector4> ilClearValues, ViewportScissorFlags vsf)
{
	VERUS_QREF_RENDERER_D3D11;

	_pRenderPass = &pRendererD3D11->GetRenderPass(renderPassHandle);
	_pFramebuffer = &pRendererD3D11->GetFramebuffer(framebufferHandle);

	VERUS_RT_ASSERT(_pRenderPass->_vAttachments.size() == ilClearValues.size());

	_vClearValues.clear();
	for (const auto& x : ilClearValues)
	{
		auto pColor = x.ToPointer();
		_vClearValues.push_back(pColor[0]);
		_vClearValues.push_back(pColor[1]);
		_vClearValues.push_back(pColor[2]);
		_vClearValues.push_back(pColor[3]);
	}

	_subpassIndex = 0;
	PrepareSubpass();

	SetViewportAndScissor(vsf, _pFramebuffer->_width, _pFramebuffer->_height);
}

void CommandBufferD3D11::NextSubpass()
{
	_subpassIndex++;
	PrepareSubpass();
}

void CommandBufferD3D11::EndRenderPass()
{
	// Unbind everything:
	_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	_pRenderPass = nullptr;
	_pFramebuffer = nullptr;
	_subpassIndex = 0;
}

void CommandBufferD3D11::BindPipeline(PipelinePtr pipe)
{
	auto& pipeD3D11 = static_cast<RPipelineD3D11>(*pipe);
	if (pipeD3D11.IsCompute())
	{
		_pDeviceContext->CSSetShader(pipeD3D11.GetD3DCS(), nullptr, 0);
	}
	else
	{
		_pDeviceContext->VSSetShader(pipeD3D11.GetD3DVS(), nullptr, 0);
		_pDeviceContext->HSSetShader(pipeD3D11.GetD3DHS(), nullptr, 0);
		_pDeviceContext->DSSetShader(pipeD3D11.GetD3DDS(), nullptr, 0);
		_pDeviceContext->GSSetShader(pipeD3D11.GetD3DGS(), nullptr, 0);
		_pDeviceContext->PSSetShader(pipeD3D11.GetD3DPS(), nullptr, 0);
		_pDeviceContext->OMSetBlendState(pipeD3D11.GetD3DBlendState(), _blendFactor, pipeD3D11.GetSampleMask());
		_pDeviceContext->RSSetState(pipeD3D11.GetD3DRasterizerState());
		_pDeviceContext->OMSetDepthStencilState(pipeD3D11.GetD3DDepthStencilState(), pipeD3D11.GetStencilRef());
		_pDeviceContext->IASetInputLayout(pipeD3D11.GetD3DInputLayout());
		_pDeviceContext->IASetPrimitiveTopology(pipeD3D11.GetD3DPrimitiveTopology());
	}
}

void CommandBufferD3D11::SetViewport(std::initializer_list<Vector4> il, float minDepth, float maxDepth)
{
	if (il.size() > 0)
	{
		const float w = il.begin()->Width();
		const float h = il.begin()->Height();
		_viewportSize = Vector4(w, h, 1 / w, 1 / h);
	}

	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	D3D11_VIEWPORT vpD3D11[VERUS_MAX_CA];
	UINT count = 0;
	for (const auto& rc : il)
	{
		vpD3D11[count].TopLeftX = rc.getX();
		vpD3D11[count].TopLeftY = rc.getY();
		vpD3D11[count].Width = rc.Width();
		vpD3D11[count].Height = rc.Height();
		vpD3D11[count].MinDepth = minDepth;
		vpD3D11[count].MaxDepth = maxDepth;
		count++;
	}
	_pDeviceContext->RSSetViewports(count, vpD3D11);
}

void CommandBufferD3D11::SetScissor(std::initializer_list<Vector4> il)
{
	VERUS_RT_ASSERT(il.size() <= VERUS_MAX_CA);
	D3D11_RECT rcD3D11[VERUS_MAX_CA];
	UINT count = 0;
	for (const auto& rc : il)
		rcD3D11[count++] = CD3D11_RECT(
			static_cast<LONG>(rc.Left()),
			static_cast<LONG>(rc.Top()),
			static_cast<LONG>(rc.Right()),
			static_cast<LONG>(rc.Bottom()));
	_pDeviceContext->RSSetScissorRects(count, rcD3D11);
}

void CommandBufferD3D11::SetBlendConstants(const float* p)
{
	memcpy(_blendFactor, p, 4 * sizeof(float));
}

void CommandBufferD3D11::BindVertexBuffers(GeometryPtr geo, UINT32 bindingsFilter)
{
	auto& geoD3D11 = static_cast<RGeometryD3D11>(*geo);
	ID3D11Buffer* buffers[VERUS_MAX_VB];
	UINT strides[VERUS_MAX_VB];
	UINT offsets[VERUS_MAX_VB];
	const int count = geoD3D11.GetVertexBufferCount();
	int filteredCount = 0;
	VERUS_FOR(i, count)
	{
		if ((bindingsFilter >> i) & 0x1)
		{
			VERUS_RT_ASSERT(filteredCount < VERUS_MAX_VB);
			buffers[filteredCount] = geoD3D11.GetD3DVertexBuffer(i);
			strides[filteredCount] = geoD3D11.GetStride(i);
			offsets[filteredCount] = 0;
			filteredCount++;
		}
	}
	_pDeviceContext->IASetVertexBuffers(0, filteredCount, buffers, strides, offsets);
}

void CommandBufferD3D11::BindIndexBuffer(GeometryPtr geo)
{
	auto& geoD3D11 = static_cast<RGeometryD3D11>(*geo);
	_pDeviceContext->IASetIndexBuffer(geoD3D11.GetD3DIndexBuffer(), geoD3D11.Has32BitIndices() ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
}

bool CommandBufferD3D11::BindDescriptors(ShaderPtr shader, int setNumber, CSHandle complexSetHandle)
{
	auto& shaderD3D11 = static_cast<RShaderD3D11>(*shader);

	ShaderStageFlags stageFlags;
	ShaderResources shaderResources;
	ID3D11Buffer* pBuffer = shaderD3D11.UpdateUniformBuffer(setNumber, stageFlags);
	if (complexSetHandle.IsSet())
	{
		shaderD3D11.GetShaderResources(setNumber, complexSetHandle.Get(), shaderResources);
		shaderD3D11.GetSamplers(setNumber, complexSetHandle.Get(), shaderResources);
	}

	if (stageFlags & ShaderStageFlags::vs)
	{
		_pDeviceContext->VSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->VSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->VSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
	}
	if (stageFlags & ShaderStageFlags::hs)
	{
		_pDeviceContext->HSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->HSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->HSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
	}
	if (stageFlags & ShaderStageFlags::ds)
	{
		_pDeviceContext->DSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->DSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->DSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
	}
	if (stageFlags & ShaderStageFlags::gs)
	{
		_pDeviceContext->GSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->GSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->GSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
	}
	if (stageFlags & ShaderStageFlags::fs)
	{
		_pDeviceContext->PSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->PSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->PSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
	}
	if (stageFlags & ShaderStageFlags::cs)
	{
		_pDeviceContext->CSSetConstantBuffers(setNumber, 1, &pBuffer);
		if (shaderResources._srvCount)
		{
			_pDeviceContext->CSSetShaderResources(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._srvs);
			_pDeviceContext->CSSetSamplers(shaderResources._srvStartSlot, shaderResources._srvCount, shaderResources._samplers);
		}
		if (shaderResources._uavCount)
			_pDeviceContext->CSSetUnorderedAccessViews(shaderResources._uavStartSlot, shaderResources._uavCount, shaderResources._uavs, nullptr);
	}

	return true;
}

void CommandBufferD3D11::PushConstants(ShaderPtr shader, int offset, int size, const void* p, ShaderStageFlags stageFlags)
{
}

void CommandBufferD3D11::Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
{
	_pDeviceContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBufferD3D11::DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance)
{
	_pDeviceContext->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBufferD3D11::Dispatch(int groupCountX, int groupCountY, int groupCountZ)
{
	_pDeviceContext->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandBufferD3D11::DispatchMesh(int groupCountX, int groupCountY, int groupCountZ)
{
}

void CommandBufferD3D11::TraceRays(int width, int height, int depth)
{
}

ID3D11DeviceContext* CommandBufferD3D11::GetD3DDeviceContext() const
{
	return _pDeviceContext.Get();
}

void CommandBufferD3D11::PrepareSubpass()
{
	RP::RcD3DSubpass subpass = _pRenderPass->_vSubpasses[_subpassIndex];
	RP::RcD3DFramebufferSubpass fs = _pFramebuffer->_vSubpasses[_subpassIndex];

	// Clear attachments for this subpass:
	int index = 0;
	for (const auto& ref : subpass._vColor)
	{
		const auto& attachment = _pRenderPass->_vAttachments[ref._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
			_pDeviceContext->ClearRenderTargetView(fs._vRTVs[index], &_vClearValues[ref._index << 2]);
		index++;
	}
	if (subpass._depthStencil._index >= 0)
	{
		const auto& attachment = _pRenderPass->_vAttachments[subpass._depthStencil._index];
		if (RP::Attachment::LoadOp::clear == attachment._loadOp && attachment._clearSubpassIndex == _subpassIndex)
		{
			UINT clearFlags = D3D11_CLEAR_DEPTH;
			UINT8 stencil = 0;
			if (RP::Attachment::LoadOp::clear == attachment._stencilLoadOp)
			{
				clearFlags |= D3D11_CLEAR_STENCIL;
				stencil = static_cast<UINT8>(_vClearValues[(subpass._depthStencil._index << 2) + 1]);
			}
			_pDeviceContext->ClearDepthStencilView(fs._pDSV, clearFlags, _vClearValues[subpass._depthStencil._index << 2], stencil);
		}
	}

	ID3D11RenderTargetView* rtvs[VERUS_MAX_CA] = {};
	const int count = Math::Min(static_cast<int>(fs._vRTVs.size()), VERUS_MAX_CA);
	if (!fs._vRTVs.empty())
		memcpy(rtvs, fs._vRTVs.data(), count * sizeof(ID3D11RenderTargetView*));
	ID3D11DepthStencilView* pDSV = fs._pDSV ? fs._pDSV : nullptr;
	_pDeviceContext->OMSetRenderTargets(
		Utils::Cast32(subpass._vColor.size()),
		!fs._vRTVs.empty() ? rtvs : nullptr,
		fs._pDSV ? pDSV : nullptr);
}
