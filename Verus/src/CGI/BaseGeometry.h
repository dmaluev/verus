#pragma once

namespace verus
{
	namespace CGI
	{
		struct GeometryDesc
		{
			PcInputElementDesc _pInputElementDesc;
			const int*         _pStrides = nullptr;
			bool               _32bitIndices = false;
		};
		VERUS_TYPEDEFS(GeometryDesc);

		class BaseGeometry : public Object
		{
		protected:
			int  _bindingInstMask = 0;
			bool _32bitIndices = false;

			BaseGeometry() = default;
			virtual ~BaseGeometry() = default;

		public:
			virtual void Init(RcGeometryDesc desc) = 0;
			virtual void Done() = 0;

			virtual void BufferDataVB(const void* p, int num, int binding) = 0;
			virtual void BufferDataIB(const void* p, int num) = 0;

			static int GetNumInputElementDesc(PcInputElementDesc p);
			static int GetNumBindings(PcInputElementDesc p);

			bool Has32bitIndices() const { return _32bitIndices; }
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
