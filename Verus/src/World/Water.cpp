// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

Water::UB_WaterVS        Water::s_ubWaterVS;
Water::UB_WaterFS        Water::s_ubWaterFS;
Water::UB_Gen            Water::s_ubGen;
Water::UB_GenHeightmapFS Water::s_ubGenHeightmapFS;
Water::UB_GenNormalsFS   Water::s_ubGenNormalsFS;


Water::Water()
{
}

Water::~Water()
{
	Done();
}

void Water::Init(RTerrain terrain)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_pTerrain = &terrain;

	_rphGenHeightmap = renderer->CreateSimpleRenderPass(CGI::Format::floatR16, CGI::RP::Attachment::LoadOp::dontCare, CGI::ImageLayout::xsReadOnly);
	_rphGenNormals = renderer->CreateSimpleRenderPass(CGI::Format::unormR10G10B10A2);
	_rphReflection = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Color", CGI::Format::floatR11G11B10).LoadOpDontCare().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("Color", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	_shader[SHADER_MAIN].Init("[Shaders]:Water.hlsl");
	_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubWaterVS, sizeof(s_ubWaterVS), settings.GetLimits()._water_ubVSCapacity,
		{
			CGI::Sampler::linearClampMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipN
		}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubWaterFS, sizeof(s_ubWaterFS), settings.GetLimits()._water_ubFSCapacity,
		{
			CGI::Sampler::linearClampMipN, // TerrainHeightmap
			CGI::Sampler::linearMipL, // GenHeightmap
			CGI::Sampler::custom, // GenNormals
			CGI::Sampler::linearMipL, // Foam
			CGI::Sampler::anisoClamp, // Reflection
			CGI::Sampler::linearClampMipN, // Refraction
			CGI::Sampler::linearClampMipN, // GBuffer3
			CGI::Sampler::shadow,
			CGI::Sampler::nearestMipN
		}, CGI::ShaderStageFlags::fs);
	_shader[SHADER_MAIN]->CreatePipelineLayout();

	_shader[SHADER_GEN].Init("[Shaders]:WaterGen.hlsl");
	_shader[SHADER_GEN]->CreateDescriptorSet(0, &s_ubGen, sizeof(s_ubGen), 4, {}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_GEN]->CreateDescriptorSet(1, &s_ubGenHeightmapFS, sizeof(s_ubGenHeightmapFS), 2, { CGI::Sampler::linearMipN }, CGI::ShaderStageFlags::fs);
	_shader[SHADER_GEN]->CreateDescriptorSet(2, &s_ubGenNormalsFS, sizeof(s_ubGenNormalsFS), 2, { CGI::Sampler::linearMipN }, CGI::ShaderStageFlags::fs);
	_shader[SHADER_GEN]->CreatePipelineLayout();

	Vector<UINT16> vIndices;
	Math::CreateStripGrid(_gridWidth - 1, _gridHeight - 1, vIndices);
	_indexCount = Utils::Cast32(vIndices.size());

	CGI::GeometryDesc geoDesc;
	geoDesc._name = "Water.Geo";
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(Vertex, _pos), CGI::ViaType::floats, 4, CGI::ViaUsage::position, 0},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(_gridWidth * _gridHeight, 0);
	_geo->CreateIndexBuffer(Utils::Cast32(vIndices.size()));
	_geo->UpdateIndexBuffer(vIndices.data());

	CreateWaterPlane();

	{
		CGI::PipelineDesc pipeDesc(_geo, _shader[SHADER_MAIN], "#0", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		//pipeDesc._rasterizationState._polygonMode = CGI::PolygonMode::line;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_MAIN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_GEN], "#GenHeightmap", _rphGenHeightmap);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_GEN_HEIGHTMAP].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_GEN], "#GenNormals", _rphGenNormals);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_GEN_NORMALS].Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._url = "[Textures]:Water/Foam.dds";
	texDesc._flags = CGI::TextureDesc::Flags::anyShaderResource;
	_tex[TEX_FOAM].Init(texDesc);

	_tex[TEX_SOURCE_HEIGHTMAP].Init("[Textures]:Water/Heightmap.FX.dds");

	CGI::SamplerDesc normalsSamplerDesc;
	normalsSamplerDesc.SetFilter("ll");
	normalsSamplerDesc.SetAddressMode("rr");
	normalsSamplerDesc._mipLodBias = 0.5f;
	normalsSamplerDesc._minLod = 1;
	texDesc.Reset();
	texDesc._name = "Water.GenHeightmap";
	texDesc._format = CGI::Format::floatR16;
	texDesc._width = _genSide;
	texDesc._height = _genSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::anyShaderResource | CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_GEN_HEIGHTMAP].Init(texDesc);
	texDesc._name = "Water.GenNormals";
	texDesc._format = CGI::Format::unormR10G10B10A2;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	texDesc._pSamplerDesc = &normalsSamplerDesc;
	_tex[TEX_GEN_NORMALS].Init(texDesc);

	_fbhGenHeightmap = renderer->CreateFramebuffer(_rphGenHeightmap, { _tex[TEX_GEN_HEIGHTMAP] }, _genSide, _genSide);
	_fbhGenNormals = renderer->CreateFramebuffer(_rphGenNormals, { _tex[TEX_GEN_NORMALS] }, _genSide, _genSide);

	_shader[SHADER_GEN]->FreeDescriptorSet(_cshGenNormals);
	_cshGenNormals = _shader[SHADER_GEN]->BindDescriptorSetTextures(2, { _tex[TEX_GEN_HEIGHTMAP] });

	texDesc.Reset();
	texDesc._name = "Water.Reflection";
	texDesc._format = CGI::Format::floatR11G11B10;
	texDesc._width = 1024;
	texDesc._height = 512;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_REFLECTION].Init(texDesc);
	texDesc._name = "Water.ReflectionDepth";
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._mipLevels = 1;
	texDesc._flags = CGI::TextureDesc::Flags::none;
	_tex[TEX_REFLECTION_DEPTH].Init(texDesc);

	_fbhReflection = renderer->CreateFramebuffer(_rphReflection,
		{
			_tex[TEX_REFLECTION],
			_tex[TEX_REFLECTION_DEPTH]
		},
		texDesc._width,
		texDesc._height);

	VERUS_FOR(i, s_maxHarmonics)
		_amplitudes[i] = PhillipsSpectrum(0.5f + i / 2.f);

	_pTerrain->InitByWater();
}

void Water::Done()
{
	if (_shader[SHADER_GEN])
	{
		_shader[SHADER_GEN]->FreeDescriptorSet(_cshGenNormals);
		_shader[SHADER_GEN]->FreeDescriptorSet(_cshGenHeightmap);
	}
	if (_shader[SHADER_MAIN])
	{
		_shader[SHADER_MAIN]->FreeDescriptorSet(_cshWaterFS);
		_shader[SHADER_MAIN]->FreeDescriptorSet(_cshWaterVS);
	}
	VERUS_DONE(Water);
}

void Water::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;
	_phase = fmod(_phase + dt * (1 / 19.f), 1.f);
	_wavePhase = fmod(_wavePhase + dt * (1 / 29.f), 1.f);
}

void Water::Draw()
{
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	VERUS_QREF_ATMO;
	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!_cshWaterFS.IsSet())
	{
		OnSwapChainResized();
		if (!_cshWaterFS.IsSet())
			return;
	}

	auto cb = renderer.GetCommandBuffer();

	RCamera headCamera = *wm.GetHeadCamera();
	RCamera passCamera = *wm.GetPassCamera();

	Transform3 matW;

	Point3 headPosAtGroundLevel = headCamera.GetEyePosition();
	headPosAtGroundLevel.setY(0);

	Vector3 flatDir = headCamera.GetFrontDirection();
	flatDir.setY(0);
	flatDir = VMath::normalizeApprox(flatDir);
	const float angle = atan2(
		static_cast<float>(flatDir.getX()),
		static_cast<float>(flatDir.getZ())); // Align with high density sector.
	const Matrix3 matR = Matrix3::rotationY(angle);
	matW = Transform3(matR, Vector3(headPosAtGroundLevel - flatDir * 5 * abs(headCamera.GetFrontDirection().getY())));

	const Matrix4 matScreen = Matrix4(Math::ToUVMatrix()) * passCamera.GetMatrixVP();

	s_ubWaterVS._matW = matW.UniformBufferFormat();
	s_ubWaterVS._matVP = passCamera.GetMatrixVP().UniformBufferFormat();
	s_ubWaterVS._matScreen = matScreen.UniformBufferFormat();
	s_ubWaterVS._headPos = float4(headCamera.GetEyePosition().GLM(), 0);
	s_ubWaterVS._eyePos_invMapSide = float4(passCamera.GetEyePosition().GLM(), 1.f / _pTerrain->GetMapSide());
	s_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.x = 1 / _patchSide;
	s_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.y =
		Math::ComputeDistToMipScale(static_cast<float>(_genSide << 6), static_cast<float>(renderer.GetCurrentViewWidth()), _patchSide, headCamera.GetYFov());
	s_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.z =
		Math::ComputeDistToMipScale(static_cast<float>(_pTerrain->GetMapSide()), static_cast<float>(renderer.GetCurrentViewHeight()), static_cast<float>(_pTerrain->GetMapSide()), headCamera.GetYFov());
	s_ubWaterVS._waterScale_distToMipScale_landDistToMipScale_wavePhase.w = _wavePhase;

	s_ubWaterFS._matV = passCamera.GetMatrixV().UniformBufferFormat();
	s_ubWaterFS._phase_wavePhase_camScale.x = _phase;
	s_ubWaterFS._phase_wavePhase_camScale.y = _wavePhase;
	s_ubWaterFS._phase_wavePhase_camScale.z = passCamera.GetFovScale() / passCamera.GetAspectRatio();
	s_ubWaterFS._phase_wavePhase_camScale.w = -passCamera.GetFovScale();
	s_ubWaterFS._diffuseColorShallow = float4(_diffuseColorShallow.GLM(), 0);
	s_ubWaterFS._diffuseColorDeep = float4(_diffuseColorDeep.GLM(), 0);
	s_ubWaterFS._ambientColor = float4(atmo.GetAmbientColor().GLM(), 0);
	if (IsUnderwater())
		s_ubWaterFS._fogColor = Vector4(VMath::mulPerElem(_diffuseColorShallow, atmo.GetAmbientColor()) * 0.1f, _fogDensity).GLM();
	else
		s_ubWaterFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubWaterFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubWaterFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubWaterFS._matShadow = csmb.GetShadowMatrix(0).UniformBufferFormat();
	s_ubWaterFS._matShadowCSM1 = csmb.GetShadowMatrix(1).UniformBufferFormat();
	s_ubWaterFS._matShadowCSM2 = csmb.GetShadowMatrix(2).UniformBufferFormat();
	s_ubWaterFS._matShadowCSM3 = csmb.GetShadowMatrix(3).UniformBufferFormat();
	s_ubWaterFS._matScreenCSM = csmb.GetScreenMatrixVP().UniformBufferFormat();
	s_ubWaterFS._csmSliceBounds = csmb.GetSliceBounds().GLM();
	memcpy(&s_ubWaterFS._shadowConfig, &csmb.GetConfig(), sizeof(s_ubWaterFS._shadowConfig));

	cb->BindPipeline(_pipe[PIPE_MAIN]);
	cb->BindVertexBuffers(_geo);
	cb->BindIndexBuffer(_geo);
	_shader[SHADER_MAIN]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_MAIN], 0, _cshWaterVS);
	cb->BindDescriptors(_shader[SHADER_MAIN], 1, _cshWaterFS);
	_shader[SHADER_MAIN]->EndBindDescriptors();
	cb->DrawIndexed(_indexCount);
}

void Water::OnSwapChainResized()
{
	if (!IsInitialized())
		return;
	if (!_tex[TEX_FOAM]->IsLoaded())
		return;

	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;

	_shader[SHADER_MAIN]->FreeDescriptorSet(_cshWaterFS);
	_shader[SHADER_MAIN]->FreeDescriptorSet(_cshWaterVS);
	_cshWaterVS = _shader[SHADER_MAIN]->BindDescriptorSetTextures(0, { _pTerrain->GetHeightmapTexture(), _tex[TEX_GEN_HEIGHTMAP], _tex[TEX_FOAM] });
	_cshWaterFS = _shader[SHADER_MAIN]->BindDescriptorSetTextures(1,
		{
			_pTerrain->GetHeightmapTexture(),
			_tex[TEX_GEN_HEIGHTMAP],
			_tex[TEX_GEN_NORMALS],
			_tex[TEX_FOAM],
			_tex[TEX_REFLECTION],
			renderer.GetDS().GetComposedTextureB(),
			renderer.GetDS().GetGBuffer(3),
			csmb.GetTexture(),
			csmb.GetTexture(),
		});
}

void Water::BeginPlanarReflection(CGI::PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();

	pCB->BeginRenderPass(_rphReflection, _fbhReflection,
		{
			_tex[TEX_REFLECTION]->GetClearValue(),
			_tex[TEX_REFLECTION_DEPTH]->GetClearValue()
		},
		CGI::ViewportScissorFlags::setAllForFramebuffer);

	_headCamera = *wm.GetHeadCamera();
	if (!IsUnderwater(_headCamera.GetEyePosition()))
		_headCamera.EnableReflectionMode();

	_pPrevPassCamera = wm.SetPassCamera(&_headCamera);
	_pPrevHeadCamera = wm.SetHeadCamera(&_headCamera);
}

void Water::EndPlanarReflection(CGI::PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();

	pCB->EndRenderPass();

	wm.SetPassCamera(_pPrevPassCamera);
	wm.SetHeadCamera(_pPrevHeadCamera);

	VERUS_PROFILER_BEGIN_EVENT(pCB, VERUS_COLOR_BLACK, "Water/PlanarReflection/GenerateMips");
	_tex[TEX_REFLECTION]->GenerateMips();
	VERUS_PROFILER_END_EVENT(pCB);
}

void Water::GenerateTextures()
{
	if (!_cshGenHeightmap.IsSet())
	{
		if (_tex[TEX_SOURCE_HEIGHTMAP]->IsLoaded())
		{
			_shader[SHADER_GEN]->FreeDescriptorSet(_cshGenHeightmap);
			_cshGenHeightmap = _shader[SHADER_GEN]->BindDescriptorSetTextures(1, { _tex[TEX_SOURCE_HEIGHTMAP] });
		}
		else
			return;
	}

	GenerateHeightmapTexture();
	GenerateNormalsTexture();
}

void Water::GenerateHeightmapTexture()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(96, 96, 160, 255), "Water/GenerateHeightmapTexture");
	{
		s_ubGen._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubGen._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubGenHeightmapFS._phase.x = _phase;
		memcpy(&s_ubGenHeightmapFS._amplitudes, _amplitudes, sizeof(_amplitudes));

		cb->BeginRenderPass(_rphGenHeightmap, _fbhGenHeightmap, { _tex[TEX_GEN_HEIGHTMAP]->GetClearValue(), },
			CGI::ViewportScissorFlags::setAllForFramebuffer);

		cb->BindPipeline(_pipe[PIPE_GEN_HEIGHTMAP]);
		_shader[SHADER_GEN]->BeginBindDescriptors();
		cb->BindDescriptors(_shader[SHADER_GEN], 0);
		cb->BindDescriptors(_shader[SHADER_GEN], 1, _cshGenHeightmap);
		_shader[SHADER_GEN]->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();

		VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_BLACK, "Water/GenerateHeightmapTexture/GenerateMips");
		_tex[TEX_GEN_HEIGHTMAP]->GenerateMips(cb.Get());
		VERUS_PROFILER_END_EVENT(cb);
	}
	VERUS_PROFILER_END_EVENT(cb);
}

void Water::GenerateNormalsTexture()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(128, 128, 255, 255), "Water/GenerateNormalsTexture");
	{
		s_ubGen._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubGen._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubGenNormalsFS._textureSize = _tex[TEX_GEN_HEIGHTMAP]->GetSize().GLM();
		s_ubGenNormalsFS._waterScale.x = 1 / _patchSide;

		cb->BeginRenderPass(_rphGenNormals, _fbhGenNormals, { _tex[TEX_GEN_NORMALS]->GetClearValue(), },
			CGI::ViewportScissorFlags::setAllForFramebuffer);

		cb->BindPipeline(_pipe[PIPE_GEN_NORMALS]);
		_shader[SHADER_GEN]->BeginBindDescriptors();
		cb->BindDescriptors(_shader[SHADER_GEN], 0);
		cb->BindDescriptors(_shader[SHADER_GEN], 2, _cshGenNormals);
		_shader[SHADER_GEN]->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());

		cb->EndRenderPass();

		VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_BLACK, "Water/GenerateNormalsTexture/GenerateMips");
		_tex[TEX_GEN_NORMALS]->GenerateMips(cb.Get());
		VERUS_PROFILER_END_EVENT(cb);
	}
	VERUS_PROFILER_END_EVENT(cb);
}

CGI::TexturePtr Water::GetCausticsTexture() const
{
	VERUS_QREF_CONST_SETTINGS;
	if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesReflection)
		return _tex[TEX_GEN_NORMALS];
	else
		return _tex[TEX_SOURCE_HEIGHTMAP];
}

void Water::CreateWaterPlane()
{
	VERUS_QREF_WM;

	const float maxRadius = Math::Min(wm.GetHeadCamera()->GetZFar(), 5000.f);
	const float stepW = 1.f / (_gridWidth - 1);
	const float stepH = 1.f / (_gridHeight - 1);

	float xPolar = 0;
	float zPolar = 0;

	Vector<glm::vec4> vVB;
	vVB.resize(_gridWidth * _gridHeight);

	const float fudgeFactor = 100;
	const float maxAngle = atan(maxRadius / fudgeFactor);
	VERUS_FOR(i, _gridHeight)
	{
		const float radius = tan(zPolar * maxAngle) * fudgeFactor;
		VERUS_FOR(j, _gridWidth)
		{
			float xSqueeze = xPolar;
			VERUS_FOR(k, 6)
				xSqueeze = glm::sineEaseInOut(xSqueeze);
			xSqueeze = Math::Lerp(xSqueeze, xPolar, 0.2f);

			const float xCartesian = radius * sin(xSqueeze * VERUS_2PI);
			const float zCartesian = radius * cos(xSqueeze * VERUS_2PI);
			vVB[i * _gridWidth + j] = glm::vec4(xCartesian, 0, zCartesian, 1);
			xPolar += stepW;
		}
		xPolar = 0;
		zPolar += stepH;
	}
	// Watertight mesh, pun intended:
	VERUS_FOR(i, _gridHeight)
		vVB[i * _gridWidth + (_gridWidth - 1)] = vVB[i * _gridWidth];

	_geo->UpdateVertexBuffer(vVB.data(), 0);
}

float Water::PhillipsSpectrum(float k)
{
	const float kl = k;
	const float k2 = k * k;
	return exp(-1 / (kl * kl)) / (k2 * k2);
}

bool Water::IsUnderwater() const
{
	return _pPrevHeadCamera ?
		IsUnderwater(_pPrevHeadCamera->GetEyePosition()) :
		IsUnderwater(WorldManager::I().GetHeadCamera()->GetEyePosition());
}

bool Water::IsUnderwater(RcPoint3 eyePos) const
{
	return eyePos.getY() < 0;
}
