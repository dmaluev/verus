// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureD3D11 : public BaseTexture
		{
			ComPtr<ID3D11Texture2D>                   _pTexture2D;
			ComPtr<ID3D11Texture3D>                   _pTexture3D;
			ComPtr<ID3D11Texture2D>                   _pUaTexture;
			Vector<ComPtr<ID3D11Texture2D>>           _vReadbackTextures;
			Vector<CSHandle>                          _vCshGenerateMips;
			ComPtr<ID3D11ShaderResourceView>          _pSRV;
			Vector<ComPtr<ID3D11UnorderedAccessView>> _vUAVs;
			Vector<ComPtr<ID3D11RenderTargetView>>    _vRTVs;
			ComPtr<ID3D11DepthStencilView>            _pDSV[2];
			ComPtr<ID3D11SamplerState>                _pSamplerState;

		public:
			TextureD3D11();
			virtual ~TextureD3D11() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			virtual void UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB) override;
			virtual bool ReadbackSubresource(void* p, bool recordCopyCommand, PBaseCommandBuffer pCB) override;

			virtual void GenerateMips(PBaseCommandBuffer pCB) override;
			void GenerateCubeMapMips(PBaseCommandBuffer pCB);

			virtual Continue Scheduled_Update() override;

			//
			// D3D11
			//

			void ClearCshGenerateMips();

			void CreateSampler();

			ID3D11Resource* GetD3DResource() const;

			ID3D11ShaderResourceView* GetSRV() const { return _pSRV.Get(); }
			ID3D11UnorderedAccessView* GetUAV(int index = 0) const { return _vUAVs[index].Get(); }
			ID3D11RenderTargetView* GetRTV(int index = 0) const { return _vRTVs[index].Get(); }
			ID3D11DepthStencilView* GetDSV(bool readOnly = false) const { return readOnly ? _pDSV[1].Get() : _pDSV[0].Get(); }
			ID3D11SamplerState* GetD3DSamplerState() const { return _pSamplerState.Get(); }

			static DXGI_FORMAT RemoveSRGB(DXGI_FORMAT format);
		};
		VERUS_TYPEDEFS(TextureD3D11);
	}
}
