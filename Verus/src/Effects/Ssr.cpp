// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Ssr::UB_SsrVS Ssr::s_ubSsrVS;
Ssr::UB_SsrFS Ssr::s_ubSsrFS;

Ssr::Ssr()
{
}

Ssr::~Ssr()
{
	Done();
}

void Ssr::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_rph = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Color", CGI::Format::floatR11G11B10).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Alpha", CGI::Format::unormR8).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly)
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("Color", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("Alpha", CGI::ImageLayout::colorAttachment)
				})
		},
		{});

	_shader.Init("[Shaders]:Ssr.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubSsrVS, sizeof(s_ubSsrVS), 1, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubSsrFS, sizeof(s_ubSsrFS), 1,
		{
			CGI::Sampler::linearClampMipN, // Color
			CGI::Sampler::nearestClampMipN, // Normal
			CGI::Sampler::linearClampMipN, // Depth
			CGI::Sampler::linearClampMipN // EnvMap
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Ssr::Done()
{
	VERUS_DONE(Ssr);
}

void Ssr::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SSAO;
	VERUS_QREF_ATMO;

	{
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
	}
	{
		_fbh = renderer->CreateFramebuffer(_rph,
			{
				renderer.GetDS().GetLightAccSpecTexture(),
				ssao.GetTexture()
			},
			renderer.GetSwapChainWidth(), renderer.GetSwapChainHeight());
		if (atmo.GetCubeMap().GetColorTexture())
		{
			_csh = _shader->BindDescriptorSetTextures(1,
				{
					renderer.GetDS().GetComposedTextureA(),
					renderer.GetDS().GetGBuffer(1),
					renderer.GetTexDepthStencil(),
					atmo.GetCubeMap().GetColorTexture()
				});
		}
	}
}

void Ssr::Generate()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_SSAO;
	VERUS_QREF_ATMO;

	if (!_csh.IsSet())
	{
		if (atmo.GetCubeMap().GetColorTexture())
		{
			_csh = _shader->BindDescriptorSetTextures(1,
				{
					renderer.GetDS().GetComposedTextureA(),
					renderer.GetDS().GetGBuffer(1),
					renderer.GetTexDepthStencil(),
					atmo.GetCubeMap().GetColorTexture()
				});
		}
		else
			return;
	}

	if (_editMode)
	{
		ImGui::DragFloat("SSR radius", &_radius, 0.01f);
		ImGui::DragFloat("SSR depthBias", &_depthBias, 0.001f);
		ImGui::DragFloat("SSR thickness", &_thickness, 0.001f);
		ImGui::DragFloat("SSR equalizeDist", &_equalizeDist, 0.01f);
		ImGui::Checkbox("SSR blur", &_blur);
	}

	Scene::RCamera cam = *sm.GetCamera();

	const Matrix4 matPTex = Matrix4(Math::ToUVMatrix()) * cam.GetMatrixP();

	auto cb = renderer.GetCommandBuffer();

	s_ubSsrVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubSsrVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubSsrFS._matInvV = cam.GetMatrixVi().UniformBufferFormat();
	s_ubSsrFS._matPTex = matPTex.UniformBufferFormat();
	s_ubSsrFS._matInvP = cam.GetMatrixPi().UniformBufferFormat();
	s_ubSsrFS._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.x = _radius;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.y = _depthBias;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.z = _thickness;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.w = _equalizeDist;

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rph, _fbh,
		{
			renderer.GetDS().GetLightAccSpecTexture()->GetClearValue(),
			ssao.GetTexture()->GetClearValue()
		});

	cb->BindPipeline(_pipe);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

	if (_blur)
		Blur::I().GenerateForSsr();
}
