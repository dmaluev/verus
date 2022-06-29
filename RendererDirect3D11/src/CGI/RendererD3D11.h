// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		typedef Store<CommandBufferD3D11> TStoreCommandBuffers;
		typedef Store<GeometryD3D11>      TStoreGeometry;
		typedef Store<PipelineD3D11>      TStorePipelines;
		typedef Store<ShaderD3D11>        TStoreShaders;
		typedef Store<TextureD3D11>       TStoreTextures;
		class RendererD3D11 : public Singleton<RendererD3D11>, public BaseRenderer,
			private TStoreCommandBuffers, private TStoreGeometry, private TStorePipelines, private TStoreShaders, private TStoreTextures
		{
			ComPtr<ID3D11Device>               _pDevice;
			ComPtr<ID3D11DeviceContext>        _pDeviceContext;
			ComPtr<IDXGISwapChain>             _pSwapChain;
			ComPtr<ID3D11Texture2D>            _pSwapChainBuffer;
			ComPtr<ID3D11RenderTargetView>     _pSwapChainBufferRTV;

			Vector<ComPtr<ID3D11SamplerState>> _vSamplers;
			Vector<RP::D3DRenderPass>          _vRenderPasses;
			Vector<RP::D3DFramebuffer>         _vFramebuffers;
			D3D_FEATURE_LEVEL                  _featureLevel = D3D_FEATURE_LEVEL_11_0;
			DXGI_SWAP_CHAIN_DESC               _swapChainDesc = {};

		public:
			RendererD3D11();
			~RendererD3D11();

			virtual void ReleaseMe() override;

			void Init();
			void Done();

		private:
			static ComPtr<IDXGIFactory1> CreateFactory();
			static ComPtr<IDXGIAdapter1> GetAdapter(ComPtr<IDXGIFactory1> pFactory);
			void CreateSwapChainBufferRTV();
			void InitD3D();

		public:
			// <CreateAndGet>
			void CreateSamplers();

			ID3D11Device* GetD3DDevice() const { return _pDevice.Get(); }
			ID3D11DeviceContext* GetD3DDeviceContext() const { return _pDeviceContext.Get(); }
			ID3D11SamplerState* GetD3DSamplerState(Sampler s) const;
			// </CreateAndGet>

			virtual void ImGuiInit(RPHandle renderPassHandle) override;
			virtual void ImGuiRenderDrawData() override;

			virtual void ResizeSwapChain() override;

			// Which graphics API?
			virtual Gapi GetGapi() override { return Gapi::direct3D11; }

			// <FrameCycle>
			virtual void BeginFrame() override;
			virtual void AcquireSwapChainImage() override;
			virtual void EndFrame() override;
			virtual void WaitIdle() override;
			virtual void OnMinimized() override;
			// </FrameCycle>

			// <Resources>
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

			virtual RPHandle CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD) override;
			virtual FBHandle CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h,
				int swapChainBufferIndex = -1, CubeMapFace cubeMapFace = CubeMapFace::none) override;
			virtual void DeleteRenderPass(RPHandle handle) override;
			virtual void DeleteFramebuffer(FBHandle handle) override;
			int GetNextRenderPassIndex() const;
			int GetNextFramebufferIndex() const;
			RP::RcD3DRenderPass GetRenderPass(RPHandle handle) const;
			RP::RcD3DFramebuffer GetFramebuffer(FBHandle handle) const;
			// </Resources>
		};
		VERUS_TYPEDEFS(RendererD3D11);
	}
}

#define VERUS_QREF_RENDERER_D3D11 CGI::PRendererD3D11 pRendererD3D11 = CGI::RendererD3D11::P()
