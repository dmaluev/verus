// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

Atmosphere::UB_View       Atmosphere::s_ubView;
Atmosphere::UB_MaterialFS Atmosphere::s_ubMaterialFS;
Atmosphere::UB_MeshVS     Atmosphere::s_ubMeshVS;
Atmosphere::UB_Object     Atmosphere::s_ubObject;

Atmosphere::Atmosphere()
{
}

Atmosphere::~Atmosphere()
{
	Done();
}

void Atmosphere::Init()
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_UTILS;

	_skyDome.Init("[Models]:SkyDome.x3d");

	_shader.Init("[Shaders]:Sky.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubView, sizeof(s_ubView), settings.GetLimits()._sky_ubViewCapacity);
	_shader->CreateDescriptorSet(1, &s_ubMaterialFS, sizeof(s_ubMaterialFS), settings.GetLimits()._sky_ubMaterialFSCapacity,
		{
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubMeshVS, sizeof(s_ubMeshVS), settings.GetLimits()._sky_ubMeshVSCapacity, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(3, &s_ubObject, sizeof(s_ubObject), settings.GetLimits()._sky_ubObjectCapacity);
	_shader->CreatePipelineLayout();

	_cubeMapBaker.Init();

	CreateCelestialBodyMesh();

	{
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#Sun", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._depthWriteEnable = false;
		pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
		_pipe[PIPE_SUN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#Moon", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._depthWriteEnable = false;
		pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
		_pipe[PIPE_MOON].Init(pipeDesc);
	}

	_texSky.LoadDDS("[Textures]:Sky/Sky.dds");

	_tex[TEX_SKY].Init("[Textures]:Sky/Sky.dds");
	_tex[TEX_STARS].Init("[Textures]:Sky/Stars.dds");
	_tex[TEX_SUN].Init("[Textures]:Sky/Sun.dds");
	_tex[TEX_MOON].Init("[Textures]:Sky/Moon.dds");
	_tex[TEX_CLOUDS].Init("[Textures]:Sky/Clouds.FX.dds");
	_tex[TEX_CLOUDS_NM].Init("[Textures]:Sky/Clouds.NM.dds");

	_clouds._phaseA.setX(utils.GetRandom().NextFloat());
	_clouds._phaseA.setY(utils.GetRandom().NextFloat());
	_clouds._phaseB.setX(utils.GetRandom().NextFloat());
	_clouds._phaseB.setY(utils.GetRandom().NextFloat());

	_fog._density[0] = _fog._density[1] = 0.0005f;

	_sun._matTilt = Matrix3::rotationX(_sun._latitude);
	UpdateSun(_time);
}

void Atmosphere::Done()
{
	VERUS_DONE(Atmosphere);
}

void Atmosphere::UpdateSun(float time)
{
	// Update time and sun's color before calling this method.

	_sun._dirTo = _sun._matTilt * Matrix3::rotationZ(time * VERUS_2PI) * Vector3(0, -1, 0);
	_night = false;
	if (_sun._dirTo.getY() < 0) // Moon light:
	{
		_night = true;
		_sun._dirTo = _sun._matTilt * Matrix3::rotationZ(time * VERUS_2PI) * Vector3(0, 1, 0);
	}

	// Reduce light's intensity when near horizon:
	_sun._alpha = Math::Clamp<float>(abs(_sun._dirTo.getY()) * 4, 0, 1) * (_night ? 0.25f : 1.f);
	_sun._color *= _sun._alpha;

	if (_sun._lightNode)
	{
		_sun._lightNode->SetColor(Vector4(_sun._color, 1));
		_sun._lightNode->SetDirection(-_sun._dirTo);
	}
}

void Atmosphere::UpdateWind()
{
	VERUS_QREF_TIMER;

	const float baseWindSpeed = VMath::length(_wind._baseVelocity);
	const float baseWindStrength = Math::Clamp<float>(baseWindSpeed * (1 / 50.f), 0, 1);

	const glm::vec2 jounceDir = glm::circularRand(1.f);
	const glm::vec2 jounce = jounceDir * 100.f;
	_wind._jerk += Vector3(jounce.x, 0, jounce.y) * dt;
	_wind._accel += _wind._jerk * dt;
	_wind._velocity += _wind._accel * dt;

	auto Stabilize = [dt](RVector3 v, RcVector3 base, float power)
	{
		const Vector3 toBase = base - v;
		const float toBaseLen = VMath::length(toBase);
		if (toBaseLen >= 0.01f)
		{
			const float scale = pow(toBaseLen, power) * dt;
			v += (toBase / toBaseLen) * Math::Min(scale, toBaseLen);
		}
	};
	Stabilize(_wind._jerk, Vector3(0), 0.9f - 0.6f * baseWindStrength);
	Stabilize(_wind._accel, Vector3(0), 1.1f - 0.6f * baseWindStrength);
	Stabilize(_wind._velocity, _wind._baseVelocity, 2 - 0.5f * baseWindStrength);

	_wind._speed = VMath::length(_wind._velocity);
	if (_wind._speed >= VERUS_FLOAT_THRESHOLD)
		_wind._direction = _wind._velocity / _wind._speed;
	else
		_wind._direction = Vector3(1, 0, 0);

	const float angle = Math::Clamp<float>(_wind._speed * (1 / 80.f), 0, VERUS_PI * 0.25f);
	const Vector3 axis = VMath::cross(Vector3(0, 1, 0), _wind._velocity);
	if (VMath::lengthSqr(axis) >= VERUS_FLOAT_THRESHOLD * VERUS_FLOAT_THRESHOLD)
		_wind._matPlantBending = Matrix3::rotation(angle, VMath::normalizeApprox(axis));
	else
		_wind._matPlantBending = Matrix3::identity();
}

void Atmosphere::Update()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;
	VERUS_QREF_WATER;

	if (!_async_loaded)
	{
		bool allTexLoaded = true;
		VERUS_FOR(i, TEX_COUNT)
		{
			if (!_tex[i]->IsLoaded())
			{
				allTexLoaded = false;
				break;
			}
		}
		if (_skyDome.IsLoaded() && allTexLoaded)
		{
			_async_loaded = true;

			{
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Sky", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
				pipeDesc._vertexInputBindingsFilter = (1 << 0);
				pipeDesc._depthWriteEnable = false;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
				_pipe[PIPE_SKY].Init(pipeDesc);
				pipeDesc._renderPassHandle = water.GetRenderPassHandle();
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
				_pipe[PIPE_SKY_REFLECTION].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Clouds", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
				pipeDesc._vertexInputBindingsFilter = (1 << 0);
				pipeDesc._depthWriteEnable = false;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
				_pipe[PIPE_CLOUDS].Init(pipeDesc);
				pipeDesc._renderPassHandle = water.GetRenderPassHandle();
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
				_pipe[PIPE_CLOUDS_REFLECTION].Init(pipeDesc);
			}

			_shader->FreeDescriptorSet(_cshSkyFS);
			_cshSkyFS = _shader->BindDescriptorSetTextures(1, { _tex[TEX_SKY], _tex[TEX_STARS], _tex[TEX_CLOUDS], _tex[TEX_CLOUDS_NM] });
			_shader->FreeDescriptorSet(_cshSunFS);
			_cshSunFS = _shader->BindDescriptorSetTextures(1, { _tex[TEX_SKY], _tex[TEX_STARS], _tex[TEX_SUN], _tex[TEX_CLOUDS_NM] });
			_shader->FreeDescriptorSet(_cshMoonFS);
			_cshMoonFS = _shader->BindDescriptorSetTextures(1, { _tex[TEX_SKY], _tex[TEX_STARS], _tex[TEX_MOON], _tex[TEX_CLOUDS_NM] });
		}
	}
	if (!_async_loaded)
		return;

	UpdateWind();

	_time = glm::fract(_time + dt * _timeSpeed);

	_clouds._cloudiness.Update();

	const float cloudinessSq = _clouds._cloudiness * _clouds._cloudiness;
	const float cloudScaleAmb = 1 - 0.9f * _clouds._cloudiness;
	const float cloudScaleSun = 1 - 0.9999f * cloudinessSq;
	const int count = 1;
	std::stringstream ssDebug;
	VERUS_FOR(i, count)
	{
		const float time = (count > 1) ? (i / 256.f) : _time;
		_ambientMagnitude = GetMagnitude(time, 28000, 5000, 1000) * cloudScaleAmb;
		float color[3];
		SampleSkyColor(time, 200, color); _ambientColorY1 = Vector3::MakeFromPointer(color) * _ambientMagnitude;
		SampleSkyColor(time, 216, color); _ambientColorY0 = Vector3::MakeFromPointer(color) * _ambientMagnitude;
		SampleSkyColor(time, 232, color); _ambientColor = Vector3::MakeFromPointer(color) * _ambientMagnitude;
		SampleSkyColor(time, 248, color); _sun._color = Vector3::MakeFromPointer(color) * (GetMagnitude(time, 40000, 20000, 1000) * cloudScaleSun * cloudScaleSun);
		if (count > 1)
			ssDebug << static_cast<int>(glm::saturation(0.f, _ambientColor.GLM()).x) << ", ";
	}
	const String debug = ssDebug.str();

	// <Cloudiness>
	_ambientColor = glm::saturation(1 - 0.3f * _clouds._cloudiness, _ambientColor.GLM());
	_ambientColorY0 = glm::saturation(1 - 0.3f * _clouds._cloudiness, _ambientColorY0.GLM());
	_ambientColorY1 = glm::saturation(1 - 0.3f * _clouds._cloudiness, _ambientColorY1.GLM());
	// </Cloudiness>

	// <Clouds>
	const Vector4 phaseV(_wind._baseVelocity.getX(), _wind._baseVelocity.getZ());
	_clouds._phaseA = Vector4(_clouds._phaseA - phaseV * (dt * _clouds._speedPhaseA * 0.05f)).Mod(1);
	_clouds._phaseB = Vector4(_clouds._phaseB - phaseV * (dt * _clouds._speedPhaseB * 0.05f)).Mod(1);
	// </Clouds>

	UpdateSun(_time);

	// <Fog>
	_fog._color = VMath::lerp(0.1f, _ambientColor, _sun._color) * 0.75f;
	_fog._color = glm::saturation(1 - 0.2f * cloudinessSq, _fog._color.GLM());
	_fog._density[0] = _fog._density[1] * (1 + 9 * cloudinessSq);
	// </Fog>
}

void Atmosphere::DrawSky(bool reflection)
{
	if (!_async_loaded)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;
	VERUS_QREF_WATER;

	if (water.IsUnderwater())
		reflection = false;

	RcCamera cam = *wm.GetPassCamera();
	Matrix3 matS = Matrix3::scale(Vector3(4, 1, 4)); // Stretch sky dome.
	const Matrix4 matSkyDome = cam.GetMatrixVP() * Transform3(matS, Vector3(cam.GetEyePosition()));

	auto cb = renderer.GetCommandBuffer();

	s_ubView._time_cloudiness.x = _time;
	s_ubView._time_cloudiness.y = _clouds._cloudiness;
	s_ubView._ambientColor = float4(_ambientColor.GLM(), _ambientMagnitude);
	s_ubView._fogColor = float4(_fog._color.GLM(), _fog._density[0]);
	s_ubView._dirToSun = float4(_sun._dirTo.GLM(), 0);
	s_ubView._sunColor = float4(_sun._color.GLM(), _sun._alpha);
	s_ubView._phaseAB = float4(
		_clouds._phaseA.getX(),
		_clouds._phaseA.getY(),
		_clouds._phaseB.getX(),
		_clouds._phaseB.getY());

	_skyDome.CopyPosDeqScale(&s_ubMeshVS._posDeqScale.x);
	_skyDome.CopyPosDeqBias(&s_ubMeshVS._posDeqBias.x);

	// <Sky>
	s_ubObject._matWVP = matSkyDome.UniformBufferFormat();
	cb->BindPipeline(_pipe[reflection ? PIPE_SKY_REFLECTION : PIPE_SKY]);
	_skyDome.BindGeo(cb);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshSkyFS);
	cb->BindDescriptors(_shader, 2);
	cb->BindDescriptors(_shader, 3);
	_shader->EndBindDescriptors();
	cb->DrawIndexed(_skyDome.GetIndexCount());
	// </Sky>

	const float celestialBodyScale = 0.02f + 0.02f * (1 - abs(_sun._dirTo.getY()));
	matS = Matrix3::scale(Vector3(celestialBodyScale, 1, celestialBodyScale));

	// <Sun>
	{
		const Matrix3 matR = Matrix3::rotationZ(_time * VERUS_2PI);
		const Transform3 matW = Transform3(_sun._matTilt * matR * matS, Vector3(cam.GetEyePosition()));
		s_ubObject._matW = matW.UniformBufferFormat();
		s_ubObject._matWVP = Matrix4(cam.GetMatrixVP() * matW).UniformBufferFormat();

		cb->BindPipeline(_pipe[PIPE_SUN]);
		cb->BindVertexBuffers(_geo);
		_shader->BeginBindDescriptors();
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _cshSunFS);
		cb->BindDescriptors(_shader, 2);
		cb->BindDescriptors(_shader, 3);
		_shader->EndBindDescriptors();
		cb->Draw(4);
	}
	// </Sun>

	// <Moon>
	{
		const Matrix3 matR = Matrix3::rotationZ(_time * VERUS_2PI + VERUS_PI);
		const Transform3 matW = Transform3(_sun._matTilt * matR * matS, Vector3(cam.GetEyePosition()));
		s_ubObject._matW = matW.UniformBufferFormat();
		s_ubObject._matWVP = Matrix4(cam.GetMatrixVP() * matW).UniformBufferFormat();

		cb->BindPipeline(_pipe[PIPE_MOON]);
		cb->BindVertexBuffers(_geo);
		_shader->BeginBindDescriptors();
		cb->BindDescriptors(_shader, 0);
		cb->BindDescriptors(_shader, 1, _cshMoonFS);
		cb->BindDescriptors(_shader, 2);
		cb->BindDescriptors(_shader, 3);
		_shader->EndBindDescriptors();
		cb->Draw(4);
	}
	// </Moon>

	// <Clouds>
	s_ubObject._matWVP = matSkyDome.UniformBufferFormat();
	cb->BindPipeline(_pipe[reflection ? PIPE_CLOUDS_REFLECTION : PIPE_CLOUDS]);
	_skyDome.BindGeo(cb);
	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshSkyFS);
	cb->BindDescriptors(_shader, 2);
	cb->BindDescriptors(_shader, 3);
	_shader->EndBindDescriptors();
	cb->DrawIndexed(_skyDome.GetIndexCount());
	// </Clouds>
}

void Atmosphere::SampleSkyColor(float time, int level, float* pOut)
{
	if (!_texSky.IsLoaded())
		return;
	const float texCoord = time * 256;
	const int intPartA = static_cast<int>(texCoord) & 0xFF;
	const int intPartB = (intPartA + 1) & 0xFF;
	const float fractPart = texCoord - intPartA;

	const int offsetA = (level << 10) + (intPartA << 2);
	const int offsetB = (level << 10) + (intPartB << 2);
	auto LoadFromSky = [this](float rgb[3], int offset)
	{
		rgb[0] = Convert::SRGBToLinear(Convert::Uint8ToUnorm(_texSky.GetData()[offset + 0]));
		rgb[1] = Convert::SRGBToLinear(Convert::Uint8ToUnorm(_texSky.GetData()[offset + 1]));
		rgb[2] = Convert::SRGBToLinear(Convert::Uint8ToUnorm(_texSky.GetData()[offset + 2]));
	};
	float rgbA[3];
	float rgbB[3];
	LoadFromSky(rgbA, offsetA);
	LoadFromSky(rgbB, offsetB);

	VERUS_FOR(i, 3)
		pOut[i] = Math::Lerp(rgbA[i], rgbB[i], fractPart);
}

float Atmosphere::GetMagnitude(float time, float noon, float dusk, float midnight)
{
	const float time4 = time * 4;
	const int interval = Math::Clamp(static_cast<int>(time4), 0, 3);
	const float fractPart = time4 - interval;
	const float cp[] = { midnight, dusk, noon, dusk };
	const int indices[4][4] =
	{
		{3, 0, 1, 2},
		{0, 1, 2, 3},
		{1, 2, 3, 0},
		{2, 3, 0, 1}
	};
	return glm::catmullRom(
		glm::vec1(cp[indices[interval][0]]),
		glm::vec1(cp[indices[interval][1]]),
		glm::vec1(cp[indices[interval][2]]),
		glm::vec1(cp[indices[interval][3]]),
		fractPart).x;
}

RcVector3 Atmosphere::GetDirToSun() const
{
	return _sun._dirTo;
}

Vector3 Atmosphere::GetSunShadowMapUpDir() const
{
	return _sun._matTilt.getCol2();
}

RcVector3 Atmosphere::GetSunColor() const
{
	return _sun._color;
}

float Atmosphere::GetSunAlpha() const
{
	return _sun._alpha;
}

void Atmosphere::OnWorldResetInitSunLight()
{
	VERUS_QREF_WM;
	// This should be called every time new world is initialized.

	if (_sun._lightNode)
		_sun._lightNode.Detach(); // Clear dangling pointer from the old world.

	wm.DeleteNode(NodeType::light, "Sun"); // Remove any existing light.

	LightNode::Desc desc;
	desc._name = "Sun";
	desc._data._lightType = CGI::LightType::dir;
	desc._data._dir = -_sun._dirTo;
	_sun._lightNode.Init(desc);
	_sun._lightNode->SetGeneratedFlag();
}

void Atmosphere::SetLatitude(float lat)
{
	_sun._latitude = lat;
	_sun._matTilt = Matrix3::rotationX(_sun._latitude);
}

RcMatrix3 Atmosphere::GetPlantBendingMatrix() const
{
	return _wind._matPlantBending;
}

RcVector3 Atmosphere::GetBaseWindVelocity() const
{
	return _wind._baseVelocity;
}

void Atmosphere::SetBaseWindVelocity(RcVector3 v)
{
	_wind._baseVelocity = v;
}

RcVector3 Atmosphere::GetWindVelocity() const
{
	return _wind._velocity;
}

RcVector3 Atmosphere::GetWindDirection() const
{
	return _wind._direction;
}

float Atmosphere::GetWindSpeed() const
{
	return _wind._speed;
}

void Atmosphere::CreateCelestialBodyMesh()
{
	CGI::GeometryDesc geoDesc;
	geoDesc._name = "Atmosphere.Geo";
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(Vertex, _pos), CGI::ViaType::floats, 3, CGI::ViaUsage::position, 0},
		{0, offsetof(Vertex, _tc),  CGI::ViaType::shorts, 2, CGI::ViaUsage::texCoord, 0},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	const Vertex skyBody[] =
	{
		{-1, -1, -1, 1, 0},
		{-1, -1, +1, 0, 0},
		{+1, -1, -1, 1, 1},
		{+1, -1, +1, 0, 1},
	};
	_geo->CreateVertexBuffer(4, 0);
	_geo->UpdateVertexBuffer(skyBody, 0);
}

void Atmosphere::GetReport(RReport report)
{
	report._time = _time;
	report._speed = _timeSpeed;
	report._cloudiness = _clouds._cloudiness.GetTarget();
}

void Atmosphere::SetReport(RcReport report)
{
	_time = report._time;
	_timeSpeed = report._speed;
	_clouds._cloudiness = report._cloudiness;
}
