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
		String entryVS, entryHS, entryDS, entryGS, entryFS;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String entry = Parse(*branches, entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");

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

		_mapCompiled[entry] = compiled;

		branches++;
	}

	_vUniformBuffers.reserve(4);
}

void ShaderVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_descriptorPool, vkDestroyDescriptorPool(pRendererVulkan->GetVkDevice(), _descriptorPool, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_pipelineLayout, vkDestroyPipelineLayout(pRendererVulkan->GetVkDevice(), _pipelineLayout, pRendererVulkan->GetAllocator()));
	for (auto& x : _vDescriptorSetLayouts)
		VERUS_VULKAN_DESTROY(x, vkDestroyDescriptorSetLayout(pRendererVulkan->GetVkDevice(), x, pRendererVulkan->GetAllocator()));
	_vDescriptorSetLayouts.clear();
	for (auto& x : _vUniformBuffers)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));
	_vUniformBuffers.clear();
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

void ShaderVulkan::BindUniformBufferSource(int descSet, const void* pSrc, int size, int capacity, ShaderStageFlags stageFlags)
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_RT_ASSERT(!(reinterpret_cast<intptr_t>(pSrc) & 0xF));

	const VkPhysicalDeviceProperties* pPhysicalDeviceProperties = nullptr;
	vmaGetPhysicalDeviceProperties(pRendererVulkan->GetVmaAllocator(), &pPhysicalDeviceProperties);

	UniformBuffer uniformBuffer;
	uniformBuffer._pSrc = pSrc;
	uniformBuffer._size = size;
	uniformBuffer._sizeAligned = Math::AlignUp(uniformBuffer._size, static_cast<int>(pPhysicalDeviceProperties->limits.minUniformBufferOffsetAlignment));
	uniformBuffer._capacity = capacity;
	uniformBuffer._capacityInBytes = uniformBuffer._sizeAligned*uniformBuffer._capacity;
	uniformBuffer._stageFlags = ToNativeStageFlags(stageFlags);

	if (capacity > 0)
	{
		const VkDeviceSize bufferSize = uniformBuffer._capacityInBytes * BaseRenderer::s_ringBufferSize;
		pRendererVulkan->CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
			uniformBuffer._buffer, uniformBuffer._vmaAllocation);
	}

	if (_vUniformBuffers.size() <= descSet)
		_vUniformBuffers.resize(descSet + 1);
	_vUniformBuffers[descSet] = uniformBuffer;
}

void ShaderVulkan::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkPushConstantRange vkpcr = {};
	VkPipelineLayoutCreateInfo vkplci = {};
	vkplci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (_vUniformBuffers.empty())
	{
		vkplci.setLayoutCount = 0;
		vkplci.pushConstantRangeCount = 0;
	}
	else
	{
		_vDescriptorSetLayouts.reserve(_vUniformBuffers.size());
		for (auto& ub : _vUniformBuffers)
		{
			if (ub._capacity > 0)
			{
				VkDescriptorSetLayoutBinding vkdslb = {};
				vkdslb.binding = 0;
				vkdslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
				vkdslb.descriptorCount = 1;
				vkdslb.stageFlags = ub._stageFlags;
				vkdslb.pImmutableSamplers = nullptr;

				VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
				VkDescriptorSetLayoutCreateInfo vkdslci = {};
				vkdslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				vkdslci.bindingCount = 1;
				vkdslci.pBindings = &vkdslb;
				if (VK_SUCCESS != (res = vkCreateDescriptorSetLayout(pRendererVulkan->GetVkDevice(), &vkdslci, pRendererVulkan->GetAllocator(), &descriptorSetLayout)))
					throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorSetLayout(), res=" << res;

				_vDescriptorSetLayouts.push_back(descriptorSetLayout);
			}
			else
			{
				VERUS_RT_ASSERT(&ub == &_vUniformBuffers.back()); // Only the last set is allowed to use push constants.
				vkpcr.stageFlags = ub._stageFlags;
				vkpcr.offset = 0;
				vkpcr.size = ub._size;
				vkplci.pushConstantRangeCount = 1;
				vkplci.pPushConstantRanges = &vkpcr;
			}
		}

		vkplci.setLayoutCount = Utils::Cast32(_vDescriptorSetLayouts.size());
		vkplci.pSetLayouts = _vDescriptorSetLayouts.data();
	}

	if (VK_SUCCESS != (res = vkCreatePipelineLayout(pRendererVulkan->GetVkDevice(), &vkplci, pRendererVulkan->GetAllocator(), &_pipelineLayout)))
		throw VERUS_RUNTIME_ERROR << "vkCreatePipelineLayout(), res=" << res;

	if (vkplci.setLayoutCount)
	{
		CreateDescriptorPool();
		CreateDescriptorSets();
	}
}

void ShaderVulkan::BeginBindDescriptors()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const bool resetOffset = _currentFrame != renderer.GetNumFrames();
	_currentFrame = renderer.GetNumFrames();

	for (auto& ub : _vUniformBuffers)
	{
		if (!ub._capacity)
			continue;
		VERUS_RT_ASSERT(!ub._pMappedData);
		void* pData = nullptr;
		if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), ub._vmaAllocation, &pData)))
			throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
		ub._pMappedData = static_cast<BYTE*>(pData);
		ub._pMappedData += ub._capacityInBytes * pRendererVulkan->GetRingBufferIndex(); // Adjust address for this frame.
		if (resetOffset)
			ub._offset = 0;
	}
}

void ShaderVulkan::EndBindDescriptors()
{
	VERUS_QREF_RENDERER_VULKAN;

	for (auto& ub : _vUniformBuffers)
	{
		if (!ub._capacity)
			continue;
		VERUS_RT_ASSERT(ub._pMappedData);
		vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), ub._vmaAllocation);
		ub._pMappedData = nullptr;
	}
}

void ShaderVulkan::CreateDescriptorPool()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkDescriptorPoolSize vkdps = {};
	vkdps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	vkdps.descriptorCount = Utils::Cast32(_vDescriptorSetLayouts.size());
	VkDescriptorPoolCreateInfo vkdpci = {};
	vkdpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	vkdpci.maxSets = Utils::Cast32(_vDescriptorSetLayouts.size()); // Each set has one uniform buffer.
	vkdpci.poolSizeCount = 1;
	vkdpci.pPoolSizes = &vkdps;
	if (VK_SUCCESS != (res = vkCreateDescriptorPool(pRendererVulkan->GetVkDevice(), &vkdpci, pRendererVulkan->GetAllocator(), &_descriptorPool)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorPool(), res=" << res;
}

void ShaderVulkan::CreateDescriptorSets()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	_vDescriptorSets.resize(_vDescriptorSetLayouts.size());
	VkDescriptorSetAllocateInfo vkdsai = {};
	vkdsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	vkdsai.descriptorPool = _descriptorPool;
	vkdsai.descriptorSetCount = Utils::Cast32(_vDescriptorSetLayouts.size());
	vkdsai.pSetLayouts = _vDescriptorSetLayouts.data();
	if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, _vDescriptorSets.data())))
		throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(), res=" << res;

	int index = 0;
	for (auto& ub : _vUniformBuffers)
	{
		if (ub._capacity > 0)
		{
			VkDescriptorBufferInfo vkdbi = {};
			vkdbi.buffer = ub._buffer;
			vkdbi.offset = 0;
			vkdbi.range = ub._size;

			VkWriteDescriptorSet vkwds = {};
			vkwds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vkwds.dstSet = _vDescriptorSets[index];
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

VkDescriptorSet ShaderVulkan::GetVkDescriptorSet(int descSet)
{
	return _vDescriptorSets[descSet];
}

bool ShaderVulkan::TryPushConstants(int descSet, RBaseCommandBuffer cb)
{
	auto& ub = _vUniformBuffers[descSet];
	if (!ub._capacity)
	{
		auto& cbVulkan = static_cast<RCommandBufferVulkan>(cb);
		vkCmdPushConstants(cbVulkan.GetVkCommandBuffer(), _pipelineLayout, ub._stageFlags, 0, ub._size, ub._pSrc);
		return true;
	}
	return false;
}

int ShaderVulkan::UpdateUniformBuffer(int descSet)
{
	VERUS_QREF_RENDERER_VULKAN;

	auto& ub = _vUniformBuffers[descSet];
	if (ub._offset + ub._sizeAligned > ub._capacityInBytes)
		return -1;

	VERUS_RT_ASSERT(ub._pMappedData);
	const int ret = ub._capacityInBytes * pRendererVulkan->GetRingBufferIndex() + ub._offset;
	memcpy(ub._pMappedData + ub._offset, ub._pSrc, ub._size);
	ub._offset += ub._sizeAligned;
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
