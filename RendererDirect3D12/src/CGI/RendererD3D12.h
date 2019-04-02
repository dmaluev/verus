#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererD3D12 : public Singleton<RendererD3D12>, public BaseRenderer
		{
			typedef Map<std::thread::id, ComPtr<ID3D12CommandAllocator>> TMapCommandAllocators;

			ComPtr<ID3D12Device3>             _pDevice;
			ComPtr<ID3D12CommandQueue>        _pCommandQueue;
			ComPtr<IDXGISwapChain4>           _pSwapChain;
			Vector<ComPtr<ID3D12Resource>>    _vSwapChainBuffers;
			ComPtr<ID3D12DescriptorHeap>      _dSwapChainBuffersRTVs;
			TMapCommandAllocators             _mapCommandAllocators[ringBufferSize];
			ComPtr<ID3D12GraphicsCommandList> _pCommandLists[ringBufferSize];
			ComPtr<ID3D12Fence>               _pFence;
			HANDLE                            _hFence = INVALID_HANDLE_VALUE;
			UINT64                            _nextFenceValue = 1;
			UINT64                            _fenceValues[ringBufferSize];
			UINT                              _descHandleIncSizeRTV = 0;
			DXGI_SWAP_CHAIN_DESC1             _swapChainDesc = {};

		public:
			RendererD3D12();
			~RendererD3D12();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

		private:
			static void EnableDebugLayer();
			static ComPtr<IDXGIFactory6> CreateDXGIFactory();
			static ComPtr<IDXGIAdapter4> GetAdapter(ComPtr<IDXGIFactory6> pFactory);
			static bool CheckFeatureSupportAllowTearing(ComPtr<IDXGIFactory6> pFactory);
			void CreateSwapChainBuffersRTVs();
			void InitD3D();

		public:
			ComPtr<ID3D12CommandQueue> CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
			ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);
			ComPtr<ID3D12GraphicsCommandList> CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> pCommandAllocator);
			ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num);
			ComPtr<ID3D12Fence> CreateFence();
			UINT64 QueueSignal();
			void WaitForFenceValue(UINT64 value);
			void QueueWaitIdle();

			static D3D12_RESOURCE_BARRIER MakeResourceBarrierTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::direct3D12; }

			// Frame cycle:
			virtual void BeginFrame() override;
			virtual void EndFrame() override;
			virtual void Present() override;
			virtual void Clear(UINT32 flags) override;
		};
		VERUS_TYPEDEFS(RendererD3D12);
	}
}

#define VERUS_QREF_RENDERER_D3D12 CGI::PRendererD3D12 pRendererD3D12 = CGI::RendererD3D12::P()
