#pragma once

namespace verus
{
	namespace CGI
	{
		class ShaderVulkan : public BaseShader
		{
		public:
			ShaderVulkan();
			virtual ~ShaderVulkan() override;

			virtual void Init(CSZ source, CSZ* list) override;
			virtual void Done() override;
		};
	}
}
