#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryVulkan : public BaseGeometry
		{
			struct VkBufferEx
			{
				VkBuffer      _buffer = VK_NULL_HANDLE;
				VmaAllocation _vmaAllocation = VK_NULL_HANDLE;
				VkDeviceSize  _bufferSize = 0;
			};

			Vector<VkBufferEx>                        _vVertexBuffers;
			VkBufferEx                                _indexBuffer;
			Vector<VkBufferEx>                        _vStagingVertexBuffers;
			VkBufferEx                                _stagingIndexBuffer;
			Vector<VkVertexInputBindingDescription>   _vVertexInputBindingDesc;
			Vector<VkVertexInputAttributeDescription> _vVertexInputAttributeDesc;
			Vector<int>                               _vStrides;
			DestroyStaging                            _destroyStagingBuffers;

		public:
			GeometryVulkan();
			virtual ~GeometryVulkan() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;

			virtual void CreateVertexBuffer(int num, int binding) override;
			virtual void UpdateVertexBuffer(const void* p, int binding, PBaseCommandBuffer pCB) override;

			virtual void CreateIndexBuffer(int num) override;
			virtual void UpdateIndexBuffer(const void* p, PBaseCommandBuffer pCB) override;

			//
			// Vulkan
			//

			void DestroyStagingBuffers();

			VkPipelineVertexInputStateCreateInfo GetVkPipelineVertexInputStateCreateInfo(UINT32 bindingsFilter,
				Vector<VkVertexInputBindingDescription>& vVertexInputBindingDesc, Vector<VkVertexInputAttributeDescription>& vVertexInputAttributeDesc) const;

			int GetNumVertexBuffers() const { return Utils::Cast32(_vVertexBuffers.size()); }
			VkBuffer GetVkVertexBuffer(int binding) const { return _vVertexBuffers[binding]._buffer; }
			VkBuffer GetVkIndexBuffer() const { return _indexBuffer._buffer; }
		};
		VERUS_TYPEDEFS(GeometryVulkan);
	}
}
