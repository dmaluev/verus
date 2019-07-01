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

			struct UniformBuffer
			{
				VkBuffer           _buffer = VK_NULL_HANDLE;
				VkBuffer           _bufferTemp = VK_NULL_HANDLE;
				VmaAllocation      _vmaAllocation = VK_NULL_HANDLE;
				VmaAllocation      _vmaAllocationTemp = VK_NULL_HANDLE;
				VkDescriptorSet    _descriptorSet[2] = {};
				BYTE*              _pMappedData = nullptr;
				const void*        _pSrc = nullptr;
				int                _size = 0;
				int                _sizeAligned = 0;
				int                _capacity = 1;
				int                _capacityInBytes = 0;
				int                _offset = 0;
				VkShaderStageFlags _stageFlags = 0;
			};

			TMapCompiled                  _mapCompiled;
			Vector<UniformBuffer>         _vUniformBuffers;
			Vector<VkDescriptorSetLayout> _vDescriptorSetLayouts;
			Vector<VkDescriptorSet>       _vDescriptorSets;
			VkDescriptorPool              _descriptorPool = VK_NULL_HANDLE;
			VkPipelineLayout              _pipelineLayout = VK_NULL_HANDLE;
			UINT64                        _currentFrame = UINT64_MAX;

		public:
			ShaderVulkan();
			virtual ~ShaderVulkan() override;

			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
			virtual void Done() override;

			virtual void BindUniformBufferSource(int descSet, const void* pSrc, int size, int capacity, ShaderStageFlags stageFlags) override;
			virtual void CreatePipelineLayout() override;

			virtual void BeginBindDescriptors() override;
			virtual void EndBindDescriptors() override;

			//
			// Vulkan
			//

			void CreateDescriptorPool();
			void CreateDescriptorSets();
			VkDescriptorSet GetVkDescriptorSet(int descSet);

			VkShaderModule GetVkShaderModule(CSZ branch, Stage stage) const { return _mapCompiled.at(branch)._shaderModules[+stage]; }
			int GetNumStages(CSZ branch) const { return _mapCompiled.at(branch)._numStages; }

			VkPipelineLayout GetVkPipelineLayout() const { return _pipelineLayout; }

			bool TryPushConstants(int descSet, RBaseCommandBuffer cb);
			int UpdateUniformBuffer(int descSet);

			void OnError(CSZ s);
		};
		VERUS_TYPEDEFS(ShaderVulkan);
	}
}
