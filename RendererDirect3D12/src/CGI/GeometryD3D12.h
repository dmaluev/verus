#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryD3D12 : public BaseGeometry
		{
			ComPtr<ID3D12Resource>           _pVertexBuffer;
			ComPtr<ID3D12Resource>           _pIndexBuffer;
			ComPtr<ID3D12Resource>           _pStagingVertexBuffer;
			ComPtr<ID3D12Resource>           _pStagingIndexBuffer;
			D3D12_VERTEX_BUFFER_VIEW         _vertexBufferView;
			D3D12_INDEX_BUFFER_VIEW          _indexBufferView;
			Vector<D3D12_INPUT_ELEMENT_DESC> _vInputElementDesc;
			Vector<int>                      _vStrides;

		public:
			GeometryD3D12();
			virtual ~GeometryD3D12() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;

			virtual void BufferDataVB(const void* p, int num, int binding) override;
			virtual void BufferDataIB(const void* p, int num) override;

			//
			// D3D12
			//

			void BufferData(
				ID3D12Resource** ppDestRes,
				ID3D12Resource** ppTempRes,
				size_t numElements,
				size_t elementSize,
				const void* p,
				D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

			D3D12_INPUT_LAYOUT_DESC GetD3DInputLayoutDesc() const;

			const D3D12_VERTEX_BUFFER_VIEW* GetD3DVertexBufferView() const;
			const D3D12_INDEX_BUFFER_VIEW* GetD3DIndexBufferView() const;
		};
		VERUS_TYPEDEFS(GeometryD3D12);
	}
}
