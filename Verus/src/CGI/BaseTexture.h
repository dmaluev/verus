#pragma once

namespace verus
{
	namespace CGI
	{
		class BaseTexture : public Object
		{
		protected:
			BaseTexture() = default;
			virtual ~BaseTexture() = default;

		public:
			virtual void Init() {}
			virtual void Done() {}
		};
		VERUS_TYPEDEFS(BaseTexture);
	}
}
