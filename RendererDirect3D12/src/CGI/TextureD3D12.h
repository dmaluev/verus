#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureD3D12 : public BaseTexture
		{
			ComPtr<ID3D12Resource> _pResource;

		public:
			TextureD3D12();
			virtual ~TextureD3D12() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			//
			// D3D12
			//

			ID3D12Resource* GetID3D12Resource() const { return _pResource.Get(); }
		};
		VERUS_TYPEDEFS(TextureD3D12);
	}
}
