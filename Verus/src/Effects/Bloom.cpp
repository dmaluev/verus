#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Bloom::UB_BloomVS        Bloom::s_ubBloomVS;
Bloom::UB_BloomFS        Bloom::s_ubBloomFS;
Bloom::UB_BloomGodRaysFS Bloom::s_ubBloomGodRaysFS;

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
			CGI::Sampler::linearClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubBloomGodRaysFS, sizeof(s_ubBloomGodRaysFS), 100,
		{
			CGI::Sampler::linearClampMipN,
			CGI::Sampler::shadow
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#GodRays", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	OnSwapChainResized();
}

void Bloom::InitByAtmosphere(CGI::TexturePtr texShadow)
{
	VERUS_QREF_RENDERER;

	_texAtmoShadow = texShadow;

	_cshGodRays = _shader->BindDescriptorSetTextures(2, { renderer.GetTexDepthStencil(), _texAtmoShadow });
}

void Bloom::Done()
{
	VERUS_QREF_RENDERER;
	_shader->FreeDescriptorSet(_cshGodRays);
	_shader->FreeDescriptorSet(_csh);
	renderer->DeleteFramebuffer(_fbh);
	VERUS_DONE(Bloom);
}

void Bloom::OnSwapChainResized()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_ATMO;
	{
		_shader->FreeDescriptorSet(_cshGodRays);
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

		if (_texAtmoShadow)
			InitByAtmosphere(_texAtmoShadow);
	}
	renderer.GetDS().InitByBloom(_tex[TEX_PING]);
}

void Bloom::Generate()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_ATMO;
	VERUS_QREF_SM;

	auto cb = renderer.GetCommandBuffer();

	s_ubBloomVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBloomVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubBloomFS._exposure.x = renderer.GetExposure();
	s_ubBloomGodRaysFS._matInvVP = Matrix4(VMath::inverse(sm.GetMainCamera()->GetMatrixVP())).UniformBufferFormat();
	s_ubBloomGodRaysFS._matSunShadow = atmo.GetShadowMap().GetShadowMatrix(0).UniformBufferFormat();
	s_ubBloomGodRaysFS._matSunShadowCSM1 = atmo.GetShadowMap().GetShadowMatrix(1).UniformBufferFormat();
	s_ubBloomGodRaysFS._matSunShadowCSM2 = atmo.GetShadowMap().GetShadowMatrix(2).UniformBufferFormat();
	s_ubBloomGodRaysFS._matSunShadowCSM3 = atmo.GetShadowMap().GetShadowMatrix(3).UniformBufferFormat();
	memcpy(&s_ubBloomGodRaysFS._shadowConfig, &atmo.GetShadowMap().GetConfig(), sizeof(s_ubBloomGodRaysFS._shadowConfig));
	s_ubBloomGodRaysFS._splitRanges = atmo.GetShadowMap().GetSplitRanges().GLM();
	s_ubBloomGodRaysFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubBloomGodRaysFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubBloomGodRaysFS._eyePos = float4(atmo.GetEyePosition().GLM(), 0);

	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly, 0);
	cb->BeginRenderPass(_rph, _fbh, { _tex[TEX_PING]->GetClearValue() });

	cb->BindPipeline(_pipe);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	cb->BindDescriptors(_shader, 2, _cshGodRays);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
	cb->PipelineImageMemoryBarrier(renderer.GetTexDepthStencil(), CGI::ImageLayout::depthStencilReadOnly, CGI::ImageLayout::depthStencilAttachment, 0);

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
