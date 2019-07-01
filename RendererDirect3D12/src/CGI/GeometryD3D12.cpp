#include "stdafx.h"

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

	_32bitIndices = desc._32bitIndices;

	_vInputElementDesc.reserve(GetNumInputElementDesc(desc._pInputElementDesc));
	int i = 0;
	while (desc._pInputElementDesc[i]._offset >= 0)
	{
		int binding = desc._pInputElementDesc[i]._binding;
		D3D12_INPUT_CLASSIFICATION inputClassification = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		UINT instanceDataStepRate = 0;
		if (binding < 0)
		{
			binding = -binding;
			_bindingInstMask |= (1 << binding);
			inputClassification = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
			instanceDataStepRate = 1;
		}
		D3D12_INPUT_ELEMENT_DESC ieDesc =
		{
			ToNativeSemanticName(desc._pInputElementDesc[i]._usage),
			static_cast<UINT>(desc._pInputElementDesc[i]._usageIndex),
			ToNativeFormat(desc._pInputElementDesc[i]._usage, desc._pInputElementDesc[i]._type, desc._pInputElementDesc[i]._components),
			static_cast<UINT>(binding),
			static_cast<UINT>(desc._pInputElementDesc[i]._offset),
			inputClassification,
			instanceDataStepRate
		};
		_vInputElementDesc.push_back(ieDesc);
		i++;
	}

	_vStrides.reserve(GetNumBindings(desc._pInputElementDesc));
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
	_destroyStagingBuffers.Allow();
	DestroyStagingBuffers();

	VERUS_COM_RELEASE_CHECK(_pIndexBuffer.Get());
	for (auto& x : _vVertexBuffers)
		VERUS_COM_RELEASE_CHECK(x._pBuffer.Get());

	_pIndexBuffer.Reset();
	_vVertexBuffers.clear();

	VERUS_DONE(GeometryD3D12);
}

void GeometryD3D12::CreateVertexBuffer(int num, int binding)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if (_vVertexBuffers.size() <= binding)
		_vVertexBuffers.resize(binding + 1);

	auto& vb = _vVertexBuffers[binding];
	const int elementSize = _vStrides[binding];
	vb._bufferSize = num * elementSize;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vb._bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vb._pBuffer))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

	vb._bufferView.BufferLocation = vb._pBuffer->GetGPUVirtualAddress();
	vb._bufferView.SizeInBytes = Utils::Cast32(vb._bufferSize);
	vb._bufferView.StrideInBytes = elementSize;
}

void GeometryD3D12::UpdateVertexBuffer(const void* p, int binding, BaseCommandBuffer* pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if (_vStagingVertexBuffers.size() <= binding)
		_vStagingVertexBuffers.resize(binding + 1);

	auto& vb = _vVertexBuffers[binding];
	auto& svb = _vStagingVertexBuffers[binding];
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vb._bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&svb._pBuffer))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_UPLOAD), hr=" << VERUS_HR(hr);

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	D3D12_SUBRESOURCE_DATA sd = {};
	sd.pData = p;
	sd.RowPitch = vb._bufferSize;
	sd.SlicePitch = sd.RowPitch;
	UpdateSubresources(pCmdList,
		vb._pBuffer.Get(),
		svb._pBuffer.Get(),
		0, 0, 1, &sd);

	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		vb._pBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	pCmdList->ResourceBarrier(1, &barrier);

	_destroyStagingBuffers.Schedule();
}

void GeometryD3D12::CreateIndexBuffer(int num)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const int elementSize = _32bitIndices ? sizeof(UINT32) : sizeof(UINT16);
	_indexBufferSize = num * elementSize;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(_indexBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&_pIndexBuffer))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

	_indexBufferView.BufferLocation = _pIndexBuffer->GetGPUVirtualAddress();
	_indexBufferView.SizeInBytes = Utils::Cast32(_indexBufferSize);
	_indexBufferView.Format = _32bitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
}

void GeometryD3D12::UpdateIndexBuffer(const void* p, BaseCommandBuffer* pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(_indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&_pStagingIndexBuffer))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_UPLOAD), hr=" << VERUS_HR(hr);

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());
	auto pCmdList = static_cast<PCommandBufferD3D12>(pCB)->GetD3DGraphicsCommandList();
	D3D12_SUBRESOURCE_DATA sd = {};
	sd.pData = p;
	sd.RowPitch = _indexBufferSize;
	sd.SlicePitch = sd.RowPitch;
	UpdateSubresources(pCmdList,
		_pIndexBuffer.Get(),
		_pStagingIndexBuffer.Get(),
		0, 0, 1, &sd);

	const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_pIndexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	pCmdList->ResourceBarrier(1, &barrier);

	_destroyStagingBuffers.Schedule();
}

void GeometryD3D12::DestroyStagingBuffers()
{
	if (!_destroyStagingBuffers.IsAllowed())
		return;

	VERUS_COM_RELEASE_CHECK(_pStagingIndexBuffer.Get());
	for (auto& x : _vStagingVertexBuffers)
		VERUS_COM_RELEASE_CHECK(x._pBuffer.Get());

	_pStagingIndexBuffer.Reset();
	_vStagingVertexBuffers.clear();
}

D3D12_INPUT_LAYOUT_DESC GeometryD3D12::GetD3DInputLayoutDesc(UINT32 bindingsFilter, Vector<D3D12_INPUT_ELEMENT_DESC>& vInputElementDesc) const
{
	if (UINT32_MAX == bindingsFilter)
		return { _vInputElementDesc.data(), Utils::Cast32(_vInputElementDesc.size()) };

	uint32_t replaceBinding[VERUS_CGI_MAX_VB] = {}; // For bindings compaction.
	int binding = 0;
	VERUS_FOR(i, VERUS_CGI_MAX_VB)
	{
		replaceBinding[i] = binding;
		if ((bindingsFilter >> i) & 0x1)
			binding++;
	}

	vInputElementDesc.reserve(_vInputElementDesc.size());
	for (auto& x : _vInputElementDesc)
	{
		if ((bindingsFilter >> x.InputSlot) & 0x1)
		{
			vInputElementDesc.push_back(x);
			vInputElementDesc.back().InputSlot = replaceBinding[x.InputSlot];
		}
	}

	return { vInputElementDesc.data(), Utils::Cast32(vInputElementDesc.size()) };
}

const D3D12_VERTEX_BUFFER_VIEW* GeometryD3D12::GetD3DVertexBufferView(int binding) const
{
	return &_vVertexBuffers[binding]._bufferView;
}

const D3D12_INDEX_BUFFER_VIEW* GeometryD3D12::GetD3DIndexBufferView() const
{
	return &_indexBufferView;
}
