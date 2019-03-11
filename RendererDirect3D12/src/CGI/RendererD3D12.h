#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererD3D12 : public Singleton<RendererD3D12>, public BaseRenderer
		{
			CComPtr<IDXGISwapChain4>                _pSwapChain;
			CComPtr<ID3D12Device3>                  _pDevice;
			CComPtr<ID3D12CommandQueue>             _pCommandQueue;
			CComPtr<ID3D12Fence>                    _pFence;
			CComPtr<ID3D12DescriptorHeap>           _swapChainBuffersRTVs;
			Vector<CComPtr<ID3D12Resource>>         _vSwapChainBuffers;
			Vector<CComPtr<ID3D12CommandAllocator>> _vCommandAllocators;
			Vector<UINT64>                          _vFenceValues;
			CComPtr<ID3D12GraphicsCommandList>      _pCommandList;
			DXGI_SWAP_CHAIN_DESC1                   _swapChainDesc = {};
			HANDLE                                  _hFence = 0;
			UINT64                                  _fenceValue = 0;
			UINT                                    _descHandleIncSizeRTV = 0;
			UINT                                    _currentBackBufferIndex = 0;

		public:
			RendererD3D12();
			~RendererD3D12();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

			VERUS_P(static void EnableDebugLayer());
			VERUS_P(static CComPtr<IDXGIFactory7> CreateDXGIFactory());
			VERUS_P(static CComPtr<IDXGIAdapter4> GetAdapter(CComPtr<IDXGIFactory7> pFactory));
			VERUS_P(static bool CheckFeatureSupportAllowTearing(CComPtr<IDXGIFactory7> pFactory));
			VERUS_P(void CreateSwapChainBuffersRTVs());
			VERUS_P(void InitD3D());

			virtual Gapi GetGapi() override { return Gapi::direct3D12; }

			CComPtr<ID3D12CommandQueue> CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
			CComPtr<ID3D12CommandAllocator> CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);
			CComPtr<ID3D12GraphicsCommandList> CreateCommandList(D3D12_COMMAND_LIST_TYPE type, CComPtr<ID3D12CommandAllocator> pCommandAllocator);
			CComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num);
			CComPtr<ID3D12Fence> CreateFence();
			UINT64 Signal();
			void WaitForFenceValue(UINT64 value);
			void Flush();

			static D3D12_RESOURCE_BARRIER MakeResourceBarrierTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

			virtual void PrepareDraw() override;
			virtual void Clear(UINT32 flags) override;
			virtual void Present() override;
		};
		VERUS_TYPEDEFS(RendererD3D12);
	}
}

#define VERUS_QREF_RENDERER_D3D12 CGI::PRendererD3D12 pRendererD3D12 = CGI::RendererD3D12::P()
