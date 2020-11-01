// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Ssao::UB_SsaoVS Ssao::s_ubSsaoVS;
Ssao::UB_SsaoFS Ssao::s_ubSsaoFS;

Ssao::Ssao()
{
}

Ssao::~Ssao()
{
	Done();
}

void Ssao::Init()
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_rph = renderer->CreateSimpleRenderPass(CGI::Format::unormR8);

	_shader.Init("[Shaders]:Ssao.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubSsaoVS, sizeof(s_ubSsaoVS), 1, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubSsaoFS, sizeof(s_ubSsaoFS), 1,
		{
			CGI::Sampler::nearestMipN, // RandNormals
			CGI::Sampler::nearestClampMipN, // GBuffer1
			CGI::Sampler::linearClampMipN, // Depth
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._name = "Ssao.RandNormals";
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texDesc._width = 4;
	texDesc._height = 4;
	_tex[TEX_RAND_NORMALS].Init(texDesc);

	OnSwapChainResized();
}

void Ssao::InitCmd()
{
	UpdateRandNormalsTexture();
}

void Ssao::Done()
{
	VERUS_DONE(Ssao);
}

void Ssao::OnSwapChainResized()
{
	if (!IsInitialized())
		return;
	VERUS_QREF_RENDERER;
	{
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
		_tex[TEX_GEN_AO].Done();
	}
	{
		CGI::TextureDesc texDesc;
		texDesc._name = "Ssao.GenAO";
		texDesc._format = CGI::Format::unormR8;
		texDesc._width = renderer.GetSwapChainWidth();
		texDesc._height = renderer.GetSwapChainHeight();
		texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
		_tex[TEX_GEN_AO].Init(texDesc);
		_fbh = renderer->CreateFramebuffer(_rph, { _tex[TEX_GEN_AO] }, renderer.GetSwapChainWidth(), renderer.GetSwapChainHeight());
		_csh = _shader->BindDescriptorSetTextures(1, { _tex[TEX_RAND_NORMALS], renderer.GetDS().GetGBuffer(1), renderer.GetTexDepthStencil() });
	}
	renderer.GetDS().InitBySsao(_tex[TEX_GEN_AO]);
}

void Ssao::Generate()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	Scene::RCamera cam = *sm.GetCamera();

	auto cb = renderer.GetCommandBuffer();

	s_ubSsaoVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubSsaoVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubSsaoVS._matP = Math::ToUVMatrix(0, _tex[TEX_GEN_AO]->GetSize(), &_tex[TEX_RAND_NORMALS]->GetSize()).UniformBufferFormat();
	s_ubSsaoFS._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();
	s_ubSsaoFS._camScale.x = cam.GetFovScale() / cam.GetAspectRatio();
	s_ubSsaoFS._camScale.y = -cam.GetFovScale();

	cb->BeginRenderPass(_rph, _fbh, { _tex[TEX_GEN_AO]->GetClearValue() });

	cb->BindPipeline(_pipe);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();

	Blur::I().GenerateForSsao();
}

void Ssao::UpdateRandNormalsTexture()
{
	Vector<Point3> vPoints;
	Math::Sphere::EvenlyDistPoints(4 * 4, vPoints);
	Random random(6270);
	std::shuffle(vPoints.begin(), vPoints.end(), random.GetGenerator());
	Vector<UINT32> vData(vPoints.size());
	VERUS_FOR(i, vPoints.size())
	{
		Vector3 v3(vPoints[i]);
		v3.setZ(abs(v3.getZ()));
		Vector4 v4 = Vector4(v3 * 0.5f + Vector3::Replicate(0.5f), 1);
		vData[i] = Convert::ColorFloatToInt32(v4.ToPointer(), false);
	}
	_tex[TEX_RAND_NORMALS]->UpdateSubresource(vData.data());
}

CGI::TexturePtr Ssao::GetTexture() const
{
	return _tex[TEX_GEN_AO];
}
