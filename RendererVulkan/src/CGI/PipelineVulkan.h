#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineVulkan : public BasePipeline
		{
			VkPipeline       _pipeline = VK_NULL_HANDLE;
			VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

		public:
			PipelineVulkan();
			virtual ~PipelineVulkan() override;

			virtual void Init(RcPipelineDesc desc) override;
			virtual void Done() override;

			//
			// Vulkan
			//

			VkPipeline GetVkPipeline() const { return _pipeline; }
			VkPipelineLayout GetVkPipelineLayout() const { return _pipelineLayout; }

		private:
			void CreatePipelineLayout();
		};
		VERUS_TYPEDEFS(PipelineVulkan);
	}
}
