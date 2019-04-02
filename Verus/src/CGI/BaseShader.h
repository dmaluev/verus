#pragma once

namespace verus
{
	namespace CGI
	{
		struct ShaderDesc
		{
			CSZ _source = nullptr;
		};
		VERUS_TYPEDEFS(ShaderDesc);

		class BaseShader : public Object
		{
		protected:
			BaseShader() = default;
			virtual ~BaseShader() = default;

		public:
			virtual void Init(CSZ source, CSZ* list) = 0;
			virtual void Done() = 0;

			void Load(CSZ url);
		};
		VERUS_TYPEDEFS(BaseShader);
	}
}
