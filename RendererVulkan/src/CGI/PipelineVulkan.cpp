// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

PipelineVulkan::PipelineVulkan()
{
}

PipelineVulkan::~PipelineVulkan()
{
	Done();
}

void PipelineVulkan::Init(RcPipelineDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	if (desc._compute)
	{
		_compute = true;
		InitCompute(desc);
		return;
	}

	// Attachment count according to blend equations:
	int attachmentCount = 0;
	for (const auto& x : desc._colorAttachBlendEqs)
	{
		if (x.empty())
			break;
		attachmentCount++;
	}

	RcGeometryVulkan geo = static_cast<RcGeometryVulkan>(*desc._geometry);
	RcShaderVulkan shader = static_cast<RcShaderVulkan>(*desc._shader);

	_vertexInputBindingsFilter = desc._vertexInputBindingsFilter;
	const bool tess = desc._topology >= PrimitiveTopology::patchList3 && desc._topology <= PrimitiveTopology::patchList4;

	ShaderVulkan::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	Vector<String> entryNames(+BaseShader::Stage::count);
	Vector<VkPipelineShaderStageCreateInfo> vShaderStages;
	vShaderStages.reserve(compiled._stageCount);
	auto PushShaderStage = [&vShaderStages, &compiled, &entryNames](CSZ suffix, BaseShader::Stage stage, VkShaderStageFlagBits shaderStageFlagBits)
	{
		const VkShaderModule shaderModule = compiled._shaderModules[+stage];
		entryNames[+stage] = compiled._entry + suffix;
		if (shaderModule != VK_NULL_HANDLE)
		{
			VkPipelineShaderStageCreateInfo vkpssci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			vkpssci.stage = shaderStageFlagBits;
			vkpssci.module = shaderModule;
			vkpssci.pName = _C(entryNames[+stage]);
			vShaderStages.push_back(vkpssci);
		}
	};
	PushShaderStage("VS", BaseShader::Stage::vs, VK_SHADER_STAGE_VERTEX_BIT);
	PushShaderStage("HS", BaseShader::Stage::hs, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	PushShaderStage("DS", BaseShader::Stage::ds, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	PushShaderStage("GS", BaseShader::Stage::gs, VK_SHADER_STAGE_GEOMETRY_BIT);
	PushShaderStage("FS", BaseShader::Stage::fs, VK_SHADER_STAGE_FRAGMENT_BIT);

	Vector<VkVertexInputBindingDescription> vVertexInputBindingDesc;
	Vector<VkVertexInputAttributeDescription> vVertexInputAttributeDesc;
	VkPipelineVertexInputStateCreateInfo vertexInputState = geo.GetVkPipelineVertexInputStateCreateInfo(_vertexInputBindingsFilter,
		vVertexInputBindingDesc, vVertexInputAttributeDesc);

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyState.topology = ToNativePrimitiveTopology(desc._topology);
	inputAssemblyState.primitiveRestartEnable = desc._primitiveRestartEnable;

	VkPipelineTessellationStateCreateInfo tessellationState = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
	tessellationState.patchControlPoints = 3;
	switch (desc._topology)
	{
	case PrimitiveTopology::patchList4: tessellationState.patchControlPoints = 4; break;
	}

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationLineStateCreateInfoEXT rasterizationLineState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT };
	rasterizationLineState.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
	if (desc._colorAttachBlendEqs[0] == VERUS_COLOR_BLEND_ALPHA)
		rasterizationLineState.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;
	VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationState.pNext = pRendererVulkan->IsAdvancedLineRasterizationSupported() ? &rasterizationLineState : nullptr;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.polygonMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	rasterizationState.cullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = desc._rasterizationState._depthBiasEnable;
	rasterizationState.depthBiasConstantFactor = desc._rasterizationState._depthBiasConstantFactor * 0.5f; // Magic value to match D3D12.
	rasterizationState.depthBiasClamp = desc._rasterizationState._depthBiasClamp;
	rasterizationState.depthBiasSlopeFactor = desc._rasterizationState._depthBiasSlopeFactor;
	rasterizationState.lineWidth = 1;

	VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleState.rasterizationSamples = ToNativeSampleCount(desc._sampleCount);

	VkPipelineDepthStencilStateCreateInfo depthStencilState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencilState.depthTestEnable = desc._depthTestEnable;
	depthStencilState.depthWriteEnable = desc._depthWriteEnable;
	depthStencilState.depthCompareOp = ToNativeCompareOp(desc._depthCompareOp);
	depthStencilState.stencilTestEnable = desc._stencilTestEnable;

	VkPipelineColorBlendAttachmentState vkpcbas[VERUS_MAX_CA] = {};
	FillColorBlendAttachmentStates(desc, attachmentCount, vkpcbas);
	VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
	colorBlendState.attachmentCount = attachmentCount;
	colorBlendState.pAttachments = vkpcbas;
	colorBlendState.blendConstants[0] = 1;
	colorBlendState.blendConstants[1] = 1;
	colorBlendState.blendConstants[2] = 1;
	colorBlendState.blendConstants[3] = 1;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS };
	VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = VERUS_COUNT_OF(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo vkgpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	vkgpci.stageCount = Utils::Cast32(vShaderStages.size());
	vkgpci.pStages = vShaderStages.data();
	vkgpci.pVertexInputState = &vertexInputState;
	vkgpci.pInputAssemblyState = &inputAssemblyState;
	vkgpci.pTessellationState = tess ? &tessellationState : nullptr;
	vkgpci.pViewportState = &viewportState;
	vkgpci.pRasterizationState = &rasterizationState;
	vkgpci.pMultisampleState = &multisampleState;
	vkgpci.pDepthStencilState = &depthStencilState;
	vkgpci.pColorBlendState = &colorBlendState;
	vkgpci.pDynamicState = &dynamicState;
	vkgpci.layout = shader.GetVkPipelineLayout();
	vkgpci.renderPass = pRendererVulkan->GetRenderPass(desc._renderPassHandle);
	vkgpci.subpass = desc._subpass;
	vkgpci.basePipelineHandle = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkCreateGraphicsPipelines(pRendererVulkan->GetVkDevice(), VK_NULL_HANDLE, 1, &vkgpci, pRendererVulkan->GetAllocator(), &_pipeline)))
		throw VERUS_RUNTIME_ERROR << "vkCreateGraphicsPipelines(); res=" << res;
}

void PipelineVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_pipeline, vkDestroyPipeline(pRendererVulkan->GetVkDevice(), _pipeline, pRendererVulkan->GetAllocator()));

	VERUS_DONE(PipelineVulkan);
}

void PipelineVulkan::InitCompute(RcPipelineDesc desc)
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	RcShaderVulkan shader = static_cast<RcShaderVulkan>(*desc._shader);

	ShaderVulkan::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	const VkShaderModule shaderModule = compiled._shaderModules[+BaseShader::Stage::cs];
	String entryName = compiled._entry + "CS";

	VkComputePipelineCreateInfo vkcpci = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	vkcpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vkcpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	vkcpci.stage.module = shaderModule;
	vkcpci.stage.pName = _C(entryName);
	vkcpci.layout = shader.GetVkPipelineLayout();
	if (VK_SUCCESS != (res = vkCreateComputePipelines(pRendererVulkan->GetVkDevice(), VK_NULL_HANDLE, 1, &vkcpci, pRendererVulkan->GetAllocator(), &_pipeline)))
		throw VERUS_RUNTIME_ERROR << "vkCreateComputePipelines(); res=" << res;
}

void PipelineVulkan::FillColorBlendAttachmentStates(RcPipelineDesc desc, int attachmentCount, VkPipelineColorBlendAttachmentState vkpcbas[])
{
	VERUS_FOR(i, attachmentCount)
	{
		CSZ p = _C(desc._colorAttachWriteMasks[i]);
		while (*p)
		{
			switch (*p)
			{
			case 'r': vkpcbas[i].colorWriteMask |= VK_COLOR_COMPONENT_R_BIT; break;
			case 'g': vkpcbas[i].colorWriteMask |= VK_COLOR_COMPONENT_G_BIT; break;
			case 'b': vkpcbas[i].colorWriteMask |= VK_COLOR_COMPONENT_B_BIT; break;
			case 'a': vkpcbas[i].colorWriteMask |= VK_COLOR_COMPONENT_A_BIT; break;
			}
			p++;
		}

		if (desc._colorAttachBlendEqs[i] != "off")
		{
			vkpcbas[i].blendEnable = VK_TRUE;

			static const VkBlendFactor bfs[] =
			{
				VK_BLEND_FACTOR_ZERO,
				VK_BLEND_FACTOR_ONE,
				VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
				VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
				VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
				VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
				VK_BLEND_FACTOR_DST_ALPHA,
				VK_BLEND_FACTOR_DST_COLOR,
				VK_BLEND_FACTOR_CONSTANT_COLOR,
				VK_BLEND_FACTOR_SRC_ALPHA,
				VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
				VK_BLEND_FACTOR_SRC_COLOR
			};
			static const VkBlendOp ops[] =
			{
				VK_BLEND_OP_ADD,
				VK_BLEND_OP_SUBTRACT,
				VK_BLEND_OP_REVERSE_SUBTRACT,
				VK_BLEND_OP_MIN,
				VK_BLEND_OP_MAX
			};

			int colorBlendOp = -1;
			int alphaBlendOp = -1;
			int srcColorBlendFactor = -1;
			int dstColorBlendFactor = -1;
			int srcAlphaBlendFactor = -1;
			int dstAlphaBlendFactor = -1;

			BaseRenderer::SetAlphaBlendHelper(
				_C(desc._colorAttachBlendEqs[i]),
				colorBlendOp,
				alphaBlendOp,
				srcColorBlendFactor,
				dstColorBlendFactor,
				srcAlphaBlendFactor,
				dstAlphaBlendFactor);

			vkpcbas[i].srcColorBlendFactor = bfs[srcColorBlendFactor];
			vkpcbas[i].dstColorBlendFactor = bfs[dstColorBlendFactor];
			vkpcbas[i].colorBlendOp = ops[colorBlendOp];
			vkpcbas[i].srcAlphaBlendFactor = bfs[srcAlphaBlendFactor];
			vkpcbas[i].dstAlphaBlendFactor = bfs[dstAlphaBlendFactor];
			vkpcbas[i].alphaBlendOp = ops[alphaBlendOp];
		}
	}
}
