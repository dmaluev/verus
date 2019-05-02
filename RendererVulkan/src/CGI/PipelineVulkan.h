#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineVulkan : public BasePipeline
		{
			VkPipeline       _pipeline = VK_NULL_HANDLE;
			VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
			VkRenderPass     _renderPass = VK_NULL_HANDLE;

		public:
			PipelineVulkan();
			virtual ~PipelineVulkan() override;

			virtual void Init(RcPipelineDesc desc) override;
			virtual void Done() override;

		private:
			void CreatePipelineLayout();
			void CreateRenderPass();
		};
		VERUS_TYPEDEFS(PipelineVulkan);
	}
}
