#pragma once

namespace verus
{
	namespace CGI
	{
		typedef Store<CommandBufferD3D12> TStoreCommandBuffers;
		typedef Store<GeometryD3D12>      TStoreGeometry;
		typedef Store<PipelineD3D12>      TStorePipelines;
		typedef Store<ShaderD3D12>        TStoreShaders;
		typedef Store<TextureD3D12>       TStoreTextures;
		class RendererD3D12 : public Singleton<RendererD3D12>, public BaseRenderer,
			private TStoreCommandBuffers, private TStoreGeometry, private TStorePipelines, private TStoreShaders, private TStoreTextures
		{
			typedef Map<std::thread::id, ComPtr<ID3D12CommandAllocator>> TMapCommandAllocators;

			ComPtr<ID3D12Device3>             _pDevice;
			ComPtr<ID3D12CommandQueue>        _pCommandQueue;
			D3D12MA::Allocator* _pMaAllocator = nullptr;
			ComPtr<IDXGISwapChain4>           _pSwapChain;
			Vector<ComPtr<ID3D12Resource>>    _vSwapChainBuffers;
			DescriptorHeap                    _dhSwapChainBuffersRTVs;
			DynamicDescriptorHeap             _dhCbvSrvUav;
			DynamicDescriptorHeap             _dhSamplers;
			TMapCommandAllocators             _mapCommandAllocators[s_ringBufferSize];
			ComPtr<ID3D12Fence>               _pFence;
			HANDLE                            _hFence = INVALID_HANDLE_VALUE;
			UINT64                            _nextFenceValue = 1;
			UINT64                            _fenceValues[s_ringBufferSize] = {};
			Vector<D3D12_STATIC_SAMPLER_DESC> _vSamplers;
			Vector<RP::D3DRenderPass>         _vRenderPasses;
			Vector<RP::D3DFramebuffer>        _vFramebuffers;
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
			ComPtr<ID3D12CommandQueue>         CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
			ComPtr<ID3D12CommandAllocator>     CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);
			ComPtr<ID3D12GraphicsCommandList3> CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> pCommandAllocator);
			ComPtr<ID3D12Fence>                CreateFence();
			UINT64 QueueSignal();
			void WaitForFenceValue(UINT64 value);
			void QueueWaitIdle();
			void CreateSamplers();

			ID3D12Device3* GetD3DDevice() const { return _pDevice.Get(); }
			D3D12MA::Allocator* GetMaAllocator() const { return _pMaAllocator; }
			ID3D12CommandAllocator* GetD3DCommandAllocator(int ringBufferIndex) const { return _mapCommandAllocators[ringBufferIndex].at(std::this_thread::get_id()).Get(); }
			D3D12_STATIC_SAMPLER_DESC GetStaticSamplerDesc(Sampler s) const;

			virtual void ImGuiInit(int renderPassHandle) override;
			virtual void ImGuiRenderDrawData() override;

			virtual void ResizeSwapChain() override;

			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::direct3D12; }

			// Frame cycle:
			virtual void BeginFrame(bool present) override;
			virtual void EndFrame(bool present) override;
			virtual void Present() override;
			virtual void WaitIdle() override;

			// Resources:
			virtual PBaseCommandBuffer InsertCommandBuffer() override;
			virtual PBaseGeometry      InsertGeometry() override;
			virtual PBasePipeline      InsertPipeline() override;
			virtual PBaseShader        InsertShader() override;
			virtual PBaseTexture       InsertTexture() override;

			virtual void DeleteCommandBuffer(PBaseCommandBuffer p) override;
			virtual void DeleteGeometry(PBaseGeometry p) override;
			virtual void DeletePipeline(PBasePipeline p) override;
			virtual void DeleteShader(PBaseShader p) override;
			virtual void DeleteTexture(PBaseTexture p) override;

			virtual int CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) override;
			virtual int CreateFramebuffer(int renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex = -1) override;
			virtual void DeleteRenderPass(int handle) override;
			virtual void DeleteFramebuffer(int handle) override;
			int GetNextRenderPassHandle() const;
			int GetNextFramebufferHandle() const;
			RP::RcD3DRenderPass GetRenderPass(int handle) const;
			RP::RcD3DFramebuffer GetFramebuffer(int handle) const;

			RDynamicDescriptorHeap GetHeapCbvSrvUav() { return _dhCbvSrvUav; }
			RDynamicDescriptorHeap GetHeapSampler() { return _dhSamplers; }
		};
		VERUS_TYPEDEFS(RendererD3D12);
	}
}

#define VERUS_QREF_RENDERER_D3D12 CGI::PRendererD3D12 pRendererD3D12 = CGI::RendererD3D12::P()
