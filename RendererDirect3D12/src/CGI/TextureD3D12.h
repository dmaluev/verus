// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class TextureD3D12 : public BaseTexture
	{
		struct ResourceEx
		{
			ComPtr<ID3D12Resource> _pResource;
			D3D12MA::Allocation* _pMaAllocation = nullptr;
		};

		ResourceEx         _resource;
		ResourceEx         _uaResource;
		Vector<ResourceEx> _vStagingBuffers;
		Vector<ResourceEx> _vReadbackBuffers;
		Vector<CSHandle>   _vCshGenerateMips;
		DescriptorHeap     _dhSRV;
		DescriptorHeap     _dhUAV;
		DescriptorHeap     _dhRTV;
		DescriptorHeap     _dhDSV;
		DescriptorHeap     _dhSampler;

	public:
		TextureD3D12();
		virtual ~TextureD3D12() override;

		virtual void Init(RcTextureDesc desc) override;
		virtual void Done() override;

		virtual void UpdateSubresource(const void* p, int mipLevel, int arrayLayer, PBaseCommandBuffer pCB) override;
		virtual bool ReadbackSubresource(void* p, bool recordCopyCommand, PBaseCommandBuffer pCB) override;

		virtual void GenerateMips(PBaseCommandBuffer pCB) override;
		void GenerateCubeMapMips(PBaseCommandBuffer pCB);

		virtual Continue Scheduled_Update() override;

		//
		// D3D12
		//

		void ClearCshGenerateMips();

		void CreateSampler();

		ID3D12Resource* GetD3DResource() const { return _resource._pResource.Get(); }

		RcDescriptorHeap GetDescriptorHeapSRV() const { return _dhSRV; }
		RcDescriptorHeap GetDescriptorHeapUAV() const { return _dhUAV; }
		RcDescriptorHeap GetDescriptorHeapRTV() const { return _dhRTV; }
		RcDescriptorHeap GetDescriptorHeapDSV() const { return _dhDSV; }
		RcDescriptorHeap GetDescriptorHeapSampler() const { return _dhSampler; };

		static DXGI_FORMAT RemoveSRGB(DXGI_FORMAT format);
	};
	VERUS_TYPEDEFS(TextureD3D12);
}
