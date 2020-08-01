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
	shader->FreeDescriptorSet(_cshH);
}

void Blur::Handles::DeleteFramebuffers()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbhV);
	renderer->DeleteFramebuffer(_fbhH);
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

	_bloomHandles._rphH = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_bloomHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_ssaoHandles._rphH = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);
	_ssaoHandles._rphV = renderer->CreateSimpleRenderPass(CGI::Format::unormR8);
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
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#H", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_H].Init(pipeDesc);
		pipeDesc._shaderBranch = "#V";
		_pipe[PIPE_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#H", _bloomHandles._rphH);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_BLOOM_H].Init(pipeDesc);
		pipeDesc._shaderBranch = "#V";
		pipeDesc._renderPassHandle = _bloomHandles._rphV;
		_pipe[PIPE_BLOOM_V].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#HSsao", _ssaoHandles._rphH);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_SSAO_H].Init(pipeDesc);
		pipeDesc._shaderBranch = "#VSsao";
		pipeDesc._renderPassHandle = _ssaoHandles._rphV;
		_pipe[PIPE_SSAO_V].Init(pipeDesc);
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
	VERUS_QREF_RENDERER;
	VERUS_QREF_BLOOM;
	VERUS_QREF_SSAO;
	{
		_shader->FreeDescriptorSet(_cshMotionBlurExtra);
		_shader->FreeDescriptorSet(_cshMotionBlur);
		renderer->DeleteFramebuffer(_fbhMotionBlur);
		_shader->FreeDescriptorSet(_cshAntiAliasingExtra);
		_shader->FreeDescriptorSet(_cshAntiAliasing);
		renderer->DeleteFramebuffer(_fbhAntiAliasing);
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
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = swapChainWidth;
		texDesc._height = swapChainHeight;
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex.Init(texDesc);

		_bloomHandles._fbhH = renderer->CreateFramebuffer(_bloomHandles._rphH, { bloom.GetPongTexture() }, swapChainHalfWidth, swapChainHalfHeight);
		_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, swapChainHalfWidth, swapChainHalfHeight);
		_bloomHandles._cshH = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
		_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });

		_ssaoHandles._fbhH = renderer->CreateFramebuffer(_ssaoHandles._rphH, { _tex }, swapChainWidth, swapChainHeight);
		_ssaoHandles._fbhV = renderer->CreateFramebuffer(_ssaoHandles._rphV, { ssao.GetTexture() }, swapChainWidth, swapChainHeight);
		_ssaoHandles._cshH = _shader->BindDescriptorSetTextures(1, { ssao.GetTexture() });
		_ssaoHandles._cshV = _shader->BindDescriptorSetTextures(1, { _tex });

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

void Blur::GenerateForBloom()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_BLOOM;

	auto cb = renderer.GetCommandBuffer();

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize(), nullptr, 0.5f, 0).UniformBufferFormat();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_bloomHandles._rphH, _bloomHandles._fbhH, { bloom.GetPongTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_BLOOM_H]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshH);
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize(), nullptr, 0, 0.5f).UniformBufferFormat();
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
		cb->BeginRenderPass(_ssaoHandles._rphH, _ssaoHandles._fbhH, { _tex->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO_H]);
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssaoHandles._cshH);
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
