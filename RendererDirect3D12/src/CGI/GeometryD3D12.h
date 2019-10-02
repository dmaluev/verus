#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryD3D12 : public BaseGeometry
		{
			struct BufferEx
			{
				ComPtr<ID3D12Resource>   _pBuffer;
				D3D12MA::Allocation*     _pMaAllocation = nullptr;
				UINT64                   _bufferSize = 0;
				D3D12_VERTEX_BUFFER_VIEW _bufferView[BaseRenderer::s_ringBufferSize] = {};
			};

			Vector<BufferEx>                 _vVertexBuffers;
			BufferEx                         _indexBuffer;
			Vector<BufferEx>                 _vStagingVertexBuffers;
			BufferEx                         _stagingIndexBuffer;
			D3D12_INDEX_BUFFER_VIEW          _indexBufferView;
			Vector<D3D12_INPUT_ELEMENT_DESC> _vInputElementDesc;
			Vector<int>                      _vStrides;
			DestroyStaging                   _destroyStagingBuffers;

		public:
			GeometryD3D12();
			virtual ~GeometryD3D12() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;

			virtual void CreateVertexBuffer(int num, int binding) override;
			virtual void UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB) override;

			virtual void CreateIndexBuffer(int num) override;
			virtual void UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB) override;

			//
			// D3D12
			//

			void DestroyStagingBuffers();

			D3D12_INPUT_LAYOUT_DESC GetD3DInputLayoutDesc(UINT32 bindingsFilter, Vector<D3D12_INPUT_ELEMENT_DESC>& vInputElementDesc) const;

			int GetNumVertexBuffers() const { return Utils::Cast32(_vVertexBuffers.size()); }
			const D3D12_VERTEX_BUFFER_VIEW* GetD3DVertexBufferView(int binding) const;
			const D3D12_INDEX_BUFFER_VIEW* GetD3DIndexBufferView() const;
		};
		VERUS_TYPEDEFS(GeometryD3D12);
	}
}
