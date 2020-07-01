#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Blur::UB_BlurVS Blur::s_ubBlurVS;
Blur::UB_BlurFS Blur::s_ubBlurFS;

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

	_shader.Init("[Shaders]:Blur.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubBlurVS, sizeof(s_ubBlurVS), 100, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBlurFS, sizeof(s_ubBlurFS), 100,
		{
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

	OnSwapChainResized();
}

void Blur::Done()
{
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
		_ssaoHandles.FreeDescriptorSets(_shader);
		_ssaoHandles.DeleteFramebuffers();
		_bloomHandles.FreeDescriptorSets(_shader);
		_bloomHandles.DeleteFramebuffers();
		_tex.Done();
	}
	{
		CGI::TextureDesc texDesc;
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = renderer.GetSwapChainWidth();
		texDesc._height = renderer.GetSwapChainHeight();
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex.Init(texDesc);

		const int halfW = renderer.GetSwapChainWidth() / 2;
		const int halfH = renderer.GetSwapChainHeight() / 2;
		_bloomHandles._fbhH = renderer->CreateFramebuffer(_bloomHandles._rphH, { bloom.GetPongTexture() }, halfW, halfH);
		_bloomHandles._fbhV = renderer->CreateFramebuffer(_bloomHandles._rphV, { bloom.GetTexture() }, halfW, halfH);
		_bloomHandles._cshH = _shader->BindDescriptorSetTextures(1, { bloom.GetTexture() });
		_bloomHandles._cshV = _shader->BindDescriptorSetTextures(1, { bloom.GetPongTexture() });

		_ssaoHandles._fbhH = renderer->CreateFramebuffer(_ssaoHandles._rphH, { _tex }, renderer.GetSwapChainWidth(), renderer.GetSwapChainHeight());
		_ssaoHandles._fbhV = renderer->CreateFramebuffer(_ssaoHandles._rphV, { ssao.GetTexture() }, renderer.GetSwapChainWidth(), renderer.GetSwapChainHeight());
		_ssaoHandles._cshH = _shader->BindDescriptorSetTextures(1, { ssao.GetTexture() });
		_ssaoHandles._cshV = _shader->BindDescriptorSetTextures(1, { _tex });
	}
}

void Blur::Generate()
{
}

void Blur::GenerateForBloom()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_BLOOM;

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize(), nullptr, 0.5f, 0).UniformBufferFormat();

	auto cb = renderer.GetCommandBuffer();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_bloomHandles._rphH, _bloomHandles._fbhH, { bloom.GetPongTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_BLOOM_H]);

		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshH);

		renderer.DrawQuad(&(*cb));

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, bloom.GetTexture()->GetSize(), nullptr, 0, 0.5f).UniformBufferFormat();
	{
		cb->BeginRenderPass(_bloomHandles._rphV, _bloomHandles._fbhV, { bloom.GetTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_BLOOM_V]);

		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _bloomHandles._cshV);

		renderer.DrawQuad(&(*cb));

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForSsao()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SSAO;

	s_ubBlurVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBlurVS._matV = Math::ToUVMatrix(0, _tex->GetSize(), nullptr, 0.5f, 0).UniformBufferFormat();

	auto cb = renderer.GetCommandBuffer();

	_shader->BeginBindDescriptors();
	{
		cb->BeginRenderPass(_ssaoHandles._rphH, _ssaoHandles._fbhH, { _tex->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO_H]);

		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssaoHandles._cshH);

		renderer.DrawQuad(&(*cb));

		cb->EndRenderPass();
	}
	s_ubBlurVS._matV = Math::ToUVMatrix(0, _tex->GetSize(), nullptr, 0, 0.5f).UniformBufferFormat();
	{
		cb->BeginRenderPass(_ssaoHandles._rphV, _ssaoHandles._fbhV, { ssao.GetTexture()->GetClearValue() });

		cb->BindPipeline(_pipe[PIPE_SSAO_V]);

		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _ssaoHandles._cshV);

		renderer.DrawQuad(&(*cb));

		cb->EndRenderPass();
	}
	_shader->EndBindDescriptors();
}

void Blur::GenerateForDepthOfField()
{
}

void Blur::GenerateForMotionBlur()
{
}
