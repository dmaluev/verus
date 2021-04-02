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

void Blur::Handles::DeleteRenderPasses()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteRenderPass(_rphV);
	renderer->DeleteRenderPass(_rphU);
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
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	Vector<CSZ> vIgnoreList;
	if (settings._postProcessBloom)
	{
		_bloomHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
		_bloomHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	}
	{
		_dofHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
		_dofHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	}
	if (settings._postProcessSSAO)
	{
		_ssaoHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
		_ssaoHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::unormR8);
	}
	else
	{
		vIgnoreList.push_back("#USsao");
		vIgnoreList.push_back("#VSsao");
	}
	{
		_ssrHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
		_ssrHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	}
	_rphAntiAliasing = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	_rphMotionBlur = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);

	CGI::ShaderDesc shaderDesc("[Shaders]:Blur.hlsl");
	if (!vIgnoreList.empty())
	{
		vIgnoreList.push_back(nullptr);
		shaderDesc._ignoreList = vIgnoreList.data();
	}
	_shader.Init(shaderDesc);
	_shader->CreateDescriptorSet(0, &s_ubBlurVS, sizeof(s_ubBlurVS), 20, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBlurFS, sizeof(s_ubBlurFS), 20,
		{
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubExtraBlurFS, sizeof(s_ubExtraBlurFS), 10,
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
	if (settings._postProcessBloom)
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
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#UDoF", _dofHandles._rphU);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_DOF_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VDoF";
		pipeDesc._renderPassHandle = _dofHandles._rphV;
		_pipe[PIPE_DOF_V].Init(pipeDesc);
	}
	if (settings._postProcessSSAO)
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
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbhMotionBlur);
	renderer->DeleteRenderPass(_rphMotionBlur);
	renderer->DeleteFramebuffer(_fbhAntiAliasing);
	renderer->DeleteRenderPass(_rphAntiAliasing);
	_ssrHandles.DeleteFramebuffers();
	_ssrHandles.DeleteRenderPasses();
	_ssaoHandles.DeleteFramebuffers();
	_ssaoHandles.DeleteRenderPasses();
	_dofHandles.DeleteFramebuffers();
	_dofHandles.DeleteRenderPasses();
	_bloomHandles.DeleteFramebuffers();
	_bloomHandles.DeleteRenderPasses();
	VERUS_DONE(Blur);
}

void Blur::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_BLOOM;
	VERUS_QREF_CONST_SETTINGS;
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

		_shader->FreeDescriptorSet(_cshDofExtra);
		_dofHandles.FreeDescriptorSets(_shader);
		_dofHandles.DeleteFramebuffers();

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

		if (settings._postProcessBloom)
		{
			_bloomHandles._fbhU = renderer->CreateFramebuffer(_bloomHandles._rphU, { bloom.GetPongTexture() }, swapChainHalfWidth, swapChainHalfHeight);
			_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, swapChainHalfWidth, swapChainHalfHeight);
			_bloomHandles._cshU = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
			_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });
		}

		{
			_dofHandles._fbhU = renderer->CreateFramebuffer(_dofHandles._rphU, { renderer.GetDS().GetComposedTextureB() }, swapChainWidth, swapChainHeight);
			_dofHandles._fbhV = renderer->CreateFramebuffer(_dofHandles._rphV, { renderer.GetDS().GetComposedTextureA() }, swapChainWidth, swapChainHeight);
			_dofHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_dofHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshDofExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		if (settings._postProcessSSAO)
		{
			_ssaoHandles._fbhU = renderer->CreateFramebuffer(_ssaoHandles._rphU, { _tex }, swapChainWidth, swapChainHeight);
			_ssaoHandles._fbhV = renderer->CreateFramebuffer(_ssaoHandles._rphV, { ssao.GetTexture() }, swapChainWidth, swapChainHeight);
			_ssaoHandles._cshU = _shader->BindDescriptorSetTextures(1, { ssao.GetTexture() });
			_ssaoHandles._cshV = _shader->BindDescriptorSetTextures(1, { _tex });
		}

		{
			_ssrHandles._fbhU = renderer->CreateFramebuffer(_ssrHandles._rphU, { renderer.GetDS().GetLightAccDiffTexture() }, swapChainWidth, swapChainHeight);
			_ssrHandles._fbhV = renderer->CreateFramebuffer(_ssrHandles._rphV, { renderer.GetDS().GetLightAccSpecTexture() }, swapChainWidth, swapChainHeight);
			_ssrHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetLightAccSpecTexture() });
			_ssrHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetLightAccDiffTexture() });
			_cshSsrExtra = _shader->BindDescriptorSetTextures(2, { ssao.GetTexture(), renderer.GetDS().GetGBuffer(2) });
		}

		{
			_fbhAntiAliasing = renderer->CreateFramebuffer(_rphAntiAliasing, { renderer.GetDS().GetComposedTextureB() }, swapChainWidth, swapChainHeight);
			_cshAntiAliasing = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_cshAntiAliasingExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		{
			_fbhMotionBlur = renderer->CreateFramebuffer(_rphMotionBlur, { renderer.GetDS().GetComposedTextureA() }, swapChainWidth, swapChainHeight);
			_cshMotionBlur = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshMotionBlurExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}
	}
}

void Blur::Generate()
{
}

void Blur::GenerateForBloom(bool forLightShafts)
{
	VERUS_QREF_BLOOM;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (bloom.IsEditMode())
	{
		ImGui::DragFloat("Bloom blur radius", &_bloomRadius, 0.001f);
		ImGui::DragFloat("Bloom (god rays) blur radius", &_bloomLightShaftsRadius, 0.001f);
	}

	const float radius = forLightShafts ? _bloomLightShaftsRadius : _bloomRadius;
	float samplesPerPixel = 1;
	int maxSamples = 1;
	switch (settings._gpuShaderQuality)
	{
	case App::Settings::Quality::low:
		samplesPerPixel = 1 / 6.f;
		maxSamples = 12 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 1 / 6.f;
		maxSamples = 16 - 1;
		break;
	case App::Settings::Quality::high:
		samplesPerPixel = 1 / 4.f;
		maxSamples = 24 - 1;
		break;
	case App::Settings::Quality::ultra:
		samplesPerPixel = 1 / 4.f;
		maxSamples = 32 - 1;
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

void Blur::GenerateForDepthOfField()
{
	if (!_enableDepthOfField)
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	const float radius = 0.02f;
	const Matrix3 matR = Matrix3::rotationZ(Math::ToRadians(45));
	const Vector3 dirs[] =
	{
		matR * Vector3(0.866f, 0, 0),
		matR * Vector3(0, 0.5f, 0),
		matR * Vector3(0, 1, 0)
	};

	float samplesPerPixel = 1;
	int maxSamples = 1;
	switch (settings._gpuShaderQuality)
	{
	case App::Settings::Quality::low:
		samplesPerPixel = 1 / 6.f;
		maxSamples = 12 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 1 / 6.f;
		maxSamples = 16 - 1;
		break;
	case App::Settings::Quality::high:
		samplesPerPixel = 1 / 3.f;
		maxSamples = 24 - 1;
		break;
	case App::Settings::Quality::ultra:
		samplesPerPixel = 1 / 3.f;
		maxSamples = 32 - 1;
		break;
	}

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubExtraBlurFS._zNearFarEx = sm.GetMainCamera()->GetZNearFarEx().GLM();
	s_ubExtraBlurFS._textureSize = renderer.GetDS().GetComposedTextureA()->GetSize().GLM();
	s_ubExtraBlurFS._focusDist_blurStrength.x = _dofFocusDist;
	s_ubExtraBlurFS._focusDist_blurStrength.y = _dofBlurStrength;

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	_shader->BeginBindDescriptors();
	UpdateUniformBuffer(radius, 0, renderer.GetSwapChainWidth(), samplesPerPixel, maxSamples);
	s_ubExtraBlurFS._blurDir = float4(
		dirs[0].getX(), dirs[0].getY() * renderer.GetSwapChainAspectRatio(),
		dirs[1].getX(), dirs[1].getY() * renderer.GetSwapChainAspectRatio());
	{
		cb->BeginRenderPass(_dofHandles._rphU, _dofHandles._fbhU, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_DOF_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _dofHandles._cshU);
		cb->BindDescriptors(_shader, 2, _cshDofExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	UpdateUniformBuffer(radius * 0.5f, 0, renderer.GetSwapChainWidth(), samplesPerPixel, maxSamples);
	s_ubExtraBlurFS._blurDir = float4(dirs[2].getX(), dirs[2].getY() * renderer.GetSwapChainAspectRatio(), 0, 0);
	{
		cb->BeginRenderPass(_dofHandles._rphV, _dofHandles._fbhV, { renderer.GetDS().GetComposedTextureA()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_DOF_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _dofHandles._cshV);
		cb->BindDescriptors(_shader, 2, _cshDofExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);
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
		samplesPerPixel = 1 / 6.f;
		maxSamples = 12 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 1 / 6.f;
		maxSamples = 16 - 1;
		break;
	case App::Settings::Quality::high:
		samplesPerPixel = 1 / 4.f;
		maxSamples = 24 - 1;
		break;
	case App::Settings::Quality::ultra:
		samplesPerPixel = 1 / 4.f;
		maxSamples = 32 - 1;
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
