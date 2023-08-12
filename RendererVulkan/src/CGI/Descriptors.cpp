// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

Descriptors::Descriptors()
{
}

Descriptors::~Descriptors()
{
	Done();
}

void Descriptors::Init()
{
	_vSetLayoutDesc.reserve(8);
	_vSetLayouts.reserve(8);
	_vBufferInfo.reserve(8);
	_vImageInfo.reserve(8);
	_vWriteSet.reserve(8);
}

void Descriptors::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_pool, vkDestroyDescriptorPool(pRendererVulkan->GetVkDevice(), _pool, pRendererVulkan->GetAllocator()));
	for (auto& x : _vSetLayouts)
		VERUS_VULKAN_DESTROY(x, vkDestroyDescriptorSetLayout(pRendererVulkan->GetVkDevice(), x, pRendererVulkan->GetAllocator()));
	_vSetLayouts.clear();

	VERUS_DONE(Descriptors);
}

void Descriptors::BeginCreateSetLayout(int setNumber, int capacity)
{
	VERUS_RT_ASSERT(setNumber == _vSetLayoutDesc.size());
	VERUS_RT_ASSERT(setNumber == _vSetLayouts.size());

	_activeSetNumber = setNumber;

	_vSetLayoutDesc.resize(_vSetLayoutDesc.size() + 1);
	_vSetLayouts.resize(_vSetLayouts.size() + 1);

	_vSetLayoutDesc.back()._vSetLayoutBindings.reserve(8);
	_vSetLayoutDesc.back()._capacity = capacity;
}

void Descriptors::SetLayoutBinding(int binding, VkDescriptorType type, int count, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSampler)
{
	VkDescriptorSetLayoutBinding vkdslb = {};
	vkdslb.binding = binding;
	vkdslb.descriptorType = type;
	vkdslb.descriptorCount = count;
	vkdslb.stageFlags = stageFlags;
	vkdslb.pImmutableSamplers = pImmutableSampler;
	_vSetLayoutDesc.back()._vSetLayoutBindings.push_back(vkdslb);

	switch (type)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		_mapTypeCount[type]++; // With dynamic offsets only one descriptor is required.
		break;
	default:
		_mapTypeCount[type] += _vSetLayoutDesc.back()._capacity;
	}
}

void Descriptors::EndCreateSetLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkDescriptorSetLayoutCreateInfo vkdslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	vkdslci.bindingCount = Utils::Cast32(_vSetLayoutDesc.back()._vSetLayoutBindings.size());
	vkdslci.pBindings = _vSetLayoutDesc.back()._vSetLayoutBindings.data();
	if (VK_SUCCESS != (res = vkCreateDescriptorSetLayout(pRendererVulkan->GetVkDevice(), &vkdslci, pRendererVulkan->GetAllocator(), &_vSetLayouts.back())))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorSetLayout(); res=" << res;
}

bool Descriptors::CreatePool(bool freeDescriptorSet)
{
	if (_vSetLayoutDesc.empty())
		return false;

	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	// Every complex descriptor set must start with uniform buffer.
	auto it = _mapTypeCount.find(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
	if (_mapTypeCount.end() != it)
		it->second += _complexCapacity; // Increase uniform buffer descriptor count.

	VkDescriptorPoolCreateInfo vkdpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	vkdpci.flags = freeDescriptorSet ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
	Vector<VkDescriptorPoolSize> vDescriptorPoolSizes;
	vDescriptorPoolSizes.reserve(_mapTypeCount.size());
	for (const auto& [key, value] : _mapTypeCount)
	{
		VkDescriptorPoolSize vkdps;
		vkdps.type = key;
		vkdps.descriptorCount = value;
		vDescriptorPoolSizes.push_back(vkdps);

		switch (key)
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			vkdpci.maxSets += value;
		}
	}
	vkdpci.poolSizeCount = Utils::Cast32(vDescriptorPoolSizes.size());
	vkdpci.pPoolSizes = vDescriptorPoolSizes.data();
	if (VK_SUCCESS != (res = vkCreateDescriptorPool(pRendererVulkan->GetVkDevice(), &vkdpci, pRendererVulkan->GetAllocator(), &_pool)))
		throw VERUS_RUNTIME_ERROR << "vkCreateDescriptorPool(); res=" << res;

	return true;
}

bool Descriptors::BeginAllocateSet(int setNumber)
{
	if (setNumber >= _vSetLayouts.size())
		return false;

	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	_activeSetNumber = setNumber;

	_vBufferInfo.clear();
	_vImageInfo.clear();
	_vWriteSet.clear();

	VkDescriptorSetAllocateInfo vkdsai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	vkdsai.descriptorPool = _pool;
	vkdsai.descriptorSetCount = 1;
	vkdsai.pSetLayouts = &_vSetLayouts[_activeSetNumber];
	if (VK_SUCCESS != (res = vkAllocateDescriptorSets(pRendererVulkan->GetVkDevice(), &vkdsai, &_activeSet)))
		throw VERUS_RUNTIME_ERROR << "vkAllocateDescriptorSets(); res=" << res;

	return true;
}

void Descriptors::BufferInfo(int binding, VkBuffer buffer, VkDeviceSize range, VkDeviceSize offset)
{
	VkDescriptorBufferInfo vkdbi = {};
	vkdbi.buffer = buffer;
	vkdbi.offset = offset;
	vkdbi.range = range;
	_vBufferInfo.push_back(vkdbi);

	VkWriteDescriptorSet vkwds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	vkwds.dstSet = _activeSet;
	vkwds.dstBinding = binding;
	vkwds.dstArrayElement = 0;
	vkwds.descriptorCount = 1;
	vkwds.descriptorType = _vSetLayoutDesc[_activeSetNumber]._vSetLayoutBindings[binding].descriptorType;
	vkwds.pBufferInfo = reinterpret_cast<VkDescriptorBufferInfo*>(_vBufferInfo.size());
	_vWriteSet.push_back(vkwds);
}

void Descriptors::ImageInfo(int binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo vkdii = {};
	vkdii.sampler = sampler;
	vkdii.imageView = imageView;
	vkdii.imageLayout = imageLayout;
	_vImageInfo.push_back(vkdii);

	VkWriteDescriptorSet vkwds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	vkwds.dstSet = _activeSet;
	vkwds.dstBinding = binding;
	vkwds.dstArrayElement = 0;
	vkwds.descriptorCount = 1;
	vkwds.descriptorType = _vSetLayoutDesc[_activeSetNumber]._vSetLayoutBindings[binding].descriptorType;
	vkwds.pImageInfo = reinterpret_cast<VkDescriptorImageInfo*>(_vImageInfo.size());
	_vWriteSet.push_back(vkwds);
}

VkDescriptorSet Descriptors::EndAllocateSet()
{
	VERUS_QREF_RENDERER_VULKAN;

	if (!_vWriteSet.empty())
	{
		for (auto& x : _vWriteSet) // Convert indices to pointers:
		{
			if (x.pBufferInfo)
				x.pBufferInfo = &_vBufferInfo[reinterpret_cast<size_t>(x.pBufferInfo) - 1];
			if (x.pImageInfo)
				x.pImageInfo = &_vImageInfo[reinterpret_cast<size_t>(x.pImageInfo) - 1];
		}
		vkUpdateDescriptorSets(pRendererVulkan->GetVkDevice(), Utils::Cast32(_vWriteSet.size()), _vWriteSet.data(), 0, nullptr);
	}

	_vBufferInfo.clear();
	_vImageInfo.clear();
	_vWriteSet.clear();

	return _activeSet;
}

int Descriptors::GetSetLayoutCount() const
{
	return Utils::Cast32(_vSetLayouts.size());
}

const VkDescriptorSetLayout* Descriptors::GetSetLayouts() const
{
	return _vSetLayouts.data();
}
