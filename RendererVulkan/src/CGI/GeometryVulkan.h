#pragma once

namespace verus
{
	namespace CGI
	{
		class GeometryVulkan : public BaseGeometry
		{
			struct VkBufferEx
			{
				VkBuffer       _buffer = VK_NULL_HANDLE;
				VkDeviceMemory _deviceMemory = VK_NULL_HANDLE;
			};

			VkBufferEx                                _vertexBuffer;
			VkBufferEx                                _indexBuffer;
			VkBufferEx                                _stagingVertexBuffer;
			VkBufferEx                                _stagingIndexBuffer;
			Vector<VkVertexInputBindingDescription>   _vVertexInputBindingDesc;
			Vector<VkVertexInputAttributeDescription> _vVertexInputAttributeDesc;
			Vector<int>                               _vStrides;

		public:
			GeometryVulkan();
			virtual ~GeometryVulkan() override;

			virtual void Init(RcGeometryDesc desc) override;
			virtual void Done() override;

			virtual void BufferDataVB(const void* p, int num, int binding) override;
			virtual void BufferDataIB(const void* p, int num) override;

			//
			// Vulkan
			//

			void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
			void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

			VkPipelineVertexInputStateCreateInfo GetVkPipelineVertexInputStateCreateInfo() const;

			VkBuffer GetVkVertexBuffer() const { return _vertexBuffer._buffer; }
			VkBuffer GetVkIndexBuffer() const { return _indexBuffer._buffer; }
		};
		VERUS_TYPEDEFS(GeometryVulkan);
	}
}
