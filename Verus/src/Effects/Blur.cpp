// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	if (settings._postProcessSSAO)
	{
		_rphSsao = renderer->CreateSimpleRenderPass(CGI::Format::unormR8G8B8A8); // >> GBuffer2.r
	}
	else
	{
		vIgnoreList.push_back("#Ssao");
	}
	{
		_rdsHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
		_rdsHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	}
	{
		_dofHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
		_dofHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::floatR11G11B10);
	}
	if (settings._postProcessBloom)
	{
		_bloomHandles._rphU = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
		_bloomHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
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
	if (settings._postProcessSSAO)
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#Ssao", _rphSsao);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_MIN;
		pipeDesc._colorAttachWriteMasks[0] = "r"; // >> GBuffer2.r
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_SSAO].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#ResolveDithering", _rdsHandles._rphU);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_RESOLVE_DITHERING].Init(pipeDesc);
		pipeDesc._shaderBranch = "#Sharpen";
		pipeDesc._renderPassHandle = _rdsHandles._rphV;
		_pipe[PIPE_SHARPEN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#UDoF", _dofHandles._rphU);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_DOF_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VDoF";
		pipeDesc._renderPassHandle = _dofHandles._rphV;
		_pipe[PIPE_DOF_V].Init(pipeDesc);
	}
	if (settings._postProcessBloom)
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#U", _bloomHandles._rphU);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_BLOOM_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#V";
		pipeDesc._renderPassHandle = _bloomHandles._rphV;
		_pipe[PIPE_BLOOM_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#AntiAliasing", _rphAntiAliasing);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_AA].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#Motion", _rphMotionBlur);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
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

	_bloomHandles.DeleteFramebuffers();
	_bloomHandles.DeleteRenderPasses();

	_dofHandles.DeleteFramebuffers();
	_dofHandles.DeleteRenderPasses();

	_rdsHandles.DeleteFramebuffers();
	_rdsHandles.DeleteRenderPasses();

	renderer->DeleteFramebuffer(_fbhSsao);
	renderer->DeleteRenderPass(_rphSsao);

	VERUS_DONE(Blur);
}

void Blur::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_BLOOM;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	{
		_shader->FreeDescriptorSet(_cshMotionBlurExtra);
		_shader->FreeDescriptorSet(_cshMotionBlur);
		renderer->DeleteFramebuffer(_fbhMotionBlur);

		_shader->FreeDescriptorSet(_cshAntiAliasingExtra);
		_shader->FreeDescriptorSet(_cshAntiAliasing);
		renderer->DeleteFramebuffer(_fbhAntiAliasing);

		_bloomHandles.FreeDescriptorSets(_shader);
		_bloomHandles.DeleteFramebuffers();

		_shader->FreeDescriptorSet(_cshDofExtra);
		_dofHandles.FreeDescriptorSets(_shader);
		_dofHandles.DeleteFramebuffers();

		_shader->FreeDescriptorSet(_cshRdsExtra);
		_rdsHandles.FreeDescriptorSets(_shader);
		_rdsHandles.DeleteFramebuffers();

		_shader->FreeDescriptorSet(_cshSsao);
		renderer->DeleteFramebuffer(_fbhSsao);
	}
	{
		const int scaledSwapChainWidth = settings.Scale(renderer.GetSwapChainWidth());
		const int scaledSwapChainHeight = settings.Scale(renderer.GetSwapChainHeight());
		const int scaledSwapChainHalfWidth = scaledSwapChainWidth / 2;
		const int scaledSwapChainHalfHeight = scaledSwapChainHeight / 2;

		if (settings._postProcessSSAO)
		{
			// GBuffer3.r >> GBuffer2.r:
			_fbhSsao = renderer->CreateFramebuffer(_rphSsao, { renderer.GetDS().GetGBuffer(2) }, scaledSwapChainWidth, scaledSwapChainHeight);
			_cshSsao = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetGBuffer(3) });
		}

		{
			// ComposedA >> ComposedB >> ComposedA:
			_rdsHandles._fbhU = renderer->CreateFramebuffer(_rdsHandles._rphU, { renderer.GetDS().GetComposedTextureB() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_rdsHandles._fbhV = renderer->CreateFramebuffer(_rdsHandles._rphV, { renderer.GetDS().GetComposedTextureA() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_rdsHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_rdsHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshRdsExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(3), renderer.GetDS().GetGBuffer(3) });
		}

		{
			// ComposedA >> ComposedB >> ComposedA:
			_dofHandles._fbhU = renderer->CreateFramebuffer(_dofHandles._rphU, { renderer.GetDS().GetComposedTextureB() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_dofHandles._fbhV = renderer->CreateFramebuffer(_dofHandles._rphV, { renderer.GetDS().GetComposedTextureA() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_dofHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_dofHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshDofExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		if (settings._postProcessBloom)
		{
			// Ping >> Pong >> Ping:
			_bloomHandles._fbhU = renderer->CreateFramebuffer(_bloomHandles._rphU, { bloom.GetPongTexture() }, scaledSwapChainHalfWidth, scaledSwapChainHalfHeight);
			_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, scaledSwapChainHalfWidth, scaledSwapChainHalfHeight);
			_bloomHandles._cshU = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
			_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });
		}

		{
			// ComposedA >> ComposedB:
			_fbhAntiAliasing = renderer->CreateFramebuffer(_rphAntiAliasing, { renderer.GetDS().GetComposedTextureB() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_cshAntiAliasing = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_cshAntiAliasingExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		{
			// ComposedB >> ComposedA:
			_fbhMotionBlur = renderer->CreateFramebuffer(_rphMotionBlur, { renderer.GetDS().GetComposedTextureA() }, scaledSwapChainWidth, scaledSwapChainHeight);
			_cshMotionBlur = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshMotionBlurExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}
	}
}

void Blur::GenerateForSsao()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, renderer.GetDS().GetGBuffer(2)->GetSize(), nullptr, 0.5f, 0.5f).UniformBufferFormat();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_rphSsao, _fbhSsao, { renderer.GetDS().GetGBuffer(2)->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _cshSsao);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForResolveDitheringAndSharpen()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_rdsHandles._rphU, _rdsHandles._fbhU, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_RESOLVE_DITHERING]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _rdsHandles._cshU);
		cb->BindDescriptors(_shader, 2, _cshRdsExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	{
		cb->BeginRenderPass(_rdsHandles._rphV, _rdsHandles._fbhV, { renderer.GetDS().GetComposedTextureA()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SHARPEN]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _rdsHandles._cshV);
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
