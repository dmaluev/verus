#pragma once

namespace verus
{
	namespace CGI
	{
		class ShaderVulkan : public BaseShader
		{
			struct Compiled
			{
				VkShaderModule _shaderModules[+Stage::count] = {};
				int            _numStages = 0;
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
			int GetNumStages(CSZ branch) const { return _mapCompiled.at(branch)._numStages; }

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderVulkan);
	}
}
