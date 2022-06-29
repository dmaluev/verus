// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

GeometryD3D11::GeometryD3D11()
{
}

GeometryD3D11::~GeometryD3D11()
{
	Done();
}

void GeometryD3D11::Init(RcGeometryDesc desc)
{
	VERUS_INIT();

	_name = desc._name;
	_dynBindingsMask = desc._dynBindingsMask;
	_32BitIndices = desc._32BitIndices;

	_vInputElementDescs.reserve(GetVertexInputAttrDescCount(desc._pVertexInputAttrDesc));
	int i = 0;
	while (desc._pVertexInputAttrDesc[i]._offset >= 0)
	{
		int binding = desc._pVertexInputAttrDesc[i]._binding;
		D3D11_INPUT_CLASSIFICATION inputClassification = D3D11_INPUT_PER_VERTEX_DATA;
		UINT instanceDataStepRate = 0;
		if (binding < 0)
		{
			binding = -binding;
			_instBindingsMask |= (1 << binding);
			inputClassification = D3D11_INPUT_PER_INSTANCE_DATA;
			instanceDataStepRate = 1;
		}
		D3D11_INPUT_ELEMENT_DESC ieDesc =
		{
			ToNativeSemanticName(desc._pVertexInputAttrDesc[i]._usage),
			static_cast<UINT>(desc._pVertexInputAttrDesc[i]._usageIndex),
			ToNativeFormat(desc._pVertexInputAttrDesc[i]._usage, desc._pVertexInputAttrDesc[i]._type, desc._pVertexInputAttrDesc[i]._components),
			static_cast<UINT>(binding),
			static_cast<UINT>(desc._pVertexInputAttrDesc[i]._offset),
			inputClassification,
			instanceDataStepRate
		};
		_vInputElementDescs.push_back(ieDesc);
		i++;
	}

	_vStrides.reserve(GetBindingCount(desc._pVertexInputAttrDesc));
	i = 0;
	while (desc._pStrides[i] > 0)
	{
		_vStrides.push_back(desc._pStrides[i]);
		i++;
	}

	_vVertexBuffers.reserve(4);
}

void GeometryD3D11::Done()
{
	ForceScheduled();

	_indexBuffer._pBuffer.Reset();
	for (auto& x : _vVertexBuffers)
		x._pBuffer.Reset();
	_vVertexBuffers.clear();

	VERUS_DONE(GeometryD3D11);
}

void GeometryD3D11::CreateVertexBuffer(int count, int binding)
{
	VERUS_QREF_RENDERER_D3D11;
	HRESULT hr = 0;

	if (_vVertexBuffers.size() <= binding)
		_vVertexBuffers.resize(binding + 1);

	auto& vb = _vVertexBuffers[binding];
	const int elementSize = _vStrides[binding];
	vb._bufferSize = count * elementSize;

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = Utils::Cast32(vb._bufferSize);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		vbDesc.Usage = D3D11_USAGE_DYNAMIC;
		vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBuffer(&vbDesc, nullptr, &vb._pBuffer)))
			throw VERUS_RUNTIME_ERROR << "CreateBuffer(D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC), " << VERUS_HR(hr);
		SetDebugObjectName(vb._pBuffer.Get(), _C(_name + " (Dynamic VB, " + std::to_string(binding) + ")"));
	}
	else
	{
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBuffer(&vbDesc, nullptr, &vb._pBuffer)))
			throw VERUS_RUNTIME_ERROR << "CreateBuffer(D3D11_BIND_VERTEX_BUFFER), " << VERUS_HR(hr);
		SetDebugObjectName(vb._pBuffer.Get(), _C(_name + " (VB, " + std::to_string(binding) + ")"));
	}
}

void GeometryD3D11::UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB, INT64 size, INT64 offset)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();

	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		auto& vb = _vVertexBuffers[binding];
		const int elementSize = _vStrides[binding];
		size = size ? size * elementSize : vb._bufferSize;

		const D3D11_MAP mapType = (size == vb._bufferSize) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
		D3D11_MAPPED_SUBRESOURCE ms = {};
		if (FAILED(hr = pDeviceContext->Map(vb._pBuffer.Get(), 0, mapType, 0, &ms)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pMappedData = static_cast<BYTE*>(ms.pData);
		memcpy(pMappedData + offset * elementSize, p, size);
		pDeviceContext->Unmap(vb._pBuffer.Get(), 0);
	}
	else
	{
		auto& vb = _vVertexBuffers[binding];
		pDeviceContext->UpdateSubresource(
			vb._pBuffer.Get(), 0, nullptr,
			p, 0, 0);
	}
}

void GeometryD3D11::CreateIndexBuffer(int count)
{
	VERUS_QREF_RENDERER_D3D11;
	HRESULT hr = 0;

	const int elementSize = _32BitIndices ? sizeof(UINT32) : sizeof(UINT16);
	_indexBuffer._bufferSize = count * elementSize;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = Utils::Cast32(_indexBuffer._bufferSize);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	if ((_dynBindingsMask >> 31) & 0x1)
	{
		ibDesc.Usage = D3D11_USAGE_DYNAMIC;
		ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBuffer(&ibDesc, nullptr, &_indexBuffer._pBuffer)))
			throw VERUS_RUNTIME_ERROR << "CreateBuffer(D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC), " << VERUS_HR(hr);
		SetDebugObjectName(_indexBuffer._pBuffer.Get(), _C(_name + " (Dynamic IB)"));
	}
	else
	{
		if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBuffer(&ibDesc, nullptr, &_indexBuffer._pBuffer)))
			throw VERUS_RUNTIME_ERROR << "CreateBuffer(D3D11_BIND_VERTEX_BUFFER), " << VERUS_HR(hr);
		SetDebugObjectName(_indexBuffer._pBuffer.Get(), _C(_name + " (IB)"));
	}
}

void GeometryD3D11::UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB, INT64 size, INT64 offset)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();
	auto pDeviceContext = static_cast<PCommandBufferD3D11>(pCB)->GetD3DDeviceContext();

	if ((_dynBindingsMask >> 31) & 0x1)
	{
		const int elementSize = _32BitIndices ? sizeof(UINT32) : sizeof(UINT16);
		size = size ? size * elementSize : _indexBuffer._bufferSize;

		const D3D11_MAP mapType = (size == _indexBuffer._bufferSize) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
		D3D11_MAPPED_SUBRESOURCE ms = {};
		if (FAILED(hr = pDeviceContext->Map(_indexBuffer._pBuffer.Get(), 0, mapType, 0, &ms)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pMappedData = static_cast<BYTE*>(ms.pData);
		memcpy(pMappedData + offset * elementSize, p, size);
		pDeviceContext->Unmap(_indexBuffer._pBuffer.Get(), 0);
	}
	else
	{
		pDeviceContext->UpdateSubresource(
			_indexBuffer._pBuffer.Get(), 0, nullptr,
			p, 0, 0);
	}
}

Continue GeometryD3D11::Scheduled_Update()
{
	return Continue::yes;
}

void GeometryD3D11::GetD3DInputElementDescs(UINT32 bindingsFilter, Vector<D3D11_INPUT_ELEMENT_DESC>& vInputElementDescs) const
{
	UINT replaceBinding[VERUS_MAX_VB] = {}; // For bindings compaction.
	UINT binding = 0;
	VERUS_FOR(i, VERUS_MAX_VB)
	{
		replaceBinding[i] = binding;
		if ((bindingsFilter >> i) & 0x1)
			binding++;
	}

	vInputElementDescs.reserve(_vInputElementDescs.size());
	for (const auto& x : _vInputElementDescs)
	{
		if ((bindingsFilter >> x.InputSlot) & 0x1)
		{
			vInputElementDescs.push_back(x);
			vInputElementDescs.back().InputSlot = replaceBinding[x.InputSlot];
		}
	}
}

ID3D11Buffer* GeometryD3D11::GetD3DVertexBuffer(int binding) const
{
	return _vVertexBuffers[binding]._pBuffer.Get();
}

ID3D11Buffer* GeometryD3D11::GetD3DIndexBuffer() const
{
	return _indexBuffer._pBuffer.Get();
}
