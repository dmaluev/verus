// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class PipelineVulkan : public BasePipeline
	{
		VkPipeline _pipeline = VK_NULL_HANDLE;
		bool       _compute = false;

	public:
		PipelineVulkan();
		virtual ~PipelineVulkan() override;

		virtual void Init(RcPipelineDesc desc) override;
		virtual void Done() override;

		//
		// Vulkan
		//

		VERUS_P(void InitCompute(RcPipelineDesc desc));
		bool IsCompute() const { return _compute; }
		VkPipeline GetVkPipeline() const { return _pipeline; }
		void FillColorBlendAttachmentStates(RcPipelineDesc desc, int attachmentCount, VkPipelineColorBlendAttachmentState vkpcbas[]);
	};
	VERUS_TYPEDEFS(PipelineVulkan);
}
