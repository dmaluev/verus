// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryD3D11 : public BaseGeometry
		{
			struct BufferEx
			{
				ComPtr<ID3D11Buffer> _pBuffer;
				UINT64               _bufferSize = 0;
			};

			struct StructuredBufferEx : BufferEx
			{
				ComPtr<ID3D11ShaderResourceView> _pSRV;
				int                              _structSize = 0;
			};

			Vector<BufferEx>                 _vVertexBuffers;
			BufferEx                         _indexBuffer;
			Vector<StructuredBufferEx>       _vStructuredBuffers;
			Vector<D3D11_INPUT_ELEMENT_DESC> _vInputElementDescs;
			Vector<int>                      _vStrides;

		public:
			GeometryD3D11();
			virtual ~GeometryD3D11() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;

			virtual void CreateVertexBuffer(int count, int binding) override;
			virtual void UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;

			virtual void CreateIndexBuffer(int count) override;
			virtual void UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;

			virtual void CreateStorageBuffer(int count, int structSize, int sbIndex, ShaderStageFlags stageFlags) override;
			virtual void UpdateStorageBuffer(const void* p, int sbIndex, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;
			virtual int GetStorageBufferStructSize(int sbIndex) const override;

			virtual Continue Scheduled_Update() override;

			//
			// D3D11
			//

			void GetD3DInputElementDescs(UINT32 bindingsFilter, Vector<D3D11_INPUT_ELEMENT_DESC>& vInputElementDescs) const;
			int GetStride(int binding) const { return _vStrides[binding]; }

			int GetVertexBufferCount() const { return Utils::Cast32(_vVertexBuffers.size()); }
			ID3D11Buffer* GetD3DVertexBuffer(int binding) const { return _vVertexBuffers[binding]._pBuffer.Get(); }
			ID3D11Buffer* GetD3DIndexBuffer() const { return _indexBuffer._pBuffer.Get(); }
			ID3D11ShaderResourceView* GetD3DStructuredBufferSRV(int sbIndex) const { return _vStructuredBuffers[sbIndex]._pSRV.Get(); }
		};
		VERUS_TYPEDEFS(GeometryD3D11);
	}
}
