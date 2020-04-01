#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

GeometryVulkan::GeometryVulkan()
{
}

GeometryVulkan::~GeometryVulkan()
{
	Done();
}

void GeometryVulkan::Init(RcGeometryDesc desc)
{
	VERUS_INIT();

	_dynBindingsMask = desc._dynBindingsMask;
	_32BitIndices = desc._32BitIndices;

	const int bindingCount = GetBindingCount(desc._pInputElementDesc);
	_vVertexInputBindingDesc.reserve(bindingCount);
	_vVertexInputAttributeDesc.reserve(GetInputElementDescCount(desc._pInputElementDesc));
	int i = 0;
	while (desc._pInputElementDesc[i]._offset >= 0)
	{
		int binding = desc._pInputElementDesc[i]._binding;
		VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		if (binding < 0)
		{
			binding = -binding;
			_instBindingsMask |= (1 << binding);
			inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		}

		bool found = false;
		for (const auto& x : _vVertexInputBindingDesc)
		{
			if (x.binding == binding)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			VkVertexInputBindingDescription vkvibd = {};
			vkvibd.binding = binding;
			vkvibd.stride = desc._pStrides[binding];
			vkvibd.inputRate = inputRate;
			_vVertexInputBindingDesc.push_back(vkvibd);
		}

		VkVertexInputAttributeDescription vkviad = {};
		vkviad.location = ToNativeLocation(desc._pInputElementDesc[i]._usage, desc._pInputElementDesc[i]._usageIndex);
		vkviad.binding = binding;
		vkviad.format = ToNativeFormat(desc._pInputElementDesc[i]._usage, desc._pInputElementDesc[i]._type, desc._pInputElementDesc[i]._components);
		vkviad.offset = desc._pInputElementDesc[i]._offset;
		_vVertexInputAttributeDesc.push_back(vkviad);
		i++;
	}

	_vStrides.reserve(bindingCount);
	i = 0;
	while (desc._pStrides[i] > 0)
	{
		_vStrides.push_back(desc._pStrides[i]);
		i++;
	}

	_vVertexBuffers.reserve(4);
	_vStagingVertexBuffers.reserve(4);
}

void GeometryVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	ForceScheduled();

	VERUS_VULKAN_DESTROY(_indexBuffer._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), _indexBuffer._buffer, _indexBuffer._vmaAllocation));
	for (auto& x : _vVertexBuffers)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));

	VERUS_DONE(GeometryVulkan);
}

void GeometryVulkan::CreateVertexBuffer(int count, int binding)
{
	VERUS_QREF_RENDERER_VULKAN;

	if (_vVertexBuffers.size() <= binding)
		_vVertexBuffers.resize(binding + 1);

	auto& vb = _vVertexBuffers[binding];
	const int elementSize = _vStrides[binding];
	vb._bufferSize = count * elementSize;

	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		pRendererVulkan->CreateBuffer(vb._bufferSize * BaseRenderer::s_ringBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
			vb._buffer, vb._vmaAllocation);
	}
	else
	{
		pRendererVulkan->CreateBuffer(vb._bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
			vb._buffer, vb._vmaAllocation);
	}
}

void GeometryVulkan::UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		auto& vb = _vVertexBuffers[binding];
		void* pData = nullptr;
		if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), vb._vmaAllocation, &pData)))
			throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
		BYTE* pMappedData = static_cast<BYTE*>(pData) + pRendererVulkan->GetRingBufferIndex() * vb._bufferSize;
		memcpy(pMappedData, p, vb._bufferSize);
		vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), vb._vmaAllocation);
	}
	else
	{
		if (_vStagingVertexBuffers.size() <= binding)
			_vStagingVertexBuffers.resize(binding + 1);

		auto& vb = _vVertexBuffers[binding];
		auto& svb = _vStagingVertexBuffers[binding];
		if (VK_NULL_HANDLE == svb._buffer)
		{
			pRendererVulkan->CreateBuffer(vb._bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
				svb._buffer, svb._vmaAllocation);
		}

		void* pData = nullptr;
		if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), svb._vmaAllocation, &pData)))
			throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
		memcpy(pData, p, vb._bufferSize);
		vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), svb._vmaAllocation);

		pRendererVulkan->CopyBuffer(svb._buffer, vb._buffer, vb._bufferSize, pCB);

		Schedule();
	}
}

void GeometryVulkan::CreateIndexBuffer(int count)
{
	VERUS_QREF_RENDERER_VULKAN;

	const int elementSize = _32BitIndices ? sizeof(UINT32) : sizeof(UINT16);
	_indexBuffer._bufferSize = count * elementSize;

	pRendererVulkan->CreateBuffer(_indexBuffer._bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
		_indexBuffer._buffer, _indexBuffer._vmaAllocation);
}

void GeometryVulkan::UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	if (VK_NULL_HANDLE == _stagingIndexBuffer._buffer)
	{
		pRendererVulkan->CreateBuffer(_indexBuffer._bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
			_stagingIndexBuffer._buffer, _stagingIndexBuffer._vmaAllocation);
	}

	void* pData = nullptr;
	if (VK_SUCCESS != (res = vmaMapMemory(pRendererVulkan->GetVmaAllocator(), _stagingIndexBuffer._vmaAllocation, &pData)))
		throw VERUS_RECOVERABLE << "vmaMapMemory(), res=" << res;
	memcpy(pData, p, _indexBuffer._bufferSize);
	vmaUnmapMemory(pRendererVulkan->GetVmaAllocator(), _stagingIndexBuffer._vmaAllocation);

	pRendererVulkan->CopyBuffer(_stagingIndexBuffer._buffer, _indexBuffer._buffer, _indexBuffer._bufferSize, pCB);

	Schedule();
}

Continue GeometryVulkan::Scheduled_Update()
{
	if (!IsScheduledAllowed())
		return Continue::yes;

	VERUS_QREF_RENDERER_VULKAN;
	VERUS_VULKAN_DESTROY(_stagingIndexBuffer._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), _stagingIndexBuffer._buffer, _stagingIndexBuffer._vmaAllocation));
	for (auto& x : _vStagingVertexBuffers)
		VERUS_VULKAN_DESTROY(x._buffer, vmaDestroyBuffer(pRendererVulkan->GetVmaAllocator(), x._buffer, x._vmaAllocation));

	return Continue::no;
}

VkPipelineVertexInputStateCreateInfo GeometryVulkan::GetVkPipelineVertexInputStateCreateInfo(UINT32 bindingsFilter,
	Vector<VkVertexInputBindingDescription>& vVertexInputBindingDesc, Vector<VkVertexInputAttributeDescription>& vVertexInputAttributeDesc) const
{
	if (UINT32_MAX == bindingsFilter)
	{
		VkPipelineVertexInputStateCreateInfo vertexInputState = {};
		vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputState.vertexBindingDescriptionCount = Utils::Cast32(_vVertexInputBindingDesc.size());
		vertexInputState.pVertexBindingDescriptions = _vVertexInputBindingDesc.data();
		vertexInputState.vertexAttributeDescriptionCount = Utils::Cast32(_vVertexInputAttributeDesc.size());
		vertexInputState.pVertexAttributeDescriptions = _vVertexInputAttributeDesc.data();
		return vertexInputState;
	}

	uint32_t replaceBinding[VERUS_MAX_VB] = {}; // For bindings compaction.
	int binding = 0;
	VERUS_FOR(i, VERUS_MAX_VB)
	{
		replaceBinding[i] = binding;
		if ((bindingsFilter >> i) & 0x1)
			binding++;
	}

	vVertexInputBindingDesc.reserve(_vVertexInputBindingDesc.size());
	vVertexInputAttributeDesc.reserve(_vVertexInputAttributeDesc.size());
	for (const auto& x : _vVertexInputBindingDesc)
	{
		if ((bindingsFilter >> x.binding) & 0x1)
		{
			vVertexInputBindingDesc.push_back(x);
			vVertexInputBindingDesc.back().binding = replaceBinding[x.binding];
		}
	}
	for (const auto& x : _vVertexInputAttributeDesc)
	{
		if ((bindingsFilter >> x.binding) & 0x1)
		{
			vVertexInputAttributeDesc.push_back(x);
			vVertexInputAttributeDesc.back().binding = replaceBinding[x.binding];
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = Utils::Cast32(vVertexInputBindingDesc.size());
	vertexInputState.pVertexBindingDescriptions = vVertexInputBindingDesc.data();
	vertexInputState.vertexAttributeDescriptionCount = Utils::Cast32(vVertexInputAttributeDesc.size());
	vertexInputState.pVertexAttributeDescriptions = vVertexInputAttributeDesc.data();
	return vertexInputState;
}

VkDeviceSize GeometryVulkan::GetVkVertexBufferOffset(int binding) const
{
	if (((_instBindingsMask | _dynBindingsMask) >> binding) & 0x1)
	{
		VERUS_QREF_RENDERER_VULKAN;
		auto& vb = _vVertexBuffers[binding];
		return pRendererVulkan->GetRingBufferIndex() * vb._bufferSize;
	}
	else
		return 0;
}
