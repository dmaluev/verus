// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class ShaderVulkan : public BaseShader
		{
		public:
			struct Compiled
			{
				VkShaderModule _shaderModules[+Stage::count] = {};
				String         _entry;
				int            _stageCount = 0;
			};
			VERUS_TYPEDEFS(Compiled);

		private:
			typedef Map<String, Compiled> TMapCompiled;

			struct DescriptorSetDesc
			{
				Vector<Sampler>    _vSamplers;
				VkBuffer           _buffer = VK_NULL_HANDLE;
				VmaAllocation      _vmaAllocation = VK_NULL_HANDLE;
				VkDescriptorSet    _descriptorSet = VK_NULL_HANDLE;
				BYTE* _pMappedData = nullptr;
				const void* _pSrc = nullptr;
				int                _size = 0;
				int                _alignedSize = 0;
				int                _capacity = 1;
				int                _capacityInBytes = 0;
				int                _offset = 0;
				int                _peakLoad = 0;
				VkShaderStageFlags _stageFlags = 0;
			};

			TMapCompiled                  _mapCompiled;
			Vector<DescriptorSetDesc>     _vDescriptorSetDesc;
			Vector<VkDescriptorSetLayout> _vDescriptorSetLayouts;
			Vector<VkDescriptorSet>       _vComplexDescriptorSets;
			VkDescriptorPool              _descriptorPool = VK_NULL_HANDLE;
			VkPipelineLayout              _pipelineLayout = VK_NULL_HANDLE;
			String                        _debugInfo;
			UINT64                        _currentFrame = UINT64_MAX;
			uint32_t                      _poolComplexUniformBuffers = 0;
			uint32_t                      _poolComplexImageSamplers = 0;
			uint32_t                      _poolComplexStorageImages = 0;
			uint32_t                      _poolComplexInputAttachments = 0;
			bool                          _compute = false;

		public:
			ShaderVulkan();
			virtual ~ShaderVulkan() override;

			virtual void Init(CSZ source, CSZ sourceName, CSZ* branches) override;
			virtual void Done() override;

			virtual void CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags) override;
			virtual void CreatePipelineLayout() override;
			virtual CSHandle BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMips) override;
			virtual void FreeDescriptorSet(CSHandle& complexSetHandle) override;

			virtual void BeginBindDescriptors() override;
			virtual void EndBindDescriptors() override;

			//
			// Vulkan
			//

			void CreateDescriptorPool();
			void CreateDescriptorSets();
			VkDescriptorSet GetVkDescriptorSet(int setNumber);
			VkDescriptorSet GetComplexVkDescriptorSet(CSHandle descSetID);

			RcCompiled GetCompiled(CSZ branch) const { return _mapCompiled.at(branch); }

			VkPipelineLayout GetVkPipelineLayout() const { return _pipelineLayout; }

			bool TryPushConstants(int setNumber, RBaseCommandBuffer cb);
			int UpdateUniformBuffer(int setNumber);

			bool IsCompute() const { return _compute; }

			void OnError(CSZ s);

			void UpdateUtilization();
		};
		VERUS_TYPEDEFS(ShaderVulkan);
	}
}
