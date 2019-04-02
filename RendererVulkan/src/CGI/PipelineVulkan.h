#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineVulkan : public BasePipeline
		{
		public:
			PipelineVulkan();
			virtual ~PipelineVulkan() override;

			virtual void Init() override;
			virtual void Done() override;
		};
	}
}
