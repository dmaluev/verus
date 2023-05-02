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
	{
		_rphVsmU = renderer->CreateSimpleRenderPass(CGI::Format::floatR16G16);
		_rphVsmV = renderer->CreateSimpleRenderPass(CGI::Format::floatR16G16, CGI::RP::Attachment::LoadOp::load);
	}
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
	_shader->CreateDescriptorSet(0, &s_ubBlurVS, sizeof(s_ubBlurVS), 64, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBlurFS, sizeof(s_ubBlurFS), 64,
		{
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubExtraBlurFS, sizeof(s_ubExtraBlurFS), 32,
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
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#UVsm", _rphVsmU);
		pipeDesc._colorAttachWriteMasks[0] = "rg";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_VSM_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VVsm";
		pipeDesc._renderPassHandle = _rphVsmV;
		_pipe[PIPE_VSM_V].Init(pipeDesc);
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

		pipeDesc._shaderBranch = "#UBox";
		pipeDesc._renderPassHandle = _bloomHandles._rphU;
		_pipe[PIPE_BLOOM_LIGHT_SHAFTS_U].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VBox";
		pipeDesc._renderPassHandle = _bloomHandles._rphV;
		_pipe[PIPE_BLOOM_LIGHT_SHAFTS_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#AntiAliasing", _rphAntiAliasing);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_AA].Init(pipeDesc);
		pipeDesc._shaderBranch = "#AntiAliasingOff";
		_pipe[PIPE_AA_OFF].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#Motion", _rphMotionBlur);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_MOTION_BLUR].Init(pipeDesc);
		pipeDesc._shaderBranch = "#MotionOff";
		_pipe[PIPE_MOTION_BLUR_OFF].Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Blur::Done()
{
	if (CGI::Renderer::IsLoaded())
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
	}
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
		const int scaledCombinedSwapChainWidth = settings.Scale(renderer.GetCombinedSwapChainWidth());
		const int scaledCombinedSwapChainHeight = settings.Scale(renderer.GetCombinedSwapChainHeight());
		const int scaled05CombinedSwapChainWidth = settings.Scale(renderer.GetCombinedSwapChainWidth(), 0.5f);
		const int scaled05CombinedSwapChainHeight = settings.Scale(renderer.GetCombinedSwapChainHeight(), 0.5f);

		if (settings._postProcessSSAO)
		{
			// GBuffer3.r >> GBuffer2.r:
			_fbhSsao = renderer->CreateFramebuffer(_rphSsao, { renderer.GetDS().GetGBuffer(2) }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_cshSsao = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetGBuffer(3) });
		}

		{
			// ComposedA >> ComposedB >> ComposedA:
			_rdsHandles._fbhU = renderer->CreateFramebuffer(_rdsHandles._rphU, { renderer.GetDS().GetComposedTextureB() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_rdsHandles._fbhV = renderer->CreateFramebuffer(_rdsHandles._rphV, { renderer.GetDS().GetComposedTextureA() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_rdsHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_rdsHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshRdsExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(3), renderer.GetDS().GetGBuffer(3) });
		}

		{
			// ComposedA >> ComposedB >> ComposedA:
			_dofHandles._fbhU = renderer->CreateFramebuffer(_dofHandles._rphU, { renderer.GetDS().GetComposedTextureB() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_dofHandles._fbhV = renderer->CreateFramebuffer(_dofHandles._rphV, { renderer.GetDS().GetComposedTextureA() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_dofHandles._cshU = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_dofHandles._cshV = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshDofExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		if (settings._postProcessBloom)
		{
			// Ping >> Pong >> Ping:
			_bloomHandles._fbhU = renderer->CreateFramebuffer(_bloomHandles._rphU, { bloom.GetPongTexture() }, scaled05CombinedSwapChainWidth, scaled05CombinedSwapChainHeight);
			_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, scaled05CombinedSwapChainWidth, scaled05CombinedSwapChainHeight);
			_bloomHandles._cshU = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
			_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });
		}

		{
			// ComposedA >> ComposedB:
			_fbhAntiAliasing = renderer->CreateFramebuffer(_rphAntiAliasing, { renderer.GetDS().GetComposedTextureB() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_cshAntiAliasing = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
			_cshAntiAliasingExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}

		{
			// ComposedB >> ComposedA:
			_fbhMotionBlur = renderer->CreateFramebuffer(_rphMotionBlur, { renderer.GetDS().GetComposedTextureA() }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
			_cshMotionBlur = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureB() });
			_cshMotionBlurExtra = _shader->BindDescriptorSetTextures(2, { renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
		}
	}
}

void Blur::GenerateForVSM(CGI::FBHandle fbhU, CGI::FBHandle fbhV, CGI::CSHandle cshU, CGI::CSHandle cshV,
	RcVector4 rc, RcVector4 zNearFarEx)
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForVSM");

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_rphVsmU, fbhU, { Vector4(1) }, CGI::ViewportScissorFlags::setAllForFramebuffer);

		s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubExtraBlurFS._zNearFarEx = zNearFarEx.GLM();

		cb->BindPipeline(_pipe[PIPE_VSM_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, cshU);
		cb->BindDescriptors(_shader, 2, _cshRdsExtra); // Dummy CSHandle.
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	{
		cb->BeginRenderPass(_rphVsmV, fbhV, { Vector4(1) }, CGI::ViewportScissorFlags::none);

		cb->SetViewport({ rc });
		cb->SetScissor({ rc });

		cb->BindPipeline(_pipe[PIPE_VSM_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, cshV);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Blur::GenerateForSsao()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForSsao");

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_rphSsao, _fbhSsao, { renderer.GetDS().GetGBuffer(2)->GetClearValue() });

		s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubBlurVS._matV = Math::ToUVMatrix(0, cb->GetViewportSize(), nullptr, 0.5f, 0.5f).UniformBufferFormat();
		s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();

		cb->BindPipeline(_pipe[PIPE_SSAO]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _cshSsao);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Blur::GenerateForResolveDitheringAndSharpen()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForResolveDitheringAndSharpen");

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_rdsHandles._rphU, _rdsHandles._fbhU, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

		s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();

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

	VERUS_PROFILER_END_EVENT(cb);
}

void Blur::GenerateForDepthOfField()
{
	if (!_enableDepthOfField)
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

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

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForDepthOfField");

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_dofHandles._rphU, _dofHandles._fbhU, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

		s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		UpdateUniformBuffer(_dofRadius, 0, renderer.GetCurrentViewWidth(), samplesPerPixel, maxSamples);
		s_ubExtraBlurFS._zNearFarEx = wm.GetPassCamera()->GetZNearFarEx().GLM();
		s_ubExtraBlurFS._textureSize = cb->GetViewportSize().GLM();
		s_ubExtraBlurFS._focusDist.x = _dofFocusDist;
		s_ubExtraBlurFS._blurDir = float4(
			dirs[0].getX(), dirs[0].getY() * renderer.GetCurrentViewAspectRatio(),
			dirs[1].getX(), dirs[1].getY() * renderer.GetCurrentViewAspectRatio());

		cb->BindPipeline(_pipe[PIPE_DOF_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _dofHandles._cshU);
		cb->BindDescriptors(_shader, 2, _cshDofExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	{
		cb->BeginRenderPass(_dofHandles._rphV, _dofHandles._fbhV, { renderer.GetDS().GetComposedTextureA()->GetClearValue() });

		UpdateUniformBuffer(_dofRadius * 0.5f, 0, renderer.GetCurrentViewHeight(), samplesPerPixel, maxSamples);
		s_ubExtraBlurFS._blurDir = float4(dirs[2].getX(), dirs[2].getY() * renderer.GetCurrentViewAspectRatio(), 0, 0);

		cb->BindPipeline(_pipe[PIPE_DOF_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _dofHandles._cshV);
		cb->BindDescriptors(_shader, 2, _cshDofExtra);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

	VERUS_PROFILER_END_EVENT(cb);
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
		samplesPerPixel = 1 / 5.f;
		maxSamples = 12 - 1;
		break;
	case App::Settings::Quality::medium:
		samplesPerPixel = 1 / 5.f;
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
	if (forLightShafts)
		samplesPerPixel = 1;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForBloom");

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_bloomHandles._rphU, _bloomHandles._fbhU, { bloom.GetPongTexture()->GetClearValue() },
			CGI::ViewportScissorFlags::setAllForCurrentViewScaled | CGI::ViewportScissorFlags::applyHalfScale);

		s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
		UpdateUniformBuffer(radius, 0, renderer.GetCurrentViewWidth() / 2, samplesPerPixel, maxSamples);

		cb->BindPipeline(_pipe[forLightShafts ? PIPE_BLOOM_LIGHT_SHAFTS_U : PIPE_BLOOM_U]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshU);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	{
		cb->BeginRenderPass(_bloomHandles._rphV, _bloomHandles._fbhV, { bloom.GetTexture()->GetClearValue() },
			CGI::ViewportScissorFlags::setAllForCurrentViewScaled | CGI::ViewportScissorFlags::applyHalfScale);

		UpdateUniformBuffer(radius * renderer.GetCurrentViewAspectRatio(), 0, renderer.GetCurrentViewHeight() / 2, samplesPerPixel, maxSamples);

		cb->BindPipeline(_pipe[forLightShafts ? PIPE_BLOOM_LIGHT_SHAFTS_V : PIPE_BLOOM_V]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshV);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Blur::GenerateForAntiAliasing()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForAntiAliasing");

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rphAntiAliasing, _fbhAntiAliasing, { renderer.GetDS().GetComposedTextureB()->GetClearValue() });

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubExtraBlurFS._zNearFarEx = wm.GetPassCamera()->GetZNearFarEx().GLM();
	s_ubExtraBlurFS._textureSize = cb->GetViewportSize().GLM();

	cb->BindPipeline(_pipe[settings._postProcessAntiAliasing ? PIPE_AA : PIPE_AA_OFF]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshAntiAliasing);
	cb->BindDescriptors(_shader, 2, _cshAntiAliasingExtra);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

	VERUS_PROFILER_END_EVENT(cb);
}

void Blur::GenerateForMotionBlur()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 0, 64, 255), "Blur/GenerateForMotionBlur");

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rphMotionBlur, _fbhMotionBlur, { renderer.GetDS().GetComposedTextureA()->GetClearValue() });

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubBlurVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubBlurFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubExtraBlurFS._matInvVP = Matrix4(VMath::inverse(wm.GetViewCamera()->GetMatrixVP())).UniformBufferFormat();
	s_ubExtraBlurFS._matPrevVP = wm.GetViewCamera()->GetMatrixPrevVP().UniformBufferFormat();
	s_ubExtraBlurFS._zNearFarEx = wm.GetViewCamera()->GetZNearFarEx().GLM();

	cb->BindPipeline(_pipe[settings._postProcessMotionBlur ? PIPE_MOTION_BLUR : PIPE_MOTION_BLUR_OFF]);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshMotionBlur);
	cb->BindDescriptors(_shader, 2, _cshMotionBlurExtra);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

	VERUS_PROFILER_END_EVENT(cb);
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
