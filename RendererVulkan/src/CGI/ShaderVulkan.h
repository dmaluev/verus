#pragma once

namespace verus
{
	namespace CGI
	{
		class ShaderVulkan : public BaseShader
		{
			struct Compiled
			{
				VkShaderModule _shaderModules[+Stage::count] = { VK_NULL_HANDLE };
			};
			VERUS_TYPEDEFS(Compiled);
			typedef Map<String, Compiled> TMapCompiled;

			TMapCompiled _mapCompiled;

		public:
			ShaderVulkan();
			virtual ~ShaderVulkan() override;

			virtual void Init(CSZ source, CSZ* branches) override;
			virtual void Done() override;

			//
			// Vulkan
			//

			VkShaderModule GetVkShaderModule(CSZ branch, Stage stage) const { return _mapCompiled.at(branch)._shaderModules[+stage]; }

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderVulkan);
	}
}
