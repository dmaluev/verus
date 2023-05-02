// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_rph = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);

	CGI::ShaderDesc shaderDesc("[Shaders]:Ssr.hlsl");
	if (settings._postProcessSSR)
		shaderDesc._userDefines = "SSR";
	_shader.Init(shaderDesc);
	_shader->CreateDescriptorSet(0, &s_ubSsrVS, sizeof(s_ubSsrVS), 16, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubSsrFS, sizeof(s_ubSsrFS), 16,
		{
			CGI::Sampler::nearestClampMipN, // GBuffer0
			CGI::Sampler::nearestClampMipN, // GBuffer1
			CGI::Sampler::nearestClampMipN, // GBuffer2
			CGI::Sampler::nearestClampMipN, // GBuffer3
			CGI::Sampler::linearClampMipN, // Depth
			CGI::Sampler::nearestClampMipN, // AccAmb
			CGI::Sampler::linearClampMipL, // Image
			CGI::Sampler::linearClampMipN // Scene
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MAIN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#DebugCubeMap", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_DEBUG_CUBE_MAP].Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Ssr::Done()
{
	if (CGI::Renderer::IsLoaded())
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteFramebuffer(_fbh);
		renderer->DeleteRenderPass(_rph);
	}
	VERUS_DONE(Ssr);
}

void Ssr::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	{
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
	}
	{
		const int scaledCombinedSwapChainWidth = settings.Scale(renderer.GetCombinedSwapChainWidth());
		const int scaledCombinedSwapChainHeight = settings.Scale(renderer.GetCombinedSwapChainHeight());

		_fbh = renderer->CreateFramebuffer(_rph,
			{
				renderer.GetDS().GetLightAccSpecularTexture()
			},
			scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
		BindDescriptorSetTextures();
	}
}

bool Ssr::BindDescriptorSetTextures()
{
	VERUS_QREF_ATMO;
	VERUS_QREF_RENDERER;

	if (atmo.GetCubeMapBaker().GetColorTexture())
	{
		_csh = _shader->BindDescriptorSetTextures(1,
			{
				renderer.GetDS().GetGBuffer(0),
				renderer.GetDS().GetGBuffer(1),
				renderer.GetDS().GetGBuffer(2),
				renderer.GetDS().GetGBuffer(3),
				renderer.GetTexDepthStencil(),
				renderer.GetDS().GetLightAccAmbientTexture(),
				atmo.GetCubeMapBaker().GetColorTexture(),
				renderer.GetDS().GetComposedTextureA()
			});
		return true;
	}
	return false;
}

void Ssr::Generate()
{
	VERUS_QREF_ATMO;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!_csh.IsSet())
	{
		if (!BindDescriptorSetTextures())
			return;
	}

	if (_editMode)
	{
		ImGui::DragFloat("SSR radius", &_radius, 0.01f);
		ImGui::DragFloat("SSR depthBias", &_depthBias, 0.001f);
		ImGui::DragFloat("SSR thickness", &_thickness, 0.001f);
		ImGui::DragFloat("SSR equalizeDist", &_equalizeDist, 0.01f);
	}

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(255, 32, 255, 255), "Ssr/Generate");

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rph, _fbh, { renderer.GetDS().GetLightAccSpecularTexture()->GetClearValue() });

	World::RCamera passCamera = *wm.GetPassCamera();

	const Matrix4 matPTex = Matrix4(Math::ToUVMatrix()) * passCamera.GetMatrixP();

	s_ubSsrVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubSsrVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubSsrVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubSsrFS._matInvV = passCamera.GetMatrixInvV().UniformBufferFormat();
	s_ubSsrFS._matPTex = matPTex.UniformBufferFormat();
	s_ubSsrFS._matInvP = passCamera.GetMatrixInvP().UniformBufferFormat();
	s_ubSsrFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubSsrFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubSsrFS._zNearFarEx = passCamera.GetZNearFarEx().GLM();
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.x = _radius;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.y = _depthBias;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.z = _thickness;
	s_ubSsrFS._radius_depthBias_thickness_equalizeDist.w = _equalizeDist;

	cb->BindPipeline(_cubeMapDebugMode ? _pipe[PIPE_DEBUG_CUBE_MAP] : _pipe[PIPE_MAIN]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

	VERUS_PROFILER_END_EVENT(cb);
}
