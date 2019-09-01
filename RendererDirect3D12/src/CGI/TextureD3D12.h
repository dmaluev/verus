#pragma once

namespace verus
{
	namespace CGI
	{
		class TextureD3D12 : public BaseTexture
		{
			ComPtr<ID3D12Resource>         _pResource;
			ComPtr<ID3D12Resource>         _pResourceUAV;
			Vector<ComPtr<ID3D12Resource>> _vStagingBuffers;
			DescriptorHeap                 _dhSRV;
			DescriptorHeap                 _dhUAV;
			DescriptorHeap                 _dhRTV;
			DescriptorHeap                 _dhDSV;
			DescriptorHeap                 _dhSampler;
			DestroyStaging                 _destroyStagingBuffers;

		public:
			TextureD3D12();
			virtual ~TextureD3D12() override;

			virtual void Init(RcTextureDesc desc) override;
			virtual void Done() override;

			virtual void UpdateImage(int mipLevel, const void* p, int arrayLayer, PBaseCommandBuffer pCB) override;

			virtual void GenerateMips(PBaseCommandBuffer pCB) override;

			//
			// D3D12
			//

			void DestroyStagingBuffers();

			ID3D12Resource* GetD3DResource() const { return _pResource.Get(); }

			RcDescriptorHeap GetDescriptorHeapSRV() const { return _dhSRV; }
			RcDescriptorHeap GetDescriptorHeapUAV() const { return _dhUAV; }
			RcDescriptorHeap GetDescriptorHeapRTV() const { return _dhRTV; }
			RcDescriptorHeap GetDescriptorHeapDSV() const { return _dhDSV; }
			RcDescriptorHeap GetDescriptorHeapSampler() const { return _dhSampler; };
		};
		VERUS_TYPEDEFS(TextureD3D12);
	}
}
