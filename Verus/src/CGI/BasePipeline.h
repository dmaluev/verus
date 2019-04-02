#pragma once

namespace verus
{
	namespace CGI
	{
		class BasePipeline : public Object
		{
		protected:
			BasePipeline() = default;
			virtual ~BasePipeline() = default;

		public:
			virtual void Init() {}
			virtual void Done() {}
		};
		VERUS_TYPEDEFS(BasePipeline);
	}
}
