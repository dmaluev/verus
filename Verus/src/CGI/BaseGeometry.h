#pragma once

namespace verus
{
	namespace CGI
	{
		class BaseCommandBuffer;

		struct GeometryDesc
		{
			PcInputElementDesc _pInputElementDesc;
			const int* _pStrides = nullptr;
			bool               _32BitIndices = false;
			bool               _dynamic = false;
		};
		VERUS_TYPEDEFS(GeometryDesc);

		class BaseGeometry : public Object
		{
		protected:
			int  _bindingInstMask = 0;
			bool _32BitIndices = false;
			bool _dynamic = false;

			BaseGeometry() = default;
			virtual ~BaseGeometry() = default;

		public:
			virtual void Init(RcGeometryDesc desc) = 0;
			virtual void Done() = 0;

			virtual void CreateVertexBuffer(int num, int binding) = 0;
			virtual void UpdateVertexBuffer(const void* p, int binding, BaseCommandBuffer* pCB = nullptr) = 0;

			virtual void CreateIndexBuffer(int num) = 0;
			virtual void UpdateIndexBuffer(const void* p, BaseCommandBuffer* pCB = nullptr) = 0;

			static int GetNumInputElementDesc(PcInputElementDesc p);
			static int GetNumBindings(PcInputElementDesc p);

			bool Has32BitIndices() const { return _32BitIndices; }
		};
		VERUS_TYPEDEFS(BaseGeometry);

		class GeometryPtr : public Ptr<BaseGeometry>
		{
		public:
			void Init(RcGeometryDesc desc);
		};
		VERUS_TYPEDEFS(GeometryPtr);

		class GeometryPwn : public GeometryPtr
		{
		public:
			~GeometryPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(GeometryPwn);

		template<int NUM>
		class GeometryPwns : public Pwns<GeometryPwn, NUM>
		{
		};
	}
}
