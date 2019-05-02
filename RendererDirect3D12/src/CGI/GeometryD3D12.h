#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryD3D12 : public BaseGeometry
		{
			ComPtr<ID3D12Resource> _pVertexBuffer;
			ComPtr<ID3D12Resource> _pIndexBuffer;

		public:
			GeometryD3D12();
			virtual ~GeometryD3D12() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;
		};
		VERUS_TYPEDEFS(GeometryD3D12);
	}
}
