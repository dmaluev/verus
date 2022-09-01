// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Bloom::UB_BloomVS            Bloom::s_ubBloomVS;
Bloom::UB_BloomFS            Bloom::s_ubBloomFS;
Bloom::UB_BloomLightShaftsFS Bloom::s_ubBloomLightShaftsFS;

Bloom::Bloom()
{
}

Bloom::~Bloom()
{
	Done();
}

void Bloom::Init()
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessBloom)
	{
		VERUS_LOG_INFO("Bloom disabled");
		return;
	}

	_rph = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_rphLightShafts = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8, CGI::RP::Attachment::LoadOp::load);

	_shader.Init("[Shaders]:Bloom.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubBloomVS, sizeof(s_ubBloomVS), 16, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBloomFS, sizeof(s_ubBloomFS), 16,
		{
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubBloomLightShaftsFS, sizeof(s_ubBloomLightShaftsFS), 16,
		{
			CGI::Sampler::linearClampMipN,
			CGI::Sampler::shadow
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MAIN].Init(pipeDesc);
	}
	if (settings._postProcessLightShafts)
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#LightShafts", _rphLightShafts);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_LIGHT_SHAFTS].Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Bloom::InitByAtmosphere(CGI::TexturePtr texShadow)
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessBloom)
		return;

	_texAtmoShadow = texShadow;

	_cshLightShafts = _shader->BindDescriptorSetTextures(2, { renderer.GetTexDepthStencil(), _texAtmoShadow });
}

void Bloom::Done()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbh);
	renderer->DeleteRenderPass(_rphLightShafts);
	renderer->DeleteRenderPass(_rph);
	VERUS_DONE(Bloom);
}

void Bloom::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessBloom)
		return;

	{
		_shader->FreeDescriptorSet(_cshLightShafts);
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
		_tex.Done();
	}
	{
		const int scaled05CombinedSwapChainWidth = settings.Scale(renderer.GetCombinedSwapChainWidth(), 0.5f);
		const int scaled05CombinedSwapChainHeight = settings.Scale(renderer.GetCombinedSwapChainHeight(), 0.5f);

		CGI::TextureDesc texDesc;
		texDesc._name = "Bloom.Ping";
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = scaled05CombinedSwapChainWidth;
		texDesc._height = scaled05CombinedSwapChainHeight;
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex[TEX_PING].Init(texDesc);
		texDesc._name = "Bloom.Pong";
		_tex[TEX_PONG].Init(texDesc);
		_fbh = renderer->CreateFramebuffer(_rph, { _tex[TEX_PING] }, scaled05CombinedSwapChainWidth, scaled05CombinedSwapChainHeight);
		_csh = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });

		if (_texAtmoShadow)
			InitByAtmosphere(_texAtmoShadow);
	}
	renderer.GetDS().InitByBloom(_tex[TEX_PING]);
}

void Bloom::Generate()
{
	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	if (!settings._postProcessBloom)
		return;

	if (_editMode)
	{
		ImGui::DragFloat("Bloom colorScale", &_colorScale, 0.01f);
		ImGui::DragFloat("Bloom colorBias", &_colorBias, 0.01f);
		ImGui::DragFloat("Bloom (light shafts) maxDist", &_maxDist, 0.1f);
		ImGui::DragFloat("Bloom (light shafts) sunGloss", &_sunGloss, 0.1f);
		ImGui::DragFloat("Bloom (light shafts) wideStrength", &_wideStrength, 0.01f);
		ImGui::DragFloat("Bloom (light shafts) sunStrength", &_sunStrength, 0.01f);
		ImGui::Checkbox("Bloom blur", &_blur);
		ImGui::Checkbox("Bloom (light shafts) blur", &_blurLightShafts);
	}

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rph, _fbh, { _tex[TEX_PING]->GetClearValue() },
		CGI::ViewportScissorFlags::setAllForCurrentViewScaled | CGI::ViewportScissorFlags::applyHalfScale);

	s_ubBloomVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBloomVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubBloomVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubBloomFS._colorScale_colorBias_exposure.x = _colorScale;
	s_ubBloomFS._colorScale_colorBias_exposure.y = _colorBias;
	s_ubBloomFS._colorScale_colorBias_exposure.z = renderer.GetExposure();

	cb->BindPipeline(_pipe[PIPE_MAIN]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();

	if (_blur)
		Blur::I().GenerateForBloom(false);

	if (settings._postProcessLightShafts)
	{
		cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
		cb->BeginRenderPass(_rphLightShafts, _fbh, { _tex[TEX_PING]->GetClearValue() },
			CGI::ViewportScissorFlags::setAllForCurrentViewScaled | CGI::ViewportScissorFlags::applyHalfScale);

		s_ubBloomLightShaftsFS._matInvVP = Matrix4(VMath::inverse(sm.GetPassCamera()->GetMatrixVP())).UniformBufferFormat();
		s_ubBloomLightShaftsFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
		s_ubBloomLightShaftsFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
		s_ubBloomLightShaftsFS._eyePos = float4(sm.GetPassCamera()->GetEyePosition().GLM(), 0);
		s_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.x = _maxDist;
		s_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.y = _sunGloss;
		s_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.z = _wideStrength;
		s_ubBloomLightShaftsFS._maxDist_sunGloss_wideStrength_sunStrength.w = _sunStrength;
		s_ubBloomLightShaftsFS._matShadow = atmo.GetShadowMapBaker().GetShadowMatrix(0).UniformBufferFormat();
		s_ubBloomLightShaftsFS._matShadowCSM1 = atmo.GetShadowMapBaker().GetShadowMatrix(1).UniformBufferFormat();
		s_ubBloomLightShaftsFS._matShadowCSM2 = atmo.GetShadowMapBaker().GetShadowMatrix(2).UniformBufferFormat();
		s_ubBloomLightShaftsFS._matShadowCSM3 = atmo.GetShadowMapBaker().GetShadowMatrix(3).UniformBufferFormat();
		s_ubBloomLightShaftsFS._matScreenCSM = atmo.GetShadowMapBaker().GetScreenMatrixVP().UniformBufferFormat();
		s_ubBloomLightShaftsFS._csmSplitRanges = atmo.GetShadowMapBaker().GetSplitRanges().GLM();
		memcpy(&s_ubBloomLightShaftsFS._shadowConfig, &atmo.GetShadowMapBaker().GetConfig(), sizeof(s_ubBloomLightShaftsFS._shadowConfig));

		cb->BindPipeline(_pipe[PIPE_LIGHT_SHAFTS]);
		_shader->BeginBindDescriptors();
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _csh);
		cb->BindDescriptors(_shader, 2, _cshLightShafts);
		_shader->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
		cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

		if (_blurLightShafts)
			Blur::I().GenerateForBloom(true);
	}
}

CGI::TexturePtr Bloom::GetTexture() const
{
	return _tex[TEX_PING];
}

CGI::TexturePtr Bloom::GetPongTexture() const
{
	return _tex[TEX_PONG];
}
