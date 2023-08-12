// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class GeometryVulkan : public BaseGeometry
	{
		static const int s_maxStorageBuffers = 8;

		struct BufferEx
		{
			VmaAllocation _vmaAllocation = VK_NULL_HANDLE;
			VkBuffer      _buffer = VK_NULL_HANDLE;
			VkDeviceSize  _bufferSize = 0;
			INT64         _utilization = -1;
		};

		struct StorageBufferEx : BufferEx
		{
			VkDescriptorSet       _descriptorSet = VK_NULL_HANDLE;
			int                   _structSize = 0;
		};

		Vector<BufferEx>                          _vVertexBuffers;
		BufferEx                                  _indexBuffer;
		Vector<BufferEx>                          _vStagingVertexBuffers;
		BufferEx                                  _stagingIndexBuffer;
		Vector<StorageBufferEx>                   _vStorageBuffers;
		Vector<VkVertexInputBindingDescription>   _vVertexInputBindingDesc;
		Vector<VkVertexInputAttributeDescription> _vVertexInputAttributeDesc;
		Vector<int>                               _vStrides;
		Descriptors                               _descriptors;

	public:
		GeometryVulkan();
		virtual ~GeometryVulkan() override;

		virtual void Init(RcGeometryDesc desc) override;
		virtual void Done() override;

		virtual void CreateVertexBuffer(int count, int binding) override;
		virtual void UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;

		virtual void CreateIndexBuffer(int count) override;
		virtual void UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;

		virtual void CreateStorageBuffer(int count, int structSize, int sbIndex, ShaderStageFlags stageFlags) override;
		virtual void UpdateStorageBuffer(const void* p, int sbIndex, PBaseCommandBuffer pCB, INT64 size, INT64 offset) override;
		virtual int GetStorageBufferStructSize(int sbIndex) const override;

		virtual Continue Scheduled_Update() override;

		//
		// Vulkan
		//

		VkPipelineVertexInputStateCreateInfo GetVkPipelineVertexInputStateCreateInfo(UINT32 bindingsFilter,
			Vector<VkVertexInputBindingDescription>& vVertexInputBindingDesc, Vector<VkVertexInputAttributeDescription>& vVertexInputAttributeDesc) const;

		int GetVertexBufferCount() const { return Utils::Cast32(_vVertexBuffers.size()); }
		VkBuffer GetVkVertexBuffer(int binding) const { return _vVertexBuffers[binding]._buffer; }
		VkDeviceSize GetVkVertexBufferOffset(int binding) const;

		VkBuffer GetVkIndexBuffer() const { return _indexBuffer._buffer; }
		VkDeviceSize GetVkIndexBufferOffset() const;

		VkDescriptorSet GetVkDescriptorSet(int sbIndex) const { return _vStorageBuffers[sbIndex]._descriptorSet; }
		VkDeviceSize GetVkStorageBufferOffset(int sbIndex) const;

		void UpdateUtilization() const;;
	};
	VERUS_TYPEDEFS(GeometryVulkan);
}
