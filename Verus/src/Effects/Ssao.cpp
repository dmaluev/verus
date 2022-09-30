// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessSSAO)
	{
		VERUS_LOG_INFO("SSAO disabled");
		return;
	}

	_rph = renderer->CreateSimpleRenderPass(CGI::Format::unormR8G8B8A8); // >> GBuffer3.r

	_shader.Init("[Shaders]:Ssao.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubSsaoVS, sizeof(s_ubSsaoVS), 16, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubSsaoFS, sizeof(s_ubSsaoFS), 16,
		{
			CGI::Sampler::nearestMipN, // RandNormals
			CGI::Sampler::nearestClampMipN, // GBuffer1
			CGI::Sampler::nearestClampMipN, // Depth
		}, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", _rph);
		pipeDesc._colorAttachWriteMasks[0] = "r"; // >> GBuffer3.r
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._name = "Ssao.RandNormals";
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texDesc._width = 4;
	texDesc._height = 4;
	_texRandNormals.Init(texDesc);

	OnSwapChainResized();
}

void Ssao::InitCmd()
{
	VERUS_QREF_CONST_SETTINGS;

	if (!settings._postProcessSSAO)
		return;

	UpdateRandNormalsTexture();
}

void Ssao::Done()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbh);
	renderer->DeleteRenderPass(_rph);
	VERUS_DONE(Ssao);
}

void Ssao::OnSwapChainResized()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessSSAO)
		return;

	const int scaledCombinedSwapChainWidth = settings.Scale(renderer.GetCombinedSwapChainWidth());
	const int scaledCombinedSwapChainHeight = settings.Scale(renderer.GetCombinedSwapChainHeight());

	{
		_shader->FreeDescriptorSet(_csh);
		renderer->DeleteFramebuffer(_fbh);
	}
	{
		_fbh = renderer->CreateFramebuffer(_rph, { renderer.GetDS().GetGBuffer(3) }, scaledCombinedSwapChainWidth, scaledCombinedSwapChainHeight);
		_csh = _shader->BindDescriptorSetTextures(1,
			{
				_texRandNormals,
				renderer.GetDS().GetGBuffer(1),
				renderer.GetTexDepthStencil()
			});
	}
}

void Ssao::Generate()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!settings._postProcessSSAO)
		return;

	if (_editMode)
	{
		ImGui::DragFloat("SSAO smallRad", &_smallRad, 0.001f);
		ImGui::DragFloat("SSAO largeRad", &_largeRad, 0.001f);
		ImGui::DragFloat("SSAO weightScale", &_weightScale, 0.001f);
		ImGui::DragFloat("SSAO weightBias", &_weightBias, 0.001f);
		ImGui::Checkbox("SSAO blur", &_blur);
	}

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rph, _fbh, { renderer.GetDS().GetGBuffer(3)->GetClearValue() });

	World::RCamera passCamera = *wm.GetPassCamera();

	s_ubSsaoVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubSsaoVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubSsaoVS._matP = Math::ToUVMatrix(0, cb->GetViewportSize(), &_texRandNormals->GetSize()).UniformBufferFormat();
	s_ubSsaoVS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubSsaoFS._tcViewScaleBias = cb->GetViewScaleBias().GLM();
	s_ubSsaoFS._zNearFarEx = passCamera.GetZNearFarEx().GLM();
	s_ubSsaoFS._camScale.x = passCamera.GetFovScale() / passCamera.GetAspectRatio();
	s_ubSsaoFS._camScale.y = -passCamera.GetFovScale();
	s_ubSsaoFS._smallRad_largeRad_weightScale_weightBias.x = _smallRad;
	s_ubSsaoFS._smallRad_largeRad_weightScale_weightBias.y = _largeRad;
	s_ubSsaoFS._smallRad_largeRad_weightScale_weightBias.z = _weightScale;
	s_ubSsaoFS._smallRad_largeRad_weightScale_weightBias.w = _weightBias;

	cb->BindPipeline(_pipe);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();

	if (_blur)
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
	_texRandNormals->UpdateSubresource(vData.data());
}
