// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class RendererD3D11;

		class ExtRealityD3D11 : public BaseExtReality
		{
			struct SwapChainEx
			{
				XrSwapchain                            _handle = XR_NULL_HANDLE;
				int32_t                                _width = 0;
				int32_t                                _height = 0;
				Vector<XrSwapchainImageD3D11KHR>       _vImages;
				Vector<ComPtr<ID3D11RenderTargetView>> _vImageViews;
			};

			Vector<SwapChainEx> _vSwapChains;
			LUID                _adapterLuid;
			D3D_FEATURE_LEVEL   _minFeatureLevel = D3D_FEATURE_LEVEL_11_0;

		public:
			ExtRealityD3D11();
			virtual ~ExtRealityD3D11() override;

			virtual void Init() override;
			virtual void Done() override;

			void InitByRenderer(RendererD3D11* pRenderer);

		private:
			void GetSystem();
			void CreateSwapChains(ID3D11Device* pDevice, int64_t format);
			ComPtr<ID3D11RenderTargetView> CreateImageView(ID3D11Device* pDevice, int64_t format, XrSwapchainImageD3D11KHR& image);
			virtual XrSwapchain GetSwapChain(int viewIndex) override;
			virtual void GetSwapChainSize(int viewIndex, int32_t& w, int32_t& h) override;

		public:
			LUID GetAdapterLuid() const { return _adapterLuid; }
			D3D_FEATURE_LEVEL GetMinFeatureLevel() const { return _minFeatureLevel; }
			ID3D11RenderTargetView* GetRTV(int viewIndex, int imageIndex) const;

			virtual void CreateActions() override;
			virtual void PollEvents() override;
			virtual void SyncActions(UINT32 activeActionSetsMask) override;

			virtual bool GetActionStateBoolean(int actionIndex, bool& currentState, bool* pChangedState, int subaction) override;
			virtual bool GetActionStateFloat(int actionIndex, float& currentState, bool* pChangedState, int subaction) override;
			virtual bool GetActionStatePose(int actionIndex, bool& currentState, Math::RPose pose, int subaction) override;

			virtual void BeginFrame() override;
			virtual int LocateViews() override;
			virtual void BeginView(int viewIndex, RViewDesc viewDesc) override;
			virtual void AcquireSwapChainImage() override;
			virtual void EndView(int viewIndex) override;
			virtual void EndFrame() override;

			virtual void BeginAreaUpdate() override;
			virtual void EndAreaUpdate(PcVector4 pUserOffset) override;
		};
		VERUS_TYPEDEFS(ExtRealityD3D11);
	}
}
