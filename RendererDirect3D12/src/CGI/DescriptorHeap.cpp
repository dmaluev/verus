#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

// DescriptorHeap:

void DescriptorHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool shaderVisible)
{
	VERUS_CT_ASSERT(sizeof(*this) == 32);
	VERUS_RT_ASSERT(count > 0);
	HRESULT hr = 0;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = _type = type;
	desc.NumDescriptors = count;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_pDescriptorHeap))))
		throw VERUS_RUNTIME_ERROR << "CreateDescriptorHeap(), hr=" << VERUS_HR(hr);
	_hCPUHandleForHeapStart = _pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	_hGPUHandleForHeapStart = _pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	_handleIncrementSize = pDevice->GetDescriptorHandleIncrementSize(desc.Type);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::AtCPU(INT index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(_hCPUHandleForHeapStart, index, _handleIncrementSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::AtGPU(INT index) const
{
	VERUS_RT_ASSERT(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV == _type || D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER == _type);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(_hGPUHandleForHeapStart, index, _handleIncrementSize);
}

// DynamicDescriptorHeap:

void DynamicDescriptorHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, UINT staticCount, bool shaderVisible)
{
	VERUS_CT_ASSERT(sizeof(*this) == 56);
	_capacity = count;
	_offset = 0;
	DescriptorHeap::Create(pDevice, type, _capacity * BaseRenderer::s_ringBufferSize + staticCount, shaderVisible);
}

HandlePair DynamicDescriptorHeap::GetNextHandlePair(int count)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_D3D12;

	if (_currentFrame != renderer.GetFrameCount())
	{
		_currentFrame = renderer.GetFrameCount();
		_offset = 0;
	}

	if (_offset + count <= _capacity)
	{
		const INT index = _capacity * pRendererD3D12->GetRingBufferIndex() + _offset;
		_offset += count;
		_peakLoad = Math::Max<UINT64>(_peakLoad, _offset);
		return HandlePair(AtCPU(index), AtGPU(index));
	}
	VERUS_RT_FAIL("DynamicDescriptorHeap is full.");
	return HandlePair();
}

HandlePair DynamicDescriptorHeap::GetStaticHandlePair(int index)
{
	const INT indexAt = _capacity * BaseRenderer::s_ringBufferSize + index;
	return HandlePair(AtCPU(indexAt), AtGPU(indexAt));
}
