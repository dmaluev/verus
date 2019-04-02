#pragma once

namespace verus
{
	namespace CGI
	{
		struct GeometryDesc
		{
			bool _dwordIndices = false;
		};
		VERUS_TYPEDEFS(GeometryDesc);

		class BaseGeometry : public Object
		{
		protected:
			BaseGeometry() = default;
			virtual ~BaseGeometry() = default;

		public:
			virtual void Init(RcGeometryDesc desc) = 0;
			virtual void Done() = 0;
		};
		VERUS_TYPEDEFS(BaseGeometry);
	}
}
