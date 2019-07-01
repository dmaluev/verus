#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineVulkan : public BasePipeline
		{
			VkPipeline _pipeline = VK_NULL_HANDLE;

		public:
			PipelineVulkan();
			virtual ~PipelineVulkan() override;

			virtual void Init(RcPipelineDesc desc) override;
			virtual void Done() override;

			//
			// Vulkan
			//

			VkPipeline GetVkPipeline() const { return _pipeline; }
		};
		VERUS_TYPEDEFS(PipelineVulkan);
	}
}
