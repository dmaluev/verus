#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Bloom::UB_BloomVS Bloom::s_ubBloomVS;
Bloom::UB_BloomFS Bloom::s_ubBloomFS;

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
	VERUS_QREF_RENDERER;

	_rph = renderer->CreateSimpleRenderPass(CGI::Format::srgbR8G8B8A8);

	_shader.Init("[Shaders]:Bloom.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubBloomVS, sizeof(s_ubBloomVS), 100, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubBloomFS, sizeof(s_ubBloomFS), 100,
		{
			CGI::Sampler::nearestMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Bloom::Done()
{
	VERUS_DONE(Bloom);
}

void Bloom::OnSwapChainResized()
{
	VERUS_QREF_RENDERER;
	{
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
		_tex.Done();
	}
	{
		const int w = renderer.GetSwapChainWidth() / 2;
		const int h = renderer.GetSwapChainHeight() / 2;
		CGI::TextureDesc texDesc;
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = w;
		texDesc._height = h;
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex[TEX_PING].Init(texDesc);
		_tex[TEX_PONG].Init(texDesc);
		_fbh = renderer->CreateFramebuffer(_rph, { _tex[TEX_PING] }, w, h);
		_csh = _shader->BindDescriptorSetTextures(1, { renderer.GetDS().GetComposedTextureA() });
	}
	renderer.GetDS().InitByBloom(_tex[TEX_PING]);
}

void Bloom::Generate()
{
	VERUS_QREF_RENDERER;

	s_ubBloomVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBloomVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubBloomFS._exposure.x = renderer.GetExposure();

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rph, _fbh, { _tex[TEX_PING]->GetClearValue() });

	cb->BindPipeline(_pipe);

	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();

	renderer.DrawQuad(&(*cb));

	cb->EndRenderPass();

	Blur::I().GenerateForBloom();
}

CGI::TexturePtr Bloom::GetTexture() const
{
	return _tex[TEX_PING];
}

CGI::TexturePtr Bloom::GetPongTexture() const
{
	return _tex[TEX_PONG];
}
