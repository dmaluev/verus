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
	CreateRenderPass();

	Vector<VkPipelineShaderStageCreateInfo> vShaderStages;
	vShaderStages.reserve(5);
	RcShaderVulkan shader = static_cast<RcShaderVulkan>(desc._shader);
	auto PushShaderStage = [&vShaderStages, &shader, &desc](CSZ name, BaseShader::Stage stage, VkShaderStageFlagBits shaderStageFlagBits)
	{
		VkPipelineShaderStageCreateInfo vkpssci = {};
		vkpssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vkpssci.stage = shaderStageFlagBits;
		vkpssci.module = shader.GetVkShaderModule(desc._shaderBranch, stage);
		vkpssci.pName = name;
		vShaderStages.push_back(vkpssci);
	};

	PushShaderStage("mainVS", BaseShader::Stage::vs, VK_SHADER_STAGE_VERTEX_BIT);
	PushShaderStage("mainHS", BaseShader::Stage::hs, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	PushShaderStage("mainDS", BaseShader::Stage::ds, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	PushShaderStage("mainGS", BaseShader::Stage::gs, VK_SHADER_STAGE_GEOMETRY_BIT);
	PushShaderStage("mainFS", BaseShader::Stage::fs, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.vertexBindingDescriptionCount = 0;
	vertexInputState.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = ToNativePrimitiveTopology(desc._topology);
	inputAssemblyState.primitiveRestartEnable = desc._primitiveRestartEnable;

	VkViewport viewport = {};
	viewport.x = desc._viewport.getX();
	viewport.y = desc._viewport.getY();
	viewport.width = desc._viewport.Width();
	viewport.height = desc._viewport.Height();
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	VkRect2D scissor = {};
	scissor.offset.x = static_cast<int32_t>(desc._scissor.getX());
	scissor.offset.y = static_cast<int32_t>(desc._scissor.getY());
	scissor.extent.width = static_cast<uint32_t>(desc._scissor.Width());
	scissor.extent.height = static_cast<uint32_t>(desc._scissor.Height());
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

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
	vkgpci.layout = _pipelineLayout;
	vkgpci.renderPass = _renderPass;
	vkgpci.subpass = 0;
	vkgpci.basePipelineHandle = VK_NULL_HANDLE;
	if (VK_SUCCESS != (res = vkCreateGraphicsPipelines(pRendererVulkan->GetDevice(), VK_NULL_HANDLE, 1, &vkgpci, pRendererVulkan->GetAllocator(), &_pipeline)))
		throw VERUS_RUNTIME_ERROR << "vkCreateGraphicsPipelines(), res=" << res;
}

void PipelineVulkan::Done()
{
	VERUS_QREF_RENDERER_VULKAN;

	VERUS_VULKAN_DESTROY(_pipeline, vkDestroyPipeline(pRendererVulkan->GetDevice(), _pipeline, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_pipelineLayout, vkDestroyPipelineLayout(pRendererVulkan->GetDevice(), _pipelineLayout, pRendererVulkan->GetAllocator()));
	VERUS_VULKAN_DESTROY(_renderPass, vkDestroyRenderPass(pRendererVulkan->GetDevice(), _renderPass, pRendererVulkan->GetAllocator()));

	VERUS_DONE(PipelineVulkan);
}

void PipelineVulkan::CreatePipelineLayout()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;
	VkPipelineLayoutCreateInfo vkplci = {};
	vkplci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vkplci.setLayoutCount = 0;
	vkplci.pushConstantRangeCount = 0;
	if (VK_SUCCESS != (res = vkCreatePipelineLayout(pRendererVulkan->GetDevice(), &vkplci, pRendererVulkan->GetAllocator(), &_pipelineLayout)))
		throw VERUS_RUNTIME_ERROR << "vkCreatePipelineLayout(), res=" << res;
}

void PipelineVulkan::CreateRenderPass()
{
	VERUS_QREF_RENDERER_VULKAN;
	VkResult res = VK_SUCCESS;

	VkAttachmentDescription attachmentDesc = {};
	attachmentDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachmentRef = {};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo vkrpci = {};
	vkrpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	vkrpci.attachmentCount = 1;
	vkrpci.pAttachments = &attachmentDesc;
	vkrpci.subpassCount = 1;
	vkrpci.pSubpasses = &subpass;
	vkrpci.dependencyCount = 1;
	vkrpci.pDependencies = &dependency;
	if (VK_SUCCESS != (res = vkCreateRenderPass(pRendererVulkan->GetDevice(), &vkrpci, nullptr, &_renderPass)))
		throw VERUS_RUNTIME_ERROR << "vkCreateRenderPass(), res=" << res;
}
