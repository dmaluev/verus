#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

GeometryD3D12::GeometryD3D12()
{
	VERUS_ZERO_MEM(_vertexBufferView);
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
}

void GeometryD3D12::Done()
{
	VERUS_DONE(GeometryD3D12);
}

void GeometryD3D12::BufferDataVB(const void* p, int num, int binding)
{
	const int elementSize = _vStrides[binding];

	BufferData(&_pVertexBuffer, &_pStagingVertexBuffer, num, elementSize, p);

	_vertexBufferView.BufferLocation = _pVertexBuffer->GetGPUVirtualAddress();
	_vertexBufferView.SizeInBytes = num * elementSize;
	_vertexBufferView.StrideInBytes = elementSize;
}

void GeometryD3D12::BufferDataIB(const void* p, int num)
{
	const int elementSize = _32bitIndices ? sizeof(UINT32) : sizeof(UINT16);

	BufferData(&_pIndexBuffer, &_pStagingIndexBuffer, num, elementSize, p);

	_indexBufferView.BufferLocation = _pIndexBuffer->GetGPUVirtualAddress();
	_indexBufferView.SizeInBytes = num * elementSize;
	_indexBufferView.Format = _32bitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
}

void GeometryD3D12::BufferData(
	ID3D12Resource** ppDestRes,
	ID3D12Resource** ppTempRes,
	size_t numElements,
	size_t elementSize,
	const void* p,
	D3D12_RESOURCE_FLAGS flags)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	const size_t bufferSize = numElements * elementSize;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(ppDestRes))))
		throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_DEFAULT), hr=" << VERUS_HR(hr);

	if (p)
	{
		if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(ppTempRes))))
			throw VERUS_RUNTIME_ERROR << "CreateCommittedResource(D3D12_HEAP_TYPE_UPLOAD), hr=" << VERUS_HR(hr);

		auto pCmdList = static_cast<PCommandBufferD3D12>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList();
		D3D12_SUBRESOURCE_DATA sd = {};
		sd.pData = p;
		sd.RowPitch = bufferSize;
		sd.SlicePitch = sd.RowPitch;
		UpdateSubresources(pCmdList,
			*ppDestRes,
			*ppTempRes,
			0, 0, 1, &sd);
	}
}

D3D12_INPUT_LAYOUT_DESC GeometryD3D12::GetD3DInputLayoutDesc() const
{
	return { _vInputElementDesc.data(), Utils::Cast32(_vInputElementDesc.size()) };
}

const D3D12_VERTEX_BUFFER_VIEW* GeometryD3D12::GetD3DVertexBufferView() const
{
	return &_vertexBufferView;
}

const D3D12_INDEX_BUFFER_VIEW* GeometryD3D12::GetD3DIndexBufferView() const
{
	return &_indexBufferView;
}
