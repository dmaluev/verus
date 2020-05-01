#include "verus.h"

using namespace verus;
using namespace verus::Scene;

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

	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	_pTerrain = &terrain;

	_rphGenHeightmap = renderer->CreateSimpleRenderPass(CGI::Format::floatR16, CGI::ImageLayout::xsReadOnly);
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
	_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubWaterVS, sizeof(s_ubWaterVS), 100,
		{
			CGI::Sampler::linearClampMipL,
			CGI::Sampler::linearMipL
		}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubWaterFS, sizeof(s_ubWaterFS), 100,
		{
			CGI::Sampler::linearClampMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::custom,
			CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	_shader[SHADER_MAIN]->CreatePipelineLayout();

	_shader[SHADER_GEN].Init("[Shaders]:WaterGen.hlsl");
	_shader[SHADER_GEN]->CreateDescriptorSet(0, &s_ubGen, sizeof(s_ubGen), 100, {}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_GEN]->CreateDescriptorSet(1, &s_ubGenHeightmapFS, sizeof(s_ubGenHeightmapFS), 100, { CGI::Sampler::linearMipN }, CGI::ShaderStageFlags::fs);
	_shader[SHADER_GEN]->CreateDescriptorSet(2, &s_ubGenNormalsFS, sizeof(s_ubGenNormalsFS), 100, { CGI::Sampler::linearMipN }, CGI::ShaderStageFlags::fs);
	_shader[SHADER_GEN]->CreatePipelineLayout();

	Vector<UINT16> vIndices;
	Math::CreateStripGrid(_gridWidth - 1, _gridHeight - 1, vIndices);
	_indexCount = vIndices.size();

	CGI::GeometryDesc geoDesc;
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(Vertex, _pos), CGI::IeType::floats, 4, CGI::IeUsage::position, 0},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(_gridWidth * _gridHeight, 0);
	_geo->CreateIndexBuffer(vIndices.size());
	_geo->UpdateIndexBuffer(vIndices.data());

	CreateWaterPlane();

	{
		CGI::PipelineDesc pipeDesc(_geo, _shader[SHADER_MAIN], "#0", renderer.GetDS().GetRenderPassHandle_Compose(), renderer.GetDS().GetSubpass_Compose());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		//pipeDesc._rasterizationState._polygonMode = CGI::PolygonMode::line;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		_pipe[PIPE_MAIN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_GEN], "#GenHeightmap", _rphGenHeightmap);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_GEN_HEIGHTMAP].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_GEN], "#GenNormals", _rphGenNormals);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_GEN_NORMALS].Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._url = "[Textures]:Water/Foam.dds";
	_tex[TEX_FOAM].Init(texDesc);

	texDesc._url = "[Textures]:Water/Heightmap.FX.dds";
	_tex[TEX_SOURCE_HEIGHTMAP].Init(texDesc);

	CGI::SamplerDesc normalsSamplerDesc;
	normalsSamplerDesc.SetFilter("a");
	normalsSamplerDesc.SetAddressMode("rr");
	normalsSamplerDesc._mipLodBias = 0.5f;
	normalsSamplerDesc._minLod = 1;
	texDesc.Reset();
	texDesc._format = CGI::Format::floatR16;
	texDesc._width = _genSide;
	texDesc._height = _genSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::anyShaderResource | CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_GEN_HEIGHTMAP].Init(texDesc);
	texDesc._format = CGI::Format::unormR10G10B10A2;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	texDesc._pSamplerDesc = &normalsSamplerDesc;
	_tex[TEX_GEN_NORMALS].Init(texDesc);

	_fbhGenHeightmap = renderer->CreateFramebuffer(_rphGenHeightmap, { _tex[TEX_GEN_HEIGHTMAP] }, _genSide, _genSide);
	_fbhGenNormals = renderer->CreateFramebuffer(_rphGenNormals, { _tex[TEX_GEN_NORMALS] }, _genSide, _genSide);

	_cshWaterVS = _shader[SHADER_MAIN]->BindDescriptorSetTextures(0, { _pTerrain->GetHeightmapTexture(), _tex[TEX_GEN_HEIGHTMAP] });

	_cshGenNormals = _shader[SHADER_GEN]->BindDescriptorSetTextures(2, { _tex[TEX_GEN_HEIGHTMAP] });

	texDesc.Reset();
	texDesc._format = CGI::Format::floatR11G11B10;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;
	_tex[TEX_REFLECTION].Init(texDesc);
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._flags = CGI::TextureDesc::Flags::none;
	_tex[TEX_REFLECTION_DEPTH].Init(texDesc);

	_fbhReflection = renderer->CreateFramebuffer(_rphReflection,
		{
			_tex[TEX_REFLECTION],
			_tex[TEX_REFLECTION_DEPTH]
		},
		texDesc._width,
		texDesc._height);

	// Texture for rendering reflection onto:
	/*
	if (settings._sceneWaterQuality >= App::Settings::WaterQuality::simpleReflection)
	{
		texDesc.Reset();
		texDesc._format = CGI::Format::unormR8G8B8A8;
		texDesc._width = 512;
		texDesc._height = 512;
		texDesc._mipLevels = 0;
		texDesc._flags = CGI::TextureDesc::Flags::generateMips;
		_tex[TEX_REFLECT].Init(texDesc);

		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesRefraction)
			_tex[TEX_REFRACT].Init(texDesc);

		texDesc._format = CGI::Format::unormD16;
		_tex[TEX_REFLECT_DEPTH].Init(texDesc);
	}
	*/

	VERUS_FOR(i, s_maxHarmonics)
		_amplitudes[i] = PhillipsSpectrum(0.5f + i / 2.f);
}

void Water::Done()
{
	VERUS_DONE(Water);
}

void Water::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;
	_phase = fmod(_phase + dt * (1 / 29.f), 1.f);
	_phaseWave = fmod(_phaseWave + dt * (1 / 61.f), 1.f);
}

void Water::Draw()
{
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	if (!_cshWaterFS.IsSet())
	{
		if (_tex[TEX_FOAM]->IsLoaded())
		{
			_cshWaterFS = _shader[SHADER_MAIN]->BindDescriptorSetTextures(1,
				{
					_pTerrain->GetHeightmapTexture(),
					_tex[TEX_GEN_HEIGHTMAP],
					_tex[TEX_GEN_NORMALS],
					_tex[TEX_FOAM]
				});
		}
		else
			return;
	}

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_TIMER;
	VERUS_QREF_ATMO;

	auto cb = renderer.GetCommandBuffer();

	RCamera cam = *sm.GetCamera();

	Transform3 matW;

	Point3 eyePosAtGroundLevel = cam.GetEyePosition();
	eyePosAtGroundLevel.setY(0);

	Vector3 flatDir = cam.GetFrontDirection();
	flatDir.setY(0);
	flatDir = VMath::normalizeApprox(flatDir);
	const float angle = atan2(
		static_cast<float>(flatDir.getX()),
		static_cast<float>(flatDir.getZ())); // Align with high density sector.
	const Matrix3 matR = Matrix3::rotationY(angle);
	matW = Transform3(matR, Vector3(eyePosAtGroundLevel - flatDir * 5 * abs(cam.GetFrontDirection().getY())));

	const Matrix4 matWVP = cam.GetMatrixVP() * matW;

	s_ubWaterVS._matW = matW.UniformBufferFormat();
	s_ubWaterVS._matVP = cam.GetMatrixVP().UniformBufferFormat();
	s_ubWaterVS._eyePos = float4(cam.GetEyePosition().GLM(), 0);
	s_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.x = 1 / _patchSide;
	s_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.y = Math::ComputeDistToMipScale(_genSide << 6, renderer.GetSwapChainHeight(), _patchSide, cam.GetFOV());
	s_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.z = 1.f / _pTerrain->GetMapSide();
	s_ubWaterVS._waterScale_distToMipScale_mapSideInv_landDistToMipScale.w = Math::ComputeDistToMipScale(_pTerrain->GetMapSide(), renderer.GetSwapChainHeight(), _pTerrain->GetMapSide(), cam.GetFOV());

	s_ubWaterFS._phase.x = _phase;
	s_ubWaterFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubWaterFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);

	cb->BindVertexBuffers(_geo);
	cb->BindIndexBuffer(_geo);
	cb->BindPipeline(_pipe[PIPE_MAIN]);

	_shader[SHADER_MAIN]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_MAIN], 0, _cshWaterVS);
	cb->BindDescriptors(_shader[SHADER_MAIN], 1, _cshWaterFS);
	cb->DrawIndexed(_indexCount);
	_shader[SHADER_MAIN]->EndBindDescriptors();

	//ms_cbPerObject.matWVP = matWVP.ConstBufferFormat();
	//ms_cbPerObject.matW = matW.ConstBufferFormat();
	//ms_cbPerObject.matV = cam.GetMatrixV().ConstBufferFormat();

	if (false)
	{
		//_sb[SB_DEPTH]->Apply(sbad);

		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesReflection)
		{
			//renderer->SetTextures({ _tex[TEX_HEIGHTMAP] }, 0 + VERUS_ST_VS_OFFSET);
			//renderer->SetTextures({ _texLand }, 5 + VERUS_ST_VS_OFFSET);
		}

		//_s->Bind("TDepth");
		//_s->UpdateBuffer(0);

		//_geo->BeginDraw(0x1);
		//renderer->DrawIndexedPrimitive(CGI::PT_TRIANGLESTRIP, 0, _indexCount);
		//_geo->EndDraw(0x1);
	}
	else
	{
		VERUS_QREF_ATMO;

		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesRefraction) // True refraction:
		{
			//renderer->StretchRect(m_tex[TEX_REFRACT], renderer.GetOffscreenColorTexture());
			//_tex[TEX_REFRACT]->GenerateMipmaps();
		}

		//renderer->SetRenderTargets({ renderer.GetOffscreenColorTexture(), renderer.GetDS().GetGBuffer(1) }, renderer.GetOffscreenDepthTexture());

		//_sb[SB_MASTER]->Apply(sbad);

		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesReflection)
		{
			//renderer->SetTextures({ _tex[TEX_HEIGHTMAP] }, 0 + VERUS_ST_VS_OFFSET);
			//renderer->SetTextures({ _texLand }, 5 + VERUS_ST_VS_OFFSET);
		}

		Matrix4 matTexSpace = Matrix4::identity();

		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::solidColor)
		{
			//renderer->SetTextures({ _tex[TEX_HEIGHTMAP_BASE] });
			//renderer->SetTextures({ _tex[TEX_FOAM] }, 2);
			//renderer->SetTextures({ _texLand }, 5);
			//renderer->SetTextures({ atmo.GetSunShadowTexture() }, 6);
		}
		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::simpleReflection)
		{
			//renderer->SetTextures({ _tex[TEX_REFLECT] }, 3);
			const Transform3 matShift = Transform3::translation(Vector3(0, -0.01f, 0)); // Sky-waterline bleeding fix.
			//matTexSpace = Matrix4(Math::ToUVMatrix(0, Math::TOUV_RENDER_TARGET_VFLIP | Math::TOUV_MAP_TEXELS_TO_PIXELS, _tex[TEX_REFLECT]->GetSize()) * matShift) * matWVP;
		}
		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesReflection)
		{
			//renderer->SetTextures({ _tex[TEX_HEIGHTMAP], _tex[TEX_NORMALMAP] });
		}
		if (settings._sceneWaterQuality >= App::Settings::WaterQuality::trueWavesRefraction)
		{
			//renderer->SetTextures({ _tex[TEX_REFRACT] }, 4);
		}

		//ms_cbPerFrame.matReflect = matTexSpace.ConstBufferFormat();
		//ms_cbPerFrame.shadowTexSize = atmo.GetSunShadowTexture() ? atmo.GetSunShadowTexture()->GetSize() : CVector4(0);
		//ms_cbPerFrame.matSunShadow = atmo.GetSunShadowMatrix(0).ConstBufferFormat();
		//ms_cbPerFrame.matSunShadowCSM1 = atmo.GetSunShadowMatrix(1).ConstBufferFormat();
		//ms_cbPerFrame.matSunShadowCSM2 = atmo.GetSunShadowMatrix(2).ConstBufferFormat();
		//ms_cbPerFrame.matSunShadowCSM3 = atmo.GetSunShadowMatrix(3).ConstBufferFormat();
		//ms_cbPerFrame.csmParams = atmo.GetParamsCSM();
		//ms_cbPerFrame.phases_mapSideInv.x = _phase;
		//ms_cbPerFrame.phases_mapSideInv.y = _phaseWave;
		//ms_cbPerFrame.phases_mapSideInv.z = 1.f / _mapSide;
		//ms_cbPerFrame.eyePos = cam.GetPositionEye();
		//ms_cbPerFrame.colorAmbient = atmo.GetAmbientColor();
		//ms_cbPerFrame.dirToSun = atmo.GetDirToSun();
		//ms_cbPerFrame.colorSun = atmo.GetSunColor();
		//ms_cbPerFrame.colorSunSpec = atmo.GetSunColor();
		//ms_cbPerFrame.fogColor = atmo.GetFogColor();

		//char tech[8];
		//sprintf_s(tech, "T_%d", settings._sceneWaterQuality);
		//_s->Bind(tech);
		//_s->UpdateBuffer(0);
		//_s->UpdateBuffer(1);

		//_geo->BeginDraw(0x1);
		//renderer->DrawIndexedPrimitive(CGI::PT_TRIANGLESTRIP, 0, _indexCount);
		//_geo->EndDraw(0x1);
	}

	///renderer->SetTextures({ nullptr }, 0 + VERUS_ST_VS_OFFSET);

	//renderer.DrawQuad(m_tex[HTEX_HEIGHTMAP]);
}

void Water::BeginReflection(CGI::PBaseCommandBuffer pCB)
{
	VERUS_QREF_SM;
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());

	_reflectionMode = sm.GetCamera()->GetEyePosition().getY() >= 0;
	_renderToTexture = true;

	pCB->BeginRenderPass(_rphReflection, _fbhReflection,
		{
			_tex[TEX_REFLECTION]->GetClearValue(),
			_tex[TEX_REFLECTION_DEPTH]->GetClearValue()
		});
}

void Water::EndReflection(CGI::PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = &(*renderer.GetCommandBuffer());

	pCB->EndRenderPass();

	_reflectionMode = false;
	_renderToTexture = false;
}

void Water::GenerateTextures()
{
	if (!_cshGenHeightmap.IsSet())
	{
		if (_tex[TEX_SOURCE_HEIGHTMAP]->IsLoaded())
			_cshGenHeightmap = _shader[SHADER_GEN]->BindDescriptorSetTextures(1, { _tex[TEX_SOURCE_HEIGHTMAP] });
		else
			return;
	}

	GenerateHeightmapTexture();
	GenerateNormalsTexture();
}

void Water::GenerateHeightmapTexture()
{
	VERUS_QREF_RENDERER;

	s_ubGen._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubGen._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubGenHeightmapFS._phase.x = _phase;
	memcpy(&s_ubGenHeightmapFS._amplitudes, _amplitudes, sizeof(_amplitudes));

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rphGenHeightmap, _fbhGenHeightmap, { _tex[TEX_GEN_HEIGHTMAP]->GetClearValue(), });

	cb->BindPipeline(_pipe[PIPE_GEN_HEIGHTMAP]);

	_shader[SHADER_GEN]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_GEN], 0);
	cb->BindDescriptors(_shader[SHADER_GEN], 1, _cshGenHeightmap);
	_shader[SHADER_GEN]->EndBindDescriptors();

	renderer.DrawQuad(&(*cb));

	cb->EndRenderPass();

	_tex[TEX_GEN_HEIGHTMAP]->GenerateMips();
}

void Water::GenerateNormalsTexture()
{
	VERUS_QREF_RENDERER;

	s_ubGen._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubGen._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubGenNormalsFS._textureSize = _tex[TEX_GEN_HEIGHTMAP]->GetSize().GLM();
	s_ubGenNormalsFS._waterScale.x = 1 / _patchSide;

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rphGenNormals, _fbhGenNormals, { _tex[TEX_GEN_NORMALS]->GetClearValue(), });

	cb->BindPipeline(_pipe[PIPE_GEN_NORMALS]);

	_shader[SHADER_GEN]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_GEN], 0);
	cb->BindDescriptors(_shader[SHADER_GEN], 2, _cshGenNormals);
	_shader[SHADER_GEN]->EndBindDescriptors();

	renderer.DrawQuad(&(*cb));

	cb->EndRenderPass();

	_tex[TEX_GEN_NORMALS]->GenerateMips();
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
	VERUS_QREF_SM;

	const float maxRadius = Math::Min(sm.GetCamera()->GetZFar(), 5000.f);
	const float stepW = 1.f / (_gridWidth - 1);
	const float stepH = 1.f / (_gridHeight - 1);

	float xPolar = 0;
	float zPolar = 0;

	Vector<glm::vec4> vVB;
	vVB.resize(_gridWidth * _gridHeight);

	const float maxAngle = atan(maxRadius / 10); // Assume best height is 10 meters.
	VERUS_FOR(i, _gridHeight)
	{
		const float radius = tan(zPolar * maxAngle) * 10;
		VERUS_FOR(j, _gridWidth)
		{
			float xSqueeze = xPolar;
			VERUS_FOR(k, 6)
				xSqueeze = Math::EaseInOutSine(xSqueeze);
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
