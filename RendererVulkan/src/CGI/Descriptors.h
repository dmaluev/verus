// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class Descriptors : public Object
	{
		typedef Map<VkDescriptorType, int> TMapTypeCount;

		struct SetLayoutDesc
		{
			Vector<VkDescriptorSetLayoutBinding> _vSetLayoutBindings;
			int                                  _capacity = 0;
		};

		TMapTypeCount                  _mapTypeCount;
		Vector<SetLayoutDesc>          _vSetLayoutDesc;
		Vector<VkDescriptorSetLayout>  _vSetLayouts;
		Vector<VkDescriptorBufferInfo> _vBufferInfo;
		Vector<VkDescriptorImageInfo>  _vImageInfo;
		Vector<VkWriteDescriptorSet>   _vWriteSet;
		VkDescriptorPool               _pool = VK_NULL_HANDLE;
		VkDescriptorSet                _activeSet = VK_NULL_HANDLE;
		int                            _activeSetNumber = 0;
		int                            _complexCapacity = 0;

	public:
		Descriptors();
		~Descriptors();

		void Init();
		void Done();

		void BeginCreateSetLayout(int setNumber, int capacity);
		void SetLayoutBinding(int binding, VkDescriptorType type, int count, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSampler = nullptr);
		void EndCreateSetLayout();

		bool CreatePool(bool freeDescriptorSet);

		bool BeginAllocateSet(int setNumber);
		void BufferInfo(int binding, VkBuffer buffer, VkDeviceSize range, VkDeviceSize offset = 0);
		void ImageInfo(int binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
		VkDescriptorSet EndAllocateSet();

		void IncreaseComplexCapacityBy(int capacity) { _complexCapacity += capacity; }
		int GetComplexCapacity() const { return _complexCapacity; }

		int GetSetLayoutCount() const;
		const VkDescriptorSetLayout* GetSetLayouts() const;

		VkDescriptorPool GetPool() const { return _pool; }

		template<typename T>
		void ForEach(int setNumber, const T& fn)
		{
			for (auto& x : _vSetLayoutDesc[setNumber]._vSetLayoutBindings)
			{
				if (Continue::no == fn(x))
					break;
			}
		}
	};
	VERUS_TYPEDEFS(Descriptors);
}
