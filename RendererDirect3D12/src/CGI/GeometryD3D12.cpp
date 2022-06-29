// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

GeometryD3D12::GeometryD3D12()
{
	VERUS_ZERO_MEM(_indexBufferView);
}

GeometryD3D12::~GeometryD3D12()
{
	Done();
}

void GeometryD3D12::Init(RcGeometryDesc desc)
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
		D3D12_INPUT_CLASSIFICATION inputClassification = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		UINT instanceDataStepRate = 0;
		if (binding < 0)
		{
			binding = -binding;
			_instBindingsMask |= (1 << binding);
			inputClassification = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
			instanceDataStepRate = 1;
		}
		D3D12_INPUT_ELEMENT_DESC ieDesc =
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
	_vStagingVertexBuffers.reserve(4);
}

void GeometryD3D12::Done()
{
	ForceScheduled();

	VERUS_SMART_RELEASE(_indexBuffer._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_indexBuffer._pBuffer.Get());
	_indexBuffer._pBuffer.Reset();
	for (auto& x : _vVertexBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pBuffer.Get());
		x._pBuffer.Reset();
	}
	_vVertexBuffers.clear();

	VERUS_DONE(GeometryD3D12);
}

void GeometryD3D12::CreateVertexBuffer(int count, int binding)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if (_vVertexBuffers.size() <= binding)
		_vVertexBuffers.resize(binding + 1);

	auto& vb = _vVertexBuffers[binding];
	const int elementSize = _vStrides[binding];
	vb._bufferSize = count * elementSize;

	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vb._bufferSize * BaseRenderer::s_ringBufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&vb._pMaAllocation,
			IID_PPV_ARGS(&vb._pBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
		vb._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (Dynamic VB, " + std::to_string(binding) + ")")));

		VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		{
			vb._bufferView[i].BufferLocation = vb._pBuffer->GetGPUVirtualAddress() + i * vb._bufferSize;
			vb._bufferView[i].SizeInBytes = Utils::Cast32(vb._bufferSize);
			vb._bufferView[i].StrideInBytes = elementSize;
		}
	}
	else
	{
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vb._bufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			&vb._pMaAllocation,
			IID_PPV_ARGS(&vb._pBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT); hr=" << VERUS_HR(hr);
		vb._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (VB, " + std::to_string(binding) + ")")));

		vb._bufferView[0].BufferLocation = vb._pBuffer->GetGPUVirtualAddress();
		vb._bufferView[0].SizeInBytes = Utils::Cast32(vb._bufferSize);
		vb._bufferView[0].StrideInBytes = elementSize;
	}
}

void GeometryD3D12::UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB, INT64 size, INT64 offset)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		auto& vb = _vVertexBuffers[binding];
		const int elementSize = _vStrides[binding];
		size = size ? size * elementSize : vb._bufferSize;

		CD3DX12_RANGE readRange(0, 0);
		void* pData = nullptr;
		if (FAILED(hr = vb._pBuffer->Map(0, &readRange, &pData)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pMappedData = static_cast<BYTE*>(pData) + pRendererD3D12->GetRingBufferIndex() * vb._bufferSize;
		memcpy(pMappedData + offset * elementSize, p, size);
		vb._pBuffer->Unmap(0, nullptr);
		vb._utilization = offset * elementSize + size;
	}
	else
	{
		if (_vStagingVertexBuffers.size() <= binding)
			_vStagingVertexBuffers.resize(binding + 1);

		bool revertState = true;
		auto& vb = _vVertexBuffers[binding];
		auto& svb = _vStagingVertexBuffers[binding];
		if (!svb._pBuffer)
		{
			revertState = false;
			D3D12MA::ALLOCATION_DESC allocDesc = {};
			allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vb._bufferSize);
			if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
				&allocDesc,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				&svb._pMaAllocation,
				IID_PPV_ARGS(&svb._pBuffer))))
				throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
			vb._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (Staging VB, " + std::to_string(binding) + ")")));
		}

		if (!pCB)
			pCB = renderer.GetCommandBuffer().Get();
		auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();

		if (revertState)
		{
			const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				vb._pBuffer.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
			pCmdList->ResourceBarrier(1, &barrier);
		}
		D3D12_SUBRESOURCE_DATA sd = {};
		sd.pData = p;
		sd.RowPitch = vb._bufferSize;
		sd.SlicePitch = sd.RowPitch;
		UpdateSubresources<1>(pCmdList,
			vb._pBuffer.Get(),
			svb._pBuffer.Get(), 0,
			0, 1, &sd);
		const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			vb._pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		pCmdList->ResourceBarrier(1, &barrier);

		Schedule();
	}
}

void GeometryD3D12::CreateIndexBuffer(int count)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const int elementSize = _32BitIndices ? sizeof(UINT32) : sizeof(UINT16);
	_indexBuffer._bufferSize = count * elementSize;

	if ((_dynBindingsMask >> 31) & 0x1)
	{
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(_indexBuffer._bufferSize * BaseRenderer::s_ringBufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&_indexBuffer._pMaAllocation,
			IID_PPV_ARGS(&_indexBuffer._pBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
		_indexBuffer._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (Dynamic IB)")));

		VERUS_FOR(i, BaseRenderer::s_ringBufferSize)
		{
			_indexBufferView[i].BufferLocation = _indexBuffer._pBuffer->GetGPUVirtualAddress() + i * _indexBuffer._bufferSize;
			_indexBufferView[i].SizeInBytes = Utils::Cast32(_indexBuffer._bufferSize);
			_indexBufferView[i].Format = _32BitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		}
	}
	else
	{
		D3D12MA::ALLOCATION_DESC allocDesc = {};
		allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(_indexBuffer._bufferSize);
		if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			&_indexBuffer._pMaAllocation,
			IID_PPV_ARGS(&_indexBuffer._pBuffer))))
			throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_DEFAULT); hr=" << VERUS_HR(hr);
		_indexBuffer._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (IB)")));

		_indexBufferView[0].BufferLocation = _indexBuffer._pBuffer->GetGPUVirtualAddress();
		_indexBufferView[0].SizeInBytes = Utils::Cast32(_indexBuffer._bufferSize);
		_indexBufferView[0].Format = _32BitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	}
}

void GeometryD3D12::UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB, INT64 size, INT64 offset)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if ((_dynBindingsMask >> 31) & 0x1)
	{
		const int elementSize = _32BitIndices ? sizeof(UINT32) : sizeof(UINT16);
		size = size ? size * elementSize : _indexBuffer._bufferSize;

		CD3DX12_RANGE readRange(0, 0);
		void* pData = nullptr;
		if (FAILED(hr = _indexBuffer._pBuffer->Map(0, &readRange, &pData)))
			throw VERUS_RUNTIME_ERROR << "Map(); hr=" << VERUS_HR(hr);
		BYTE* pMappedData = static_cast<BYTE*>(pData) + pRendererD3D12->GetRingBufferIndex() * _indexBuffer._bufferSize;
		memcpy(pMappedData + offset * elementSize, p, size);
		_indexBuffer._pBuffer->Unmap(0, nullptr);
		_indexBuffer._utilization = offset * elementSize + size;
	}
	else
	{
		bool revertState = true;
		if (!_stagingIndexBuffer._pBuffer)
		{
			revertState = false;
			D3D12MA::ALLOCATION_DESC allocDesc = {};
			allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			const auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(_indexBuffer._bufferSize);
			if (FAILED(hr = pRendererD3D12->GetMaAllocator()->CreateResource(
				&allocDesc,
				&resDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				&_stagingIndexBuffer._pMaAllocation,
				IID_PPV_ARGS(&_stagingIndexBuffer._pBuffer))))
				throw VERUS_RUNTIME_ERROR << "CreateResource(D3D12_HEAP_TYPE_UPLOAD); hr=" << VERUS_HR(hr);
			_stagingIndexBuffer._pBuffer->SetName(_C(Str::Utf8ToWide(_name + " (Staging IB)")));
		}

		if (!pCB)
			pCB = renderer.GetCommandBuffer().Get();
		auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();

		if (revertState)
		{
			const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				_indexBuffer._pBuffer.Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
			pCmdList->ResourceBarrier(1, &barrier);
		}
		D3D12_SUBRESOURCE_DATA sd = {};
		sd.pData = p;
		sd.RowPitch = _indexBuffer._bufferSize;
		sd.SlicePitch = sd.RowPitch;
		UpdateSubresources<1>(pCmdList,
			_indexBuffer._pBuffer.Get(),
			_stagingIndexBuffer._pBuffer.Get(), 0,
			0, 1, &sd);
		const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			_indexBuffer._pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		pCmdList->ResourceBarrier(1, &barrier);

		Schedule();
	}
}

Continue GeometryD3D12::Scheduled_Update()
{
	if (!IsScheduledAllowed())
		return Continue::yes;

	VERUS_SMART_RELEASE(_stagingIndexBuffer._pMaAllocation);
	VERUS_COM_RELEASE_CHECK(_stagingIndexBuffer._pBuffer.Get());
	_stagingIndexBuffer._pBuffer.Reset();
	for (auto& x : _vStagingVertexBuffers)
	{
		VERUS_SMART_RELEASE(x._pMaAllocation);
		VERUS_COM_RELEASE_CHECK(x._pBuffer.Get());
		x._pBuffer.Reset();
	}
	_vStagingVertexBuffers.clear();

	return Continue::no;
}

D3D12_INPUT_LAYOUT_DESC GeometryD3D12::GetD3DInputLayoutDesc(UINT32 bindingsFilter, Vector<D3D12_INPUT_ELEMENT_DESC>& vInputElementDescs) const
{
	if (UINT32_MAX == bindingsFilter)
		return { _vInputElementDescs.data(), Utils::Cast32(_vInputElementDescs.size()) };

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

	return { vInputElementDescs.data(), Utils::Cast32(vInputElementDescs.size()) };
}

const D3D12_VERTEX_BUFFER_VIEW* GeometryD3D12::GetD3DVertexBufferView(int binding) const
{
	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		VERUS_QREF_RENDERER_D3D12;
		return &_vVertexBuffers[binding]._bufferView[pRendererD3D12->GetRingBufferIndex()];
	}
	else
		return &_vVertexBuffers[binding]._bufferView[0];
}

const D3D12_INDEX_BUFFER_VIEW* GeometryD3D12::GetD3DIndexBufferView() const
{
	if ((_dynBindingsMask >> 31) & 0x1)
	{
		VERUS_QREF_RENDERER_D3D12;
		return &_indexBufferView[pRendererD3D12->GetRingBufferIndex()];
	}
	else
		return &_indexBufferView[0];
}

void GeometryD3D12::UpdateUtilization()
{
	VERUS_QREF_RENDERER;
	int binding = 0;
	for (const auto& vb : _vVertexBuffers)
	{
		if (vb._utilization >= 0)
		{
			StringStream ss;
			ss << "binding=" << binding << ", " << _name;
			renderer.AddUtilization(_C(ss.str()), vb._utilization, vb._bufferSize);
		}
		binding++;
	}
	if (_indexBuffer._utilization >= 0)
	{
		StringStream ss;
		ss << "ib, " << _name;
		renderer.AddUtilization(_C(ss.str()), _indexBuffer._utilization, _indexBuffer._bufferSize);
	}
}
