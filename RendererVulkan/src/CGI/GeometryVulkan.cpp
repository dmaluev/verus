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

	_32bitIndices = desc._32bitIndices;

	_vVertexInputBindingDesc.reserve(GetNumBindings(desc._pInputElementDesc));
	_vVertexInputAttributeDesc.reserve(GetNumInputElementDesc(desc._pInputElementDesc));
	int i = 0;
	while (desc._pInputElementDesc[i]._offset >= 0)
	{
		int binding = desc._pInputElementDesc[i]._binding;
		VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		if (binding < 0)
		{
			binding = -binding;
			_bindingInstMask |= (1 << binding);
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

	_vStrides.reserve(GetNumBindings(desc._pInputElementDesc));
	i = 0;
	while (desc._pStrides[i] > 0)
	{
		_vStrides.push_back(desc._pStrides[i]);
		i++;
	}
}

void GeometryVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_vertexBuffer._buffer, vkDestroyBuffer(pRendererVulkan->GetVkDevice(), _vertexBuffer._buffer, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_vertexBuffer._deviceMemory, vkFreeMemory(pRendererVulkan->GetVkDevice(), _vertexBuffer._deviceMemory, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_indexBuffer._buffer, vkDestroyBuffer(pRendererVulkan->GetVkDevice(), _indexBuffer._buffer, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_indexBuffer._deviceMemory, vkFreeMemory(pRendererVulkan->GetVkDevice(), _indexBuffer._deviceMemory, pRendererVulkan->GetAllocator()));

	VERUS_DONE(GeometryVulkan);
}

void GeometryVulkan::BufferDataVB(const void* p, int num, int binding)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const int elementSize = _vStrides[binding];
	const VkDeviceSize bufferSize = num * elementSize;

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		_stagingVertexBuffer._buffer, _stagingVertexBuffer._deviceMemory);

	void* pData = nullptr;
	if (VK_SUCCESS != (res = vkMapMemory(pRendererVulkan->GetVkDevice(), _stagingVertexBuffer._deviceMemory, 0, bufferSize, 0, &pData)))
		throw VERUS_RECOVERABLE << "vkMapMemory(), res=" << res;
	memcpy(pData, p, bufferSize);
	vkUnmapMemory(pRendererVulkan->GetVkDevice(), _stagingVertexBuffer._deviceMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_vertexBuffer._buffer, _vertexBuffer._deviceMemory);

	CopyBuffer(_stagingVertexBuffer._buffer, _vertexBuffer._buffer, bufferSize);
}

void GeometryVulkan::BufferDataIB(const void* p, int num)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	const int elementSize = _32bitIndices ? sizeof(UINT32) : sizeof(UINT16);
	const VkDeviceSize bufferSize = num * elementSize;

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		_stagingIndexBuffer._buffer, _stagingIndexBuffer._deviceMemory);

	void* pData = nullptr;
	if (VK_SUCCESS != (res = vkMapMemory(pRendererVulkan->GetVkDevice(), _stagingIndexBuffer._deviceMemory, 0, bufferSize, 0, &pData)))
		throw VERUS_RECOVERABLE << "vkMapMemory(), res=" << res;
	memcpy(pData, p, bufferSize);
	vkUnmapMemory(pRendererVulkan->GetVkDevice(), _stagingIndexBuffer._deviceMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_indexBuffer._buffer, _indexBuffer._deviceMemory);

	CopyBuffer(_stagingIndexBuffer._buffer, _indexBuffer._buffer, bufferSize);
}

void GeometryVulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkBufferCreateInfo vkbci = {};
	vkbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vkbci.size = size;
	vkbci.usage = usage;
	vkbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (VK_SUCCESS != (res = vkCreateBuffer(pRendererVulkan->GetVkDevice(), &vkbci, pRendererVulkan->GetAllocator(), &buffer)))
		throw VERUS_RECOVERABLE << "vkCreateBuffer(), res=" << res;

	VkMemoryRequirements vkmr = {};
	vkGetBufferMemoryRequirements(pRendererVulkan->GetVkDevice(), buffer, &vkmr);
	VkMemoryAllocateInfo vkmai = {};
	vkmai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkmai.allocationSize = vkmr.size;
	vkmai.memoryTypeIndex = pRendererVulkan->FindMemoryType(vkmr.memoryTypeBits, properties);
	if (VK_SUCCESS != (res = vkAllocateMemory(pRendererVulkan->GetVkDevice(), &vkmai, pRendererVulkan->GetAllocator(), &bufferMemory)))
		throw VERUS_RECOVERABLE << "vkAllocateMemory(), res=" << res;

	vkBindBufferMemory(pRendererVulkan->GetVkDevice(), buffer, bufferMemory, 0);
}

void GeometryVulkan::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VERUS_QREF_RENDERER;

	auto commandBuffer = static_cast<PCommandBufferVulkan>(&(*renderer.GetCommandBuffer()))->GetVkCommandBuffer();
	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
}

VkPipelineVertexInputStateCreateInfo GeometryVulkan::GetVkPipelineVertexInputStateCreateInfo() const
{
	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = Utils::Cast32(_vVertexInputBindingDesc.size());
	vertexInputState.pVertexBindingDescriptions = _vVertexInputBindingDesc.data();
	vertexInputState.vertexAttributeDescriptionCount = Utils::Cast32(_vVertexInputAttributeDesc.size());
	vertexInputState.pVertexAttributeDescriptions = _vVertexInputAttributeDesc.data();
	return vertexInputState;
}
