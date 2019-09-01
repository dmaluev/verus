#include "stdafx.h"

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
		VkShaderModuleCreateInfo vksmci = {};
		vksmci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vksmci.codeSize = size;
		vksmci.pCode = pCode;
		if (VK_SUCCESS != (res = vkCreateShaderModule(pRendererVulkan->GetVkDevice(), &vksmci, pRendererVulkan->GetAllocator(), &shaderModule)))
			throw VERUS_RUNTIME_ERROR << "vkCreateShaderModule(), res=" << res;
	};

	while (*branches)
	{
		String entryVS, entryHS, entryDS, entryGS, entryFS, entryCS;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String entry = Parse(*branches, entryVS, entryHS, entryDS, entryGS, entryFS, entryCS, vMacroName, vMacroValue, "DEF_");

		if (IsInIgnoreList(_C(entry)))
		{
			branches++;
			continue;
		}

		// <User defines>
		Vector<CSZ> vDefines;
		vDefines.reserve(40);
		const int num = Utils::Cast32(vMacroName.size());
		VERUS_FOR(i, num)
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
			sprintf_s(defMaxNumBones, "%d", VERUS_MAX_NUM_BONES);
			vDefines.push_back("VERUS_MAX_NUM_BONES");
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

		if (strstr(source, " mainVS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_VS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryVS), "vs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::vs]);
		}

		if (strstr(source, " mainHS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_HS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryHS), "hs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::hs]);
		}

		if (strstr(source, " mainDS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_DS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryDS), "ds", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::ds]);
		}

		if (strstr(source, " mainGS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_GS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryGS), "gs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::gs]);
		}

		if (strstr(source, " mainFS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_FS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryFS), "fs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::fs]);
		}

		if (strstr(source, " mainCS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_CS";
			if (!RendererVulkan::VerusCompile(source, sourceName, vDefines.data(), &inc, _C(entryCS), "cs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::cs]);
			_compute = true;
		}

		_mapCompiled[entry] = compiled;

		branches++;
	}

	_vDescriptorSetDesc.reserve(4);
}

void ShaderVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_descriptorPool, vkDestroyDescriptorPool(pRendererVulkan->GetVkDevice(), _descriptorPool, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_pipelineLayout, vkDestroyPipelineLayout(pRendererVulkan->GetVkDevice(), _pipelineLayout, pRendererVulkan->GetAllocator()));
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
	dsd._sizeAligned = Math::AlignUp(dsd._size, static_cast<int>(pPhysicalDeviceProperties->limits.minUniformBufferOffsetAlignment));
	dsd._capacity = capacity;
	dsd._capacityInBytes = dsd._sizeAligned*dsd._capacity;
	dsd._stageFlags = ToNativeStageFlags(stageFlags);

	if (capacity > 0)
	{
		const VkDeviceSize bufferSize = dsd._capacityInBytes * BaseRenderer::s_ringBufferSize;
		pRendererVulkan->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
			dsd._buffer, dsd._vmaAllocation);
	}

	_vDescriptorSetDesc.push_back(dsd);
}

void ShaderVulkan::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkPushConstantRange vkpcr = {};
	VkPipelineLayoutCreateInfo vkplci = {};
	vkplci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
			const int numTextures = Utils::Cast32(dsd._vSamplers.size());
			if (numTextures > 0)
			{
				_poolComplexUniformBuffers += dsd._capacity;
				for (auto s : dsd._vSamplers)
				{
					if (Sampler::storage == s)
						_poolComplexStorageImages += dsd._capacity;
					else
						_poolComplexImageSamplers += dsd._capacity;
				}
			}

			if (dsd._capacity > 0)
			{
				Vector<VkDescriptorSetLayoutBinding> vBindings;
				vBindings.reserve(1 + numTextures);
				VkDescriptorSetLayoutBinding vkdslb = {};
				vkdslb.binding = 0;
				vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				vkdslb.descriptorCount = 1;
				vkdslb.stageFlags = dsd._stageFlags;
				vkdslb.pImmutableSamplers = nullptr;
				vBindings.push_back(vkdslb);
				VERUS_FOR(i, numTextures)
				{
					VkDescriptorSetLayoutBinding vkdslb = {};
					vkdslb.binding = i + 1;
					vkdslb.descriptorType = (Sampler::storage == dsd._vSamplers[i]) ?
						VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					vkdslb.descriptorCount = 1;
					vkdslb.stageFlags = dsd._stageFlags;
					if (Sampler::custom != dsd._vSamplers[i])
						vkdslb.pImmutableSamplers = pRendererVulkan->GetImmutableSampler(dsd._vSamplers[i]);
					vBindings.push_back(vkdslb);
				}
				VkDescriptorSetLayoutCreateInfo vkdslci = {};
				vkdslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				vkdslci.bindingCount = Utils::Cast32(vBindings.size());
				vkdslci.pBindings = vBindings.data();
				VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
				if (VK_SUCCESS != (res = vkCreateDescriptorSetLayout(pRendererVulkan->GetVkDevice(), &vkdslci, pRendererVulkan->GetAllocator(), &descriptorSetLayout)))
					throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorSetLayout(), res=" << res;

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
			}
		}

		vkplci.setLayoutCount = Utils::Cast32(_vDescriptorSetLayouts.size());
		vkplci.pSetLayouts = _vDescriptorSetLayouts.data();
	}

	if (VK_SUCCESS != (res = vkCreatePipelineLayout(pRendererVulkan->GetVkDevice(), &vkplci, pRendererVulkan->GetAllocator(), &_pipelineLayout)))
		throw VERUS_RUNTIME_ERROR << "vkCreatePipelineLayout(), res=" << res;

	_vComplexDescriptorSets.reserve(_poolComplexUniformBuffers);

	if (vkplci.setLayoutCount)
	{
		CreateDescriptorPool();
		CreateDescriptorSets();
	}
}

int ShaderVulkan::BindDescriptorSetTextures(int setNumber, std::initializer_list<TexturePtr> il, const int* pMips)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	// New complex descriptor set's ID:
	int complexSetID = -1;
	VERUS_FOR(i, _vComplexDescriptorSets.size())
	{
		if (VK_NULL_HANDLE == _vComplexDescriptorSets[i])
		{
			complexSetID = i;
			break;
		}
	}
	if (-1 == complexSetID)
	{
		complexSetID = Utils::Cast32(_vComplexDescriptorSets.size());
		_vComplexDescriptorSets.resize(complexSetID + 1);
	}

	auto& dsd = _vDescriptorSetDesc[setNumber];
	VERUS_RT_ASSERT(dsd._vSamplers.size() == il.size());

	// Allocate from pool:
	VkDescriptorSetAllocateInfo vkdsai = {};
	vkdsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	vkdsai.descriptorPool = _descriptorPool;
	vkdsai.descriptorSetCount = 1;
	vkdsai.pSetLayouts = &_vDescriptorSetLayouts[setNumber];
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, &descriptorSet)))
		throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(), res=" << res;
	_vComplexDescriptorSets[complexSetID] = descriptorSet;

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
		const int mip = pMips ? pMips[index] : 0;
		VkDescriptorImageInfo vkdii = {};
		if (Sampler::storage == dsd._vSamplers[index])
		{
			vkdii.imageView = texVulkan.GetVkImageViewStorage(mip);
			vkdii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			vkdii.sampler = texVulkan.GetVkSampler();
			vkdii.imageView = texVulkan.GetVkImageView();
			vkdii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		vDescriptorImageInfo.push_back(vkdii);
		index++;
	}

	Vector<VkWriteDescriptorSet> vWriteDescriptorSet;
	vWriteDescriptorSet.reserve(1 + il.size());

	// First comes uniform buffer:
	VkWriteDescriptorSet vkwds = {};
	vkwds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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
		VkWriteDescriptorSet vkwds = {};
		vkwds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vkwds.dstSet = descriptorSet;
		vkwds.dstBinding = index + 1;
		vkwds.dstArrayElement = 0;
		vkwds.descriptorCount = 1;
		vkwds.descriptorType = (Sampler::storage == dsd._vSamplers[index]) ?
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		vkwds.pImageInfo = &vDescriptorImageInfo[index];
		vWriteDescriptorSet.push_back(vkwds);
		index++;
	}

	vkUpdateDescriptorSets(pRendererVulkan->GetVkDevice(), Utils::Cast32(vWriteDescriptorSet.size()), vWriteDescriptorSet.data(), 0, nullptr);

	return complexSetID;
}

void ShaderVulkan::BeginBindDescriptors()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const bool resetOffset = _currentFrame != renderer.GetNumFrames();
	_currentFrame = renderer.GetNumFrames();

	for (auto& dsd : _vDescriptorSetDesc)
	{
		if (!dsd._capacity)
			continue;
		VERUS_RT_ASSERT(!dsd._pMappedData);
		void* pData = nullptr;
		if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), dsd._vmaAllocation, &pData)))
			throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
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

	VkDescriptorPoolSize vkdps[3] = {};
	vkdps[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	vkdps[0].descriptorCount = _poolComplexUniformBuffers + Utils::Cast32(_vDescriptorSetLayouts.size());
	vkdps[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	vkdps[1].descriptorCount = _poolComplexImageSamplers;
	vkdps[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	vkdps[2].descriptorCount = _poolComplexStorageImages;
	VkDescriptorPoolCreateInfo vkdpci = {};
	vkdpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	vkdpci.maxSets = vkdps[0].descriptorCount;
	vkdpci.poolSizeCount = 1;
	if (_poolComplexImageSamplers > 0)
		vkdpci.poolSizeCount++;
	if (_poolComplexStorageImages > 0)
		vkdpci.poolSizeCount++;
	vkdpci.pPoolSizes = vkdps;
	if (VK_SUCCESS != (res = vkCreateDescriptorPool(pRendererVulkan->GetVkDevice(), &vkdpci, pRendererVulkan->GetAllocator(), &_descriptorPool)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorPool(), res=" << res;
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
			VkDescriptorSetAllocateInfo vkdsai = {};
			vkdsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			vkdsai.descriptorPool = _descriptorPool;
			vkdsai.descriptorSetCount = 1;
			vkdsai.pSetLayouts = &_vDescriptorSetLayouts[index];
			if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, &dsd._descriptorSet)))
				throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(), res=" << res;
		}

		if (dsd._capacity > 0)
		{
			VkDescriptorBufferInfo vkdbi = {};
			vkdbi.buffer = dsd._buffer;
			vkdbi.offset = 0;
			vkdbi.range = dsd._size;
			VkWriteDescriptorSet vkwds = {};
			vkwds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
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

VkDescriptorSet ShaderVulkan::GetComplexVkDescriptorSet(int descSetID)
{
	return _vComplexDescriptorSets[descSetID];
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
	if (dsd._offset + dsd._sizeAligned > dsd._capacityInBytes)
		return -1;

	VERUS_RT_ASSERT(dsd._pMappedData);
	const int ret = dsd._capacityInBytes * pRendererVulkan->GetRingBufferIndex() + dsd._offset;
	memcpy(dsd._pMappedData + dsd._offset, dsd._pSrc, dsd._size);
	dsd._offset += dsd._sizeAligned;
	return ret;
}

void ShaderVulkan::OnError(CSZ s)
{
	VERUS_QREF_RENDERER;
	if (strstr(s, ": error "))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}
