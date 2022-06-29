// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

struct ShaderInclude : BaseShaderInclude
{
	virtual void Open(CSZ filename, void** ppData, UINT32* pBytes) override
	{
		const String url = String("[Shaders]:") + filename;
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(_C(url), vData);
		char* p = new char[vData.size()];
		memcpy(p, vData.data(), vData.size());
		*pBytes = Utils::Cast32(vData.size());
		*ppData = p;
	}

	virtual void Close(void* pData) override
	{
		delete[] pData;
	}
};

// ShaderVulkan:

ShaderVulkan::ShaderVulkan()
{
}

ShaderVulkan::~ShaderVulkan()
{
	Done();
}

void ShaderVulkan::Init(CSZ source, CSZ sourceName, CSZ* branches)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;

	_sourceName = sourceName;
	ShaderInclude inc;
#ifdef _DEBUG
	const UINT32 flags = 1;
#else
	const UINT32 flags = 0;
#endif
	UINT32* pCode = nullptr;
	UINT32 size = 0;
	CSZ pErrorMsgs = nullptr;

	auto CheckErrorMsgs = [this](CSZ pErrorMsgs)
	{
		if (pErrorMsgs)
			OnError(pErrorMsgs);
	};

	auto CreateShaderModule = [](const UINT32* pCode, UINT32 size, VkShaderModule& shaderModule)
	{
		if (!pCode || !size)
			return;
		VERUS_QREF_RENDERER_VULKAN;
		VkResult res = VK_SUCCESS;
		VkShaderModuleCreateInfo vksmci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		vksmci.codeSize = size;
		vksmci.pCode = pCode;
		if (VK_SUCCESS != (res = vkCreateShaderModule(pRendererVulkan->GetVkDevice(), &vksmci, pRendererVulkan->GetAllocator(), &shaderModule)))
			throw VERUS_RUNTIME_ERROR << "vkCreateShaderModule(); res=" << res;
	};

	while (*branches)
	{
		String entry, stageEntries[+Stage::count], stages;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String branch = Parse(*branches, entry, stageEntries, stages, vMacroName, vMacroValue, "DEF_");

		if (IsInIgnoreList(_C(branch)))
		{
			branches++;
			continue;
		}

		// <User defines>
		Vector<CSZ> vDefines;
		vDefines.reserve(40);
		const int count = Utils::Cast32(vMacroName.size());
		VERUS_FOR(i, count)
		{
			vDefines.push_back(_C(vMacroName[i]));
			vDefines.push_back(_C(vMacroValue[i]));
		}
		// </User defines>

		// <System defines>
		char defAnisotropyLevel[64] = {};
		{
			sprintf_s(defAnisotropyLevel, "%d", settings._gpuAnisotropyLevel);
			vDefines.push_back("_ANISOTROPY_LEVEL");
			vDefines.push_back(defAnisotropyLevel);
		}
		char defShaderQuality[64] = {};
		{
			sprintf_s(defShaderQuality, "%d", settings._gpuShaderQuality);
			vDefines.push_back("_SHADER_QUALITY");
			vDefines.push_back(defShaderQuality);
		}
		char defShadowQuality[64] = {};
		{
			sprintf_s(defShadowQuality, "%d", settings._sceneShadowQuality);
			vDefines.push_back("_SHADOW_QUALITY");
			vDefines.push_back(defShadowQuality);
		}
		char defWaterQuality[64] = {};
		{
			sprintf_s(defWaterQuality, "%d", settings._sceneWaterQuality);
			vDefines.push_back("_WATER_QUALITY");
			vDefines.push_back(defWaterQuality);
		}
		char defMaxNumBones[64] = {};
		{
			sprintf_s(defMaxNumBones, "%d", VERUS_MAX_BONES);
			vDefines.push_back("VERUS_MAX_BONES");
			vDefines.push_back(defMaxNumBones);
		}
		vDefines.push_back("_VULKAN");
		vDefines.push_back("1");
		const int typeIndex = Utils::Cast32(vDefines.size());
		vDefines.push_back("_XS");
		vDefines.push_back("1");
		vDefines.push_back(nullptr);
		// </System defines>

		Compiled compiled;
		compiled._entry = entry;

		if (strchr(_C(stages), 'V'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_VS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::vs]), "vs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::vs]);
		}

		if (strchr(_C(stages), 'H'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_HS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::hs]), "hs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::hs]);
		}

		if (strchr(_C(stages), 'D'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_DS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::ds]), "ds", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::ds]);
		}

		if (strchr(_C(stages), 'G'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_GS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::gs]), "gs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::gs]);
		}

		if (strchr(_C(stages), 'F'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_FS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::fs]), "fs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::fs]);
		}

		if (strchr(_C(stages), 'C'))
		{
			compiled._stageCount++;
			vDefines[typeIndex] = "_CS";
			if (!RendererVulkan::VulkanCompile(source, sourceName, vDefines.data(), &inc, _C(stageEntries[+Stage::cs]), "cs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::cs]);
			_compute = true;
		}

		_mapCompiled[branch] = compiled;

		branches++;
	}

	_vDescriptorSetDesc.reserve(4);
}

void ShaderVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_pipelineLayout, vkDestroyPipelineLayout(pRendererVulkan->GetVkDevice(), _pipelineLayout, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_descriptorPool, vkDestroyDescriptorPool(pRendererVulkan->GetVkDevice(), _descriptorPool, pRendererVulkan->GetAllocator()));
	for (auto& x : _vDescriptorSetLayouts)
		VERUS_VULKAN_DESTROY(x, vkDestroyDescriptorSetLayout(pRendererVulkan->GetVkDevice(), x, pRendererVulkan->GetAllocator()));
	_vDescriptorSetLayouts.clear();
	for (auto& x : _vDescriptorSetDesc)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));
	_vDescriptorSetDesc.clear();
	for (auto& kv : _mapCompiled)
	{
		VERUS_FOR(i, +Stage::count)
		{
			VERUS_VULKAN_DESTROY(kv.second._shaderModules[i],
				vkDestroyShaderModule(pRendererVulkan->GetVkDevice(), kv.second._shaderModules[i], pRendererVulkan->GetAllocator()));
		}
	}
	_mapCompiled.clear();

	VERUS_DONE(ShaderVulkan);
}

void ShaderVulkan::CreateDescriptorSet(int setNumber, const void* pSrc, int size, int capacity, std::initializer_list<Sampler> il, ShaderStageFlags stageFlags)
{
	VERUS_QREF_RENDERER_VULKAN;
	VERUS_RT_ASSERT(_vDescriptorSetDesc.size() == setNumber);
	VERUS_RT_ASSERT(!(reinterpret_cast<intptr_t>(pSrc) & 0xF));

	const VkPhysicalDeviceProperties* pPhysicalDeviceProperties = nullptr;
	vmaGetPhysicalDeviceProperties(pRendererVulkan->GetVmaAllocator(), &pPhysicalDeviceProperties);

	DescriptorSetDesc dsd;
	dsd._vSamplers.assign(il);
	dsd._pSrc = pSrc;
	dsd._size = size;
	dsd._alignedSize = Math::AlignUp(dsd._size, static_cast<int>(pPhysicalDeviceProperties->limits.minUniformBufferOffsetAlignment));
	dsd._capacity = capacity;
	dsd._capacityInBytes = dsd._alignedSize * dsd._capacity;
	dsd._stageFlags = ToNativeStageFlags(stageFlags);

	if (capacity > 0)
	{
		const VkDeviceSize bufferSize = dsd._capacityInBytes * BaseRenderer::s_ringBufferSize;
		pRendererVulkan->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, HostAccess::sequentialWrite,
			dsd._buffer, dsd._vmaAllocation);
	}

	_vDescriptorSetDesc.push_back(dsd);
}

void ShaderVulkan::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	StringStream ss;

	auto PrintStageFlags = [&ss](VkShaderStageFlags stageFlags)
	{
		if (stageFlags & VK_SHADER_STAGE_VERTEX_BIT)
			ss << "[V]";
		if (stageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
			ss << "[TC]";
		if (stageFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
			ss << "[TE]";
		if (stageFlags & VK_SHADER_STAGE_GEOMETRY_BIT)
			ss << "[G]";
		if (stageFlags & VK_SHADER_STAGE_FRAGMENT_BIT)
			ss << "[F]";
		if (stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
			ss << "[C]";
	};

	VkPushConstantRange vkpcr = {};
	VkPipelineLayoutCreateInfo vkplci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	if (_vDescriptorSetDesc.empty())
	{
		vkplci.setLayoutCount = 0;
		vkplci.pushConstantRangeCount = 0;
	}
	else
	{
		_vDescriptorSetLayouts.reserve(_vDescriptorSetDesc.size());
		for (const auto& dsd : _vDescriptorSetDesc)
		{
			const int textureCount = Utils::Cast32(dsd._vSamplers.size());
			if (textureCount > 0)
			{
				_poolComplexUniformBuffers += dsd._capacity;
				for (auto s : dsd._vSamplers)
				{
					switch (s)
					{
					case Sampler::storage: _poolComplexStorageImages += dsd._capacity; break;
					case Sampler::input: _poolComplexInputAttachments += dsd._capacity; break;
					default: _poolComplexImageSamplers += dsd._capacity;
					}
				}
			}

			if (dsd._capacity > 0)
			{
				Vector<VkDescriptorSetLayoutBinding> vBindings;
				vBindings.reserve(1 + textureCount);
				VkDescriptorSetLayoutBinding vkdslb = {};
				vkdslb.binding = 0;
				vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				vkdslb.descriptorCount = 1;
				vkdslb.stageFlags = dsd._stageFlags;
				vkdslb.pImmutableSamplers = nullptr;
				vBindings.push_back(vkdslb);
				VERUS_FOR(i, textureCount)
				{
					VkDescriptorSetLayoutBinding vkdslb = {};
					vkdslb.binding = i + 1;
					switch (dsd._vSamplers[i])
					{
					case Sampler::storage: vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
					case Sampler::input: vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; break;
					default: vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					}
					vkdslb.descriptorCount = 1;
					vkdslb.stageFlags = dsd._stageFlags;
					if (Sampler::custom != dsd._vSamplers[i])
						vkdslb.pImmutableSamplers = pRendererVulkan->GetImmutableSampler(dsd._vSamplers[i]);
					vBindings.push_back(vkdslb);
				}
				VkDescriptorSetLayoutCreateInfo vkdslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
				vkdslci.bindingCount = Utils::Cast32(vBindings.size());
				vkdslci.pBindings = vBindings.data();

				ss << "Set=" << _vDescriptorSetLayouts.size();
				ss << std::endl;
				for (const auto& binding : vBindings)
				{
					ss << "    binding=" << binding.binding;
					ss << ", type=";
					switch (binding.descriptorType)
					{
					case VK_DESCRIPTOR_TYPE_SAMPLER: ss << "SAMPLER"; break;
					case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: ss << "COMBINED_IMAGE_SAMPLER"; break;
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: ss << "SAMPLED_IMAGE"; break;
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: ss << "STORAGE_IMAGE"; break;
					case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: ss << "UNIFORM_TEXEL_BUFFER"; break;
					case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: ss << "STORAGE_TEXEL_BUFFER"; break;
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: ss << "UNIFORM_BUFFER"; break;
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: ss << "STORAGE_BUFFER"; break;
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: ss << "UNIFORM_BUFFER_DYNAMIC"; break;
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: ss << "STORAGE_BUFFER_DYNAMIC"; break;
					case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: ss << "INPUT_ATTACHMENT"; break;
					}
					ss << ", count=" << binding.descriptorCount;
					ss << ", flags=";
					PrintStageFlags(binding.stageFlags);
					ss << ", immutableSampler=" << binding.pImmutableSamplers;
					ss << std::endl;
				}

				VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
				if (VK_SUCCESS != (res = vkCreateDescriptorSetLayout(pRendererVulkan->GetVkDevice(), &vkdslci, pRendererVulkan->GetAllocator(), &descriptorSetLayout)))
					throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorSetLayout(); res=" << res;

				_vDescriptorSetLayouts.push_back(descriptorSetLayout);
			}
			else
			{
				VERUS_RT_ASSERT(&dsd == &_vDescriptorSetDesc.back()); // Only the last set is allowed to use push constants.
				vkpcr.stageFlags = dsd._stageFlags;
				vkpcr.offset = 0;
				vkpcr.size = dsd._size;
				vkplci.pushConstantRangeCount = 1;
				vkplci.pPushConstantRanges = &vkpcr;

				ss << "Set=PushConst";
				ss << std::endl;
				ss << "    flags=";
				PrintStageFlags(vkpcr.stageFlags);
				ss << ", offset=" << vkpcr.offset;
				ss << ", size=" << vkpcr.size;
				ss << std::endl;
			}
		}

		vkplci.setLayoutCount = Utils::Cast32(_vDescriptorSetLayouts.size());
		vkplci.pSetLayouts = _vDescriptorSetLayouts.data();
	}

	_debugInfo = ss.str();

	if (VK_SUCCESS != (res = vkCreatePipelineLayout(pRendererVulkan->GetVkDevice(), &vkplci, pRendererVulkan->GetAllocator(), &_pipelineLayout)))
		throw VERUS_RUNTIME_ERROR << "vkCreatePipelineLayout(); res=" << res;

	_vComplexDescriptorSets.reserve(_poolComplexUniformBuffers);

	if (vkplci.setLayoutCount)
	{
		CreateDescriptorPool();
		CreateDescriptorSets();
	}
}

CSHandle ShaderVulkan::BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMipLevels, const int* pArrayLayers)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	// <NewComplexSetHandle>
	int complexSetHandle = -1;
	VERUS_FOR(i, _vComplexDescriptorSets.size())
	{
		if (VK_NULL_HANDLE == _vComplexDescriptorSets[i])
		{
			complexSetHandle = i;
			break;
		}
	}
	if (-1 == complexSetHandle)
	{
		complexSetHandle = Utils::Cast32(_vComplexDescriptorSets.size());
		_vComplexDescriptorSets.resize(complexSetHandle + 1);
	}
	// </NewComplexSetHandle>

	const auto& dsd = _vDescriptorSetDesc[setNumber];
	VERUS_RT_ASSERT(dsd._vSamplers.size() == il.size());

	// Allocate from pool:
	VkDescriptorSetAllocateInfo vkdsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	vkdsai.descriptorPool = _descriptorPool;
	vkdsai.descriptorSetCount = 1;
	vkdsai.pSetLayouts = &_vDescriptorSetLayouts[setNumber];
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, &descriptorSet)))
		throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(); res=" << res;
	_vComplexDescriptorSets[complexSetHandle] = descriptorSet;

	// Info structures:
	VkDescriptorBufferInfo vkdbi = {};
	vkdbi.buffer = dsd._buffer;
	vkdbi.offset = 0;
	vkdbi.range = dsd._size;
	Vector<VkDescriptorImageInfo> vDescriptorImageInfo;
	vDescriptorImageInfo.reserve(il.size());
	int index = 0;
	for (const auto& x : il)
	{
		auto& texVulkan = static_cast<RTextureVulkan>(*x);
		const int mipLevel = pMipLevels ? pMipLevels[index] : 0;
		const int arrayLayer = pArrayLayers ? pArrayLayers[index] : 0;
		VkDescriptorImageInfo vkdii = {};
		if (Sampler::storage == dsd._vSamplers[index])
		{
			vkdii.imageView = texVulkan.GetStorageVkImageView(mipLevel, arrayLayer);
			vkdii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else if (Sampler::shadow == dsd._vSamplers[index])
		{
			vkdii.sampler = texVulkan.GetVkSampler();
			vkdii.imageView = texVulkan.GetVkImageView();
			vkdii.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else
		{
			vkdii.sampler = texVulkan.GetVkSampler();
			vkdii.imageView = texVulkan.GetVkImageView();
			vkdii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			if (BaseTexture::IsDepthFormat(texVulkan.GetFormat()))
				vkdii.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		vDescriptorImageInfo.push_back(vkdii);
		index++;
	}

	Vector<VkWriteDescriptorSet> vWriteDescriptorSet;
	vWriteDescriptorSet.reserve(1 + il.size());

	// First comes uniform buffer:
	VkWriteDescriptorSet vkwds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	vkwds.dstSet = descriptorSet;
	vkwds.dstBinding = 0;
	vkwds.dstArrayElement = 0;
	vkwds.descriptorCount = 1;
	vkwds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	vkwds.pBufferInfo = &vkdbi;
	vWriteDescriptorSet.push_back(vkwds);

	// Images & samplers:
	index = 0;
	for (const auto& x : il)
	{
		VkWriteDescriptorSet vkwds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		vkwds.dstSet = descriptorSet;
		vkwds.dstBinding = index + 1;
		vkwds.dstArrayElement = 0;
		vkwds.descriptorCount = 1;
		switch (dsd._vSamplers[index])
		{
		case Sampler::storage: vkwds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
		case Sampler::input: vkwds.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; break;
		default: vkwds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		}
		vkwds.pImageInfo = &vDescriptorImageInfo[index];
		vWriteDescriptorSet.push_back(vkwds);
		index++;
	}

	vkUpdateDescriptorSets(pRendererVulkan->GetVkDevice(), Utils::Cast32(vWriteDescriptorSet.size()), vWriteDescriptorSet.data(), 0, nullptr);

	return CSHandle::Make(complexSetHandle);
}

void ShaderVulkan::FreeDescriptorSet(CSHandle& complexSetHandle)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	if (complexSetHandle.IsSet() && complexSetHandle.Get() < _vComplexDescriptorSets.size() && _vComplexDescriptorSets[complexSetHandle.Get()] != VK_NULL_HANDLE)
	{
		if (VK_SUCCESS != (res = vkFreeDescriptorSets(pRendererVulkan->GetVkDevice(), _descriptorPool, 1, &_vComplexDescriptorSets[complexSetHandle.Get()])))
			throw VERUS_RUNTIME_ERROR << "vkFreeDescriptorSets(); res=" << res;
		_vComplexDescriptorSets[complexSetHandle.Get()] = VK_NULL_HANDLE;
	}
	complexSetHandle = CSHandle();
}

void ShaderVulkan::BeginBindDescriptors()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const bool resetOffset = _currentFrame != renderer.GetFrameCount();
	_currentFrame = renderer.GetFrameCount();

	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (!dsd._capacity)
			continue;
		VERUS_RT_ASSERT(!dsd._pMappedData);
		void* pData = nullptr;
		if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), dsd._vmaAllocation, &pData)))
			throw VERUS_RECOVERABLE << "vmaMapMemory(); res=" << res;
		dsd._pMappedData = static_cast<BYTE*>(pData);
		dsd._pMappedData += dsd._capacityInBytes * pRendererVulkan->GetRingBufferIndex(); // Adjust address for this frame.
		if (resetOffset)
			dsd._offset = 0;
	}
}

void ShaderVulkan::EndBindDescriptors()
{
	VERUS_QREF_RENDERER_VULKAN;

	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (!dsd._capacity)
			continue;
		VERUS_RT_ASSERT(dsd._pMappedData);
		vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), dsd._vmaAllocation);
		dsd._pMappedData = nullptr;
	}
}

void ShaderVulkan::CreateDescriptorPool()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkDescriptorPoolCreateInfo vkdpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	vkdpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	vkdpci.poolSizeCount = 1;
	VkDescriptorPoolSize vkdps[4] = {};
	vkdps[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _poolComplexUniformBuffers + Utils::Cast32(_vDescriptorSetLayouts.size()) };
	vkdpci.maxSets = vkdps[0].descriptorCount;
	int index = 1;
	if (_poolComplexImageSamplers > 0)
	{
		vkdps[index++] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _poolComplexImageSamplers };
		vkdpci.poolSizeCount++;
	}
	if (_poolComplexStorageImages > 0)
	{
		vkdps[index++] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, _poolComplexStorageImages };
		vkdpci.poolSizeCount++;
	}
	if (_poolComplexInputAttachments > 0)
	{
		vkdps[index++] = { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, _poolComplexInputAttachments };
		vkdpci.poolSizeCount++;
	}
	vkdpci.pPoolSizes = vkdps;
	if (VK_SUCCESS != (res = vkCreateDescriptorPool(pRendererVulkan->GetVkDevice(), &vkdpci, pRendererVulkan->GetAllocator(), &_descriptorPool)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorPool(); res=" << res;
}

void ShaderVulkan::CreateDescriptorSets()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	int index = 0;
	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (index < _vDescriptorSetLayouts.size())
		{
			VkDescriptorSetAllocateInfo vkdsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			vkdsai.descriptorPool = _descriptorPool;
			vkdsai.descriptorSetCount = 1;
			vkdsai.pSetLayouts = &_vDescriptorSetLayouts[index];
			if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, &dsd._descriptorSet)))
				throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(); res=" << res;
		}

		if (dsd._capacity > 0)
		{
			VkDescriptorBufferInfo vkdbi = {};
			vkdbi.buffer = dsd._buffer;
			vkdbi.offset = 0;
			vkdbi.range = dsd._size;
			VkWriteDescriptorSet vkwds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			vkwds.dstSet = dsd._descriptorSet;
			vkwds.dstBinding = 0;
			vkwds.dstArrayElement = 0;
			vkwds.descriptorCount = 1;
			vkwds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			vkwds.pBufferInfo = &vkdbi;

			vkUpdateDescriptorSets(pRendererVulkan->GetVkDevice(), 1, &vkwds, 0, nullptr);

			index++;
		}
	}
}

VkDescriptorSet ShaderVulkan::GetVkDescriptorSet(int setNumber)
{
	return _vDescriptorSetDesc[setNumber]._descriptorSet;
}

VkDescriptorSet ShaderVulkan::GetComplexVkDescriptorSet(CSHandle descSetID)
{
	return _vComplexDescriptorSets[descSetID.Get()];
}

bool ShaderVulkan::TryPushConstants(int setNumber, RBaseCommandBuffer cb)
{
	auto& dsd = _vDescriptorSetDesc[setNumber];
	if (!dsd._capacity)
	{
		auto& cbVulkan = static_cast<RCommandBufferVulkan>(cb);
		vkCmdPushConstants(cbVulkan.GetVkCommandBuffer(), _pipelineLayout, dsd._stageFlags, 0, dsd._size, dsd._pSrc);
		return true;
	}
	return false;
}

int ShaderVulkan::UpdateUniformBuffer(int setNumber)
{
	VERUS_QREF_RENDERER_VULKAN;

	auto& dsd = _vDescriptorSetDesc[setNumber];
	if (dsd._offset + dsd._alignedSize > dsd._capacityInBytes)
	{
		VERUS_RT_FAIL("UniformBuffer is full.");
		return -1;
	}

	VERUS_RT_ASSERT(dsd._pMappedData);
	const int ret = dsd._capacityInBytes * pRendererVulkan->GetRingBufferIndex() + dsd._offset;
	memcpy(dsd._pMappedData + dsd._offset, dsd._pSrc, dsd._size);
	dsd._offset += dsd._alignedSize;
	dsd._peakLoad = Math::Max(dsd._peakLoad, dsd._offset);
	return ret;
}

void ShaderVulkan::OnError(CSZ s)
{
	VERUS_QREF_RENDERER;
	if (strstr(s, " ERROR: "))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}

void ShaderVulkan::UpdateUtilization()
{
	VERUS_QREF_RENDERER;
	int setNumber = 0;
	for (const auto& dsd : _vDescriptorSetDesc)
	{
		StringStream ss;
		ss << "set=" << setNumber << ", " << _sourceName;
		renderer.AddUtilization(_C(ss.str()), dsd._offset / dsd._alignedSize, dsd._capacity);
		setNumber++;
	}
}
