#include "stdafx.h"

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

	CreatePipelineLayout();

	RcGeometryVulkan geo = static_cast<RcGeometryVulkan>(*desc._geometry);
	RcShaderVulkan shader = static_cast<RcShaderVulkan>(*desc._shader);

	Vector<VkPipelineShaderStageCreateInfo> vShaderStages;
	vShaderStages.reserve(shader.GetNumStages(desc._shaderBranch));
	auto PushShaderStage = [&vShaderStages, &shader, &desc](CSZ name, BaseShader::Stage stage, VkShaderStageFlagBits shaderStageFlagBits)
	{
		const VkShaderModule shaderModule = shader.GetVkShaderModule(desc._shaderBranch, stage);
		if (shaderModule != VK_NULL_HANDLE)
		{
			VkPipelineShaderStageCreateInfo vkpssci = {};
			vkpssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vkpssci.stage = shaderStageFlagBits;
			vkpssci.module = shaderModule;
			vkpssci.pName = name;
			vShaderStages.push_back(vkpssci);
		}
	};

	PushShaderStage("mainVS", BaseShader::Stage::vs, VK_SHADER_STAGE_VERTEX_BIT);
	PushShaderStage("mainHS", BaseShader::Stage::hs, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	PushShaderStage("mainDS", BaseShader::Stage::ds, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	PushShaderStage("mainGS", BaseShader::Stage::gs, VK_SHADER_STAGE_GEOMETRY_BIT);
	PushShaderStage("mainFS", BaseShader::Stage::fs, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineVertexInputStateCreateInfo vertexInputState = geo.GetVkPipelineVertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = ToNativePrimitiveTopology(desc._topology);
	inputAssemblyState.primitiveRestartEnable = desc._primitiveRestartEnable;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = desc._rasterizationState._depthClampEnable;
	rasterizationState.rasterizerDiscardEnable = desc._rasterizationState._rasterizerDiscardEnable;
	rasterizationState.polygonMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	rasterizationState.cullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationState.depthBiasEnable = desc._rasterizationState._depthBiasEnable;
	rasterizationState.depthBiasConstantFactor = desc._rasterizationState._depthBiasConstantFactor;
	rasterizationState.depthBiasClamp = desc._rasterizationState._depthBiasClamp;
	rasterizationState.depthBiasSlopeFactor = desc._rasterizationState._depthBiasSlopeFactor;
	rasterizationState.lineWidth = desc._rasterizationState._lineWidth;

	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.rasterizationSamples = ToNativeSampleCount(desc._sampleCount);

	VkPipelineColorBlendAttachmentState vkpcbas = {};
	vkpcbas.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	vkpcbas.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &vkpcbas;
	colorBlendState.blendConstants[0] = 0;
	colorBlendState.blendConstants[1] = 0;
	colorBlendState.blendConstants[2] = 0;
	colorBlendState.blendConstants[3] = 0;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = VERUS_ARRAY_LENGTH(dynamicStates);
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo vkgpci = {};
	vkgpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	vkgpci.stageCount = Utils::Cast32(vShaderStages.size());
	vkgpci.pStages = vShaderStages.data();
	vkgpci.pVertexInputState = &vertexInputState;
	vkgpci.pInputAssemblyState = &inputAssemblyState;
	vkgpci.pViewportState = &viewportState;
	vkgpci.pRasterizationState = &rasterizationState;
	vkgpci.pMultisampleState = &multisampleState;
	vkgpci.pColorBlendState = &colorBlendState;
	vkgpci.pDynamicState = &dynamicState;
	vkgpci.layout = _pipelineLayout;
	vkgpci.renderPass = pRendererVulkan->GetRenderPassByID(desc._renderPassID);
	vkgpci.subpass = 0;
	vkgpci.basePipelineHandle = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkCreateGraphicsPipelines(pRendererVulkan->GetVkDevice(), VK_NULL_HANDLE, 1, &vkgpci, pRendererVulkan->GetAllocator(), &_pipeline)))
		throw VERUS_RUNTIME_ERROR << "vkCreateGraphicsPipelines(), res=" << res;
}

void PipelineVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_pipeline, vkDestroyPipeline(pRendererVulkan->GetVkDevice(), _pipeline, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_pipelineLayout, vkDestroyPipelineLayout(pRendererVulkan->GetVkDevice(), _pipelineLayout, pRendererVulkan->GetAllocator()));

	VERUS_DONE(PipelineVulkan);
}

void PipelineVulkan::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkPushConstantRange vkpcr = {};
	vkpcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	vkpcr.offset = 0;
	vkpcr.size = sizeof(matrix);

	VkPipelineLayoutCreateInfo vkplci = {};
	vkplci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vkplci.setLayoutCount = 0;
	vkplci.pushConstantRangeCount = 1;
	vkplci.pPushConstantRanges = &vkpcr;
	if (VK_SUCCESS != (res = vkCreatePipelineLayout(pRendererVulkan->GetVkDevice(), &vkplci, pRendererVulkan->GetAllocator(), &_pipelineLayout)))
		throw VERUS_RUNTIME_ERROR << "vkCreatePipelineLayout(), res=" << res;
}