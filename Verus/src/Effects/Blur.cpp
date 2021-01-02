// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Blur::UB_BlurVS      Blur::s_ubBlurVS;
Blur::UB_BlurFS      Blur::s_ubBlurFS;
Blur::UB_ExtraBlurFS Blur::s_ubExtraBlurFS;

// Blur::Handles:

void Blur::Handles::FreeDescriptorSets(CGI::ShaderPtr shader)
{
	shader->FreeDescriptorSet(_cshV);
	shader->FreeDescriptorSet(_cshU);
}

void Blur::Handles::DeleteFramebuffers()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbhV);
	renderer->DeleteFramebuffer(_fbhU);
}

// Blur:

Blur::Blur()
{
}

Blur::~Blur()
{
	Done();
}

void Blur::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_bloomHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_bloomHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_ssaoHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_ssaoHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::unormR8);
	_ssrHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	_ssrHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	_rphAntiAliasing = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	_rphMotionBlur = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);

	_shader.Init("[Shaders]:Blur.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubBlurVS, sizeof(s_ubBlurVS), 100, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBlurFS, sizeof(s_ubBlurFS), 100,
		{
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubExtraBlurFS, sizeof(s_ubExtraBlurFS), 100,
		{
			CGI::Sampler::linearClampMipN,
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#U", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#V";
		_pipe[PIPE_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#U", _bloomHandles._rphU);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_BLOOM_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#V";
		pipeDesc._renderPassHandle = _bloomHandles._rphV;
		_pipe[PIPE_BLOOM_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#USsao", _ssaoHandles._rphU);
		pipeDesc._colorAttachWriteMasks[0] = "a";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_SSAO_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VSsao";
		pipeDesc._colorAttachWriteMasks[0] = "rgba";
		pipeDesc._renderPassHandle = _ssaoHandles._rphV;
		_pipe[PIPE_SSAO_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#USsr", _ssrHandles._rphU);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_SSR_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VSsr";
		pipeDesc._renderPassHandle = _ssrHandles._rphV;
		_pipe[PIPE_SSR_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#AntiAliasing", _rphAntiAliasing);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_AA].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#Motion", _rphMotionBlur);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MOTION_BLUR].Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Blur::Done()
{
	_shader->FreeDescriptorSet(_cshMotionBlurExtra);
	_shader->FreeDescriptorSet(_cshMotionBlur);
	_shader->FreeDescriptorSet(_cshAntiAliasingExtra);
	_shader->FreeDescriptorSet(_cshAntiAliasing);
	_ssaoHandles.FreeDescriptorSets(_shader);
	_bloomHandles.FreeDescriptorSets(_shader);
	VERUS_DONE(Blur);
}

void Blur::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_BLOOM;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SSAO;

	{
		_shader->FreeDescriptorSet(_cshMotionBlurExtra);
		_shader->FreeDescriptorSet(_cshMotionBlur);
		renderer->DeleteFramebuffer(_fbhMotionBlur);

		_shader->FreeDescriptorSet(_cshAntiAliasingExtra);
		_shader->FreeDescriptorSet(_cshAntiAliasing);
		renderer->DeleteFramebuffer(_fbhAntiAliasing);

		_shader->FreeDescriptorSet(_cshSsrExtra);
		_ssrHandles.FreeDescriptorSets(_shader);
		_ssrHandles.DeleteFramebuffers();

		_ssaoHandles.FreeDescriptorSets(_shader);
		_ssaoHandles.DeleteFramebuffers();

		_bloomHandles.FreeDescriptorSets(_shader);
		_bloomHandles.DeleteFramebuffers();

		_tex.Done();
	}
	{
		const int swapChainWidth = renderer.GetSwapChainWidth();
		const int swapChainHeight = renderer.GetSwapChainHeight();
		const int swapChainHalfWidth = renderer.GetSwapChainWidth() / 2;
		const int swapChainHalfHeight = renderer.GetSwapChainHeight() / 2;

		CGI::TextureDesc texDesc;
		texDesc._name = "Blur.Tex";
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = swapChainWidth;
		texDesc._height = swapChainHeight;
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex.Init(texDesc);

		_bloomHandles._fbhU = renderer->CreateFramebuffer(_bloomHandles._rphU, { bloom.GetPongTexture() }, swapChainHalfWidth, swapChainHalfHeight);
		_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, swapChainHalfWidth, swapChainHalfHeight);
		_bloomHandles._cshU = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
		_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });

		_ssaoHandles._fbhU = renderer->CreateFramebuffer(_ssaoHandles._rphU, { _tex }, swapChainWidth, swapChainHeight);
		_ssaoHandles._fbhV = renderer->CreateFramebuffer(_ssaoHandles._rphV, { ssao.GetTexture() }, swapChainWidth, swapChainHeight);
		_ssaoHandles._cshU = _shader->BindDescriptorSetTextures(1, { ssao.GetTexture() });
		_ssaoHandles._cshV = _shader->BindDescriptorSetTextures(1, { _tex });

		_ssrHandles._fbhU = renderer->CreateFramebuffer(_ssrHandles._rphU, { renderer.GetDS().GetLightAccDiffTexture() }, swapChainWidth, swapChainHeight);
		_ssrHandles._fbhV = renderer->CreateFramebuffer(_ssrHandles._rphV, { renderer.GetDS().GetLightAccSpecTexture() }, swapChainWidth, swapChainHeight);
		_ssrHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetLightAccSpecTexture() });
		_ssrHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetLightAccDiffTexture() });
		_cshSsrExtra = _shader->BindDescriptorSetTextures(2, { ssao.GetTexture(), renderer.GetDS().GetGBuffer(2) });

		_fbhAntiAliasing = renderer->CreateFramebuffer(_rphAntiAliasing, { renderer.GetDS().GetComposedTextureB() }, swapChainWidth, swapChainHeight);
		_cshAntiAliasing = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
		_cshAntiAliasingExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });

		_fbhMotionBlur = renderer->CreateFramebuffer(_rphMotionBlur, { renderer.GetDS().GetComposedTextureA() }, swapChainWidth, swapChainHeight);
		_cshMotionBlur = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
		_cshMotionBlurExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
	}
}

void Blur::Generate()
{
}

void Blur::GenerateForBloom(bool forGodRays)
{
	VERUS_QREF_BLOOM;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (bloom.IsEditMode())
	{
		ImGui::DragFloat("Bloom blur radius", &_bloomRadius, 0.001f);
		ImGui::DragFloat("Bloom (god rays) blur radius", &_bloomGodRaysRadius, 0.001f);
	}

	const float radius = forGodRays ? _bloomGodRaysRadius : _bloomRadius;
	float samplesPerPixel = 1;
	int maxSamples = 1;
	switch (settings._gpuShaderQuality)
	{
	case App::Settings::Quality::low:
		samplesPerPixel = 0.125f;
		maxSamples = 8 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 0.25f;
		maxSamples = 16 - 1;
		break;
	case App::Settings::Quality::high:
		samplesPerPixel = 0.5f;
		maxSamples = 32 - 1;
		break;
	case App::Settings::Quality::ultra:
		samplesPerPixel = 0.5f;
		maxSamples = 64 - 1;
		break;
	}

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize()).UniformBufferFormat();
	UpdateUniformBuffer(radius, 0, renderer.GetSwapChainWidth(), samplesPerPixel, maxSamples);

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_bloomHandles._rphU, _bloomHandles._fbhU, { bloom.GetPongTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_BLOOM_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshU);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize()).UniformBufferFormat();
	UpdateUniformBuffer(radius * renderer.GetSwapChainAspectRatio(), 0, renderer.GetSwapChainHeight(), samplesPerPixel, maxSamples);
	{
		cb->BeginRenderPass(_bloomHandles._rphV, _bloomHandles._fbhV, { bloom.GetTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_BLOOM_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshV);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForSsao()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SSAO;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, _tex->GetSize(), nullptr, 0.5f, 0).UniformBufferFormat();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_ssaoHandles._rphU, _ssaoHandles._fbhU, { _tex->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssaoHandles._cshU);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, _tex->GetSize(), nullptr, 0, 0.5f).UniformBufferFormat();
	{
		cb->BeginRenderPass(_ssaoHandles._rphV, _ssaoHandles._fbhV, { ssao.GetTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssaoHandles._cshV);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForSsr()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SSR;

	if (ssr.IsEditMode())
	{
		ImGui::DragFloat("SSR blur radius", &_ssrRadius, 0.001f);
	}

	float samplesPerPixel = 1;
	int maxSamples = 1;
	switch (settings._gpuShaderQuality)
	{
	case App::Settings::Quality::low:
		samplesPerPixel = 0.25f;
		maxSamples = 8 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 0.5f;
		maxSamples = 16 - 1;
		break;
	case App::Settings::Quality::high:
		samplesPerPixel = 1;
		maxSamples = 32 - 1;
		break;
	case App::Settings::Quality::ultra:
		samplesPerPixel = 1;
		maxSamples = 64 - 1;
		break;
	}

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, renderer.GetDS().GetLightAccDiffTexture()->GetSize()).UniformBufferFormat();
	UpdateUniformBuffer(_ssrRadius, 0, renderer.GetSwapChainWidth(), samplesPerPixel, maxSamples);

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_ssrHandles._rphU, _ssrHandles._fbhU, { renderer.GetDS().GetLightAccDiffTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSR_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssrHandles._cshU);
		cb->BindDescriptors(_shader, 2, _cshSsrExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, renderer.GetDS().GetLightAccSpecTexture()->GetSize()).UniformBufferFormat();
	UpdateUniformBuffer(_ssrRadius * renderer.GetSwapChainAspectRatio(), 0, renderer.GetSwapChainHeight(), samplesPerPixel, maxSamples);
	{
		cb->BeginRenderPass(_ssrHandles._rphV, _ssrHandles._fbhV, { renderer.GetDS().GetLightAccSpecTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSR_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssrHandles._cshV);
		cb->BindDescriptors(_shader, 2, _cshSsrExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForAntiAliasing()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubExtraBlurFS._zNearFarEx = sm.GetMainCamera()->GetZNearFarEx().GLM();
	s_ubExtraBlurFS._textureSize = renderer.GetDS().GetComposedTextureA()->GetSize().GLM();

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rphAntiAliasing, _fbhAntiAliasing, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

	cb->BindPipeline(_pipe[PIPE_AA]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshAntiAliasing);
	cb->BindDescriptors(_shader, 2, _cshAntiAliasingExtra);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);
}

void Blur::GenerateForMotionBlur()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubExtraBlurFS._matInvVP = Matrix4(VMath::inverse(sm.GetMainCamera()->GetMatrixVP())).UniformBufferFormat();
	s_ubExtraBlurFS._matPrevVP = sm.GetMainCamera()->GetMatrixPrevVP().UniformBufferFormat();
	s_ubExtraBlurFS._zNearFarEx = sm.GetMainCamera()->GetZNearFarEx().GLM();

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rphMotionBlur, _fbhMotionBlur, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

	cb->BindPipeline(_pipe[PIPE_MOTION_BLUR]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshMotionBlur);
	cb->BindDescriptors(_shader, 2, _cshMotionBlurExtra);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);
}

void Blur::UpdateUniformBuffer(float radius, int sampleCount, int texSize, float samplesPerPixel, int maxSamples)
{
	if (!sampleCount)
	{
		sampleCount = static_cast<int>(texSize * (radius * 2) * samplesPerPixel);
		sampleCount = Math::Clamp(sampleCount, 3, maxSamples);
	}
	s_ubBlurFS._radius_invRadius_stride.x = radius;
	s_ubBlurFS._radius_invRadius_stride.y = 1 / radius;
	s_ubBlurFS._radius_invRadius_stride.z = radius * 2 / (sampleCount - 1);
	s_ubBlurFS._sampleCount = sampleCount;
}
