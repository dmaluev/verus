// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class DescriptorHeap
	{
		ComPtr<ID3D12DescriptorHeap>  _pDescriptorHeap;
		CD3DX12_CPU_DESCRIPTOR_HANDLE _hCPUHandleForHeapStart;
		CD3DX12_GPU_DESCRIPTOR_HANDLE _hGPUHandleForHeapStart;
		D3D12_DESCRIPTOR_HEAP_TYPE    _type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		UINT                          _handleIncrementSize = 0;

	public:
		void Reset() { VERUS_COM_RELEASE_CHECK(_pDescriptorHeap.Get()); _pDescriptorHeap.Reset(); }

		ID3D12DescriptorHeap* GetD3DDescriptorHeap() const { return _pDescriptorHeap.Get(); }

		void Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, int count, bool shaderVisible = false);

		CD3DX12_CPU_DESCRIPTOR_HANDLE AtCPU(int index) const;
		CD3DX12_GPU_DESCRIPTOR_HANDLE AtGPU(int index) const;

		D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return _type; }
		UINT GetHandleIncrementSize() const { return _handleIncrementSize; }
	};
	VERUS_TYPEDEFS(DescriptorHeap);

	struct HandlePair
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE _hCPU;
		CD3DX12_GPU_DESCRIPTOR_HANDLE _hGPU;

		HandlePair(
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCPU = D3D12_DEFAULT,
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGPU = D3D12_DEFAULT) : _hCPU(hCPU), _hGPU(hGPU) {}
	};
	VERUS_TYPEDEFS(HandlePair);

	// This descriptor heap should be refilled every frame:
	class DynamicDescriptorHeap : public DescriptorHeap
	{
		int    _capacity = 0;
		int    _offset = 0;
		UINT64 _currentFrame = UINT64_MAX;
		UINT64 _peakLoad = 0;

	public:
		void Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, int count, int staticCount = 0, bool shaderVisible = false);

		HandlePair GetNextHandlePair(int count = 1);
		HandlePair GetStaticHandlePair(int index);

		int GetCapacity() const { return _capacity; }
		int GetOffset() const { return _offset; }
	};
	VERUS_TYPEDEFS(DynamicDescriptorHeap);
}
