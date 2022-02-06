// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

Cinema::UB_CinemaVS Cinema::s_ubCinemaVS;
Cinema::UB_CinemaFS Cinema::s_ubCinemaFS;

Cinema::Cinema()
{
}

Cinema::~Cinema()
{
	Done();
}

void Cinema::Init()
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (!settings._postProcessCinema)
	{
		VERUS_LOG_INFO("Cinema disabled");
		return;
	}

	_shader.Init("[Shaders]:Cinema.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubCinemaVS, sizeof(s_ubCinemaVS), 2, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(1, &s_ubCinemaFS, sizeof(s_ubCinemaFS), 2, { CGI::Sampler::linearMipL }, CGI::ShaderStageFlags::fs);
	_shader->CreatePipelineLayout();

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader, "#", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_MAD;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe.Init(pipeDesc);
	}

	_texFilmGrain.Init("[Textures]:FilmGrain.FX.dds");
}

void Cinema::Done()
{
	VERUS_DONE(Cinema);
}

void Cinema::Draw()
{
	if (!IsInitialized())
		return;

	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

	if (!settings._postProcessCinema)
		return;

	if (_editMode)
	{
		ImGui::DragFloat("Cinema flickerStrength", &_flickerStrength, 0.001f);
		ImGui::DragFloat("Cinema noiseStrength", &_noiseStrength, 0.001f);
	}

	if (!_csh.IsSet())
	{
		if (_texFilmGrain->IsLoaded())
			_csh = _shader->BindDescriptorSetTextures(1, { _texFilmGrain });
		else
			return;
	}

	if (timer.IsEventEvery(42)) // 24 FPS.
	{
		VERUS_QREF_UTILS;
		_uOffset = floor(utils.GetRandom().NextFloat() * _texFilmGrain->GetSize().getX());
		_vOffset = floor(utils.GetRandom().NextFloat() * _texFilmGrain->GetSize().getY());
		_brightness = Math::Lerp(1.f, utils.GetRandom().NextFloat(), _flickerStrength);
	}

	auto cb = renderer.GetCommandBuffer();

	s_ubCinemaVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubCinemaVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubCinemaVS._matP = Math::ToUVMatrix(0, renderer.GetDS().GetGBuffer(0)->GetSize(), &_texFilmGrain->GetSize(), _uOffset, _vOffset).UniformBufferFormat();
	s_ubCinemaFS._brightness_noiseStrength.x = _brightness;
	s_ubCinemaFS._brightness_noiseStrength.y = _noiseStrength;

	cb->BindPipeline(_pipe);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _csh);
	_shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}
