// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

Atmosphere::UB_PerFrame      Atmosphere::s_ubPerFrame;
Atmosphere::UB_PerMaterialFS Atmosphere::s_ubPerMaterialFS;
Atmosphere::UB_PerMeshVS     Atmosphere::s_ubPerMeshVS;
Atmosphere::UB_PerObject     Atmosphere::s_ubPerObject;

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
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_UTILS;

	_skyDome.Init("[Models]:SkyDome.x3d");

	_shader.Init("[Shaders]:Sky.hlsl");
	_shader->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame), 100);
	_shader->CreateDescriptorSet(1, &s_ubPerMaterialFS, sizeof(s_ubPerMaterialFS), 100,
		{
			CGI::Sampler::aniso,
			CGI::Sampler::aniso,
			CGI::Sampler::aniso,
			CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	_shader->CreateDescriptorSet(2, &s_ubPerMeshVS, sizeof(s_ubPerMeshVS), 100, {}, CGI::ShaderStageFlags::vs);
	_shader->CreateDescriptorSet(3, &s_ubPerObject, sizeof(s_ubPerObject), 100);
	_shader->CreatePipelineLayout();

	if (settings._sceneShadowQuality > App::Settings::ShadowQuality::none)
		_shadowMap.Init(4096);

	renderer.GetDS().InitByAtmosphere(_shadowMap.GetTexture());
	Effects::Bloom::I().InitByAtmosphere(_shadowMap.GetTexture());

	CreateCelestialBodyMesh();

	{
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#Sun", renderer.GetDS().GetRenderPassHandle_ExtraCompose());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._depthWriteEnable = false;
		pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
		_pipe[PIPE_SUN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(_geo, _shader, "#Moon", renderer.GetDS().GetRenderPassHandle_ExtraCompose());
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

	_fog._density[0] = _fog._density[1] = 0.001f;

	_sun._matTilt = Matrix3::rotationX(_sun._latitude);
	UpdateSun(_time);
}

void Atmosphere::Done()
{
	if (_shader)
	{
		_shader->FreeDescriptorSet(_cshMoonFS);
		_shader->FreeDescriptorSet(_cshSunFS);
		_shader->FreeDescriptorSet(_cshSkyFS);
	}
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
		_sun._color = glm::saturation(0.1f, _sun._color.GLM()) * 0.5f;
	}

	// Reduce light's intensity when near horizon:
	_sun._alpha = Math::Clamp<float>(abs(_sun._dirTo.getY()) * 5, 0, 1);
	_sun._color *= _sun._alpha;

	if (_sun._light)
	{
		_sun._light->SetColor(Vector4(_sun._color, 1));
		_sun._light->SetDirection(-_sun._dirTo);
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
				allTexLoaded = false;
		}
		if (_skyDome.IsLoaded() && allTexLoaded)
		{
			_async_loaded = true;

			{
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Sky", renderer.GetDS().GetRenderPassHandle_ExtraCompose());
				pipeDesc._vertexInputBindingsFilter = (1 << 0);
				pipeDesc._depthWriteEnable = false;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
				_pipe[PIPE_SKY].Init(pipeDesc);
				pipeDesc._renderPassHandle = water.GetRenderPassHandle();
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
				_pipe[PIPE_SKY_REFLECTION].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Clouds", renderer.GetDS().GetRenderPassHandle_ExtraCompose());
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

	const float cloudinessSq = sqrt(_clouds._cloudiness);
	const float cloudScaleAmb = 1 - 0.6f * _clouds._cloudiness * cloudinessSq;
	const float cloudScaleFog = 1 - 0.9f * _clouds._cloudiness * cloudinessSq;
	const float cloudScaleSun = 1 - 0.999f * _clouds._cloudiness * cloudinessSq;
	float color[3];
	GetColor(208, color, 1); _ambientColor = Vector3::MakeFromPointer(color) * (GetMagnitude(50000, 30000, 1) * cloudScaleAmb);
	GetColor(100, color, 1); _fog._color = Vector3::MakeFromPointer(color) * (GetMagnitude(30000, 2000, 1) * cloudScaleFog);
	GetColor(240, color, 1); _sun._color = Vector3::MakeFromPointer(color) * (GetMagnitude(80000, 10000, 1) * cloudScaleSun);

	glm::vec3 ambientColor = _ambientColor.GLM();
	glm::vec3 fogColor = _fog._color.GLM();
	_ambientColor = glm::saturation(0.7f - 0.3f * _clouds._cloudiness, ambientColor);
	_fog._color = glm::saturation(0.8f - 0.2f * _clouds._cloudiness, fogColor);
	_fog._density[0] = _fog._density[1] * (1 + 9 * _clouds._cloudiness * _clouds._cloudiness);

#ifdef _DEBUG
	if (abs(_time - 0.5f) < 0.01f && glm::epsilonEqual(static_cast<float>(_clouds._cloudiness), 0.25f, 0.05f))
	{
		const glm::vec3 grayAmbient = glm::saturation(0.f, _ambientColor.GLM());
		const glm::vec3 grayFog = glm::saturation(0.f, _fog._color.GLM());
		const glm::vec3 graySun = glm::saturation(0.f, _sun._color.GLM());
		VERUS_RT_ASSERT(glm::epsilonEqual(grayAmbient.x, 6000.f, 640.f));
		VERUS_RT_ASSERT(glm::epsilonEqual(graySun.x, 32000.f, 3200.f));
	}
#endif

	const Vector4 phaseV(_wind._baseVelocity.getX(), _wind._baseVelocity.getZ());
	_clouds._phaseA = Vector4(_clouds._phaseA - phaseV * (dt * _clouds._speedPhaseA * 0.05f)).Mod(1);
	_clouds._phaseB = Vector4(_clouds._phaseB - phaseV * (dt * _clouds._speedPhaseB * 0.05f)).Mod(1);

	UpdateSun(_time);
}

void Atmosphere::DrawSky(bool reflection)
{
	if (!_async_loaded)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_WATER;

	if (water.IsUnderwater())
		reflection = false;

	RcCamera cam = *sm.GetCamera();
	Matrix3 matS = Matrix3::scale(Vector3(4, 1, 4)); // Stretch sky dome.
	const Matrix4 matSkyDome = cam.GetMatrixVP() * Transform3(matS, Vector3(cam.GetEyePosition()));

	auto cb = renderer.GetCommandBuffer();

	s_ubPerFrame._time_cloudiness.x = _time;
	s_ubPerFrame._time_cloudiness.y = _clouds._cloudiness;
	s_ubPerFrame._ambientColor = float4(_ambientColor.GLM(), 0);
	s_ubPerFrame._fogColor = float4(_fog._color.GLM(), 0);
	s_ubPerFrame._dirToSun = float4(_sun._dirTo.GLM(), 0);
	s_ubPerFrame._sunColor = float4(_sun._color.GLM(), _sun._alpha);
	s_ubPerFrame._phaseAB = float4(
		_clouds._phaseA.getX(),
		_clouds._phaseA.getY(),
		_clouds._phaseB.getX(),
		_clouds._phaseB.getY());

	_skyDome.CopyPosDeqScale(&s_ubPerMeshVS._posDeqScale.x);
	_skyDome.CopyPosDeqBias(&s_ubPerMeshVS._posDeqBias.x);

	// <Sky>
	s_ubPerObject._matWVP = matSkyDome.UniformBufferFormat();
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
		s_ubPerObject._matW = matW.UniformBufferFormat();
		s_ubPerObject._matWVP = Matrix4(cam.GetMatrixVP() * matW).UniformBufferFormat();

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
		s_ubPerObject._matW = matW.UniformBufferFormat();
		s_ubPerObject._matWVP = Matrix4(cam.GetMatrixVP() * matW).UniformBufferFormat();

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
	s_ubPerObject._matWVP = matSkyDome.UniformBufferFormat();
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

void Atmosphere::GetColor(int level, float* pOut, float scale)
{
	if (!_texSky.IsLoaded())
		return;
	const float texCoord = _time * 256;
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
		pOut[i] = Math::Clamp<float>(Math::Lerp(rgbA[i], rgbB[i], fractPart) * 0.5f * scale, 0, 1);
}

float Atmosphere::GetMagnitude(float noon, float dusk, float midnight) const
{
	const float time4 = _time * 4;
	const int interval = Math::Clamp(static_cast<int>(time4), 0, 3);
	const float fractPart = time4 - interval;
	const float controlPoints[] = { midnight, dusk, noon, dusk, midnight };
	return Math::Lerp(controlPoints[interval], controlPoints[interval + 1], glm::sineEaseInOut(fractPart));
}

RcVector3 Atmosphere::GetDirToSun() const
{
	return _sun._dirTo;
}

RcVector3 Atmosphere::GetSunColor() const
{
	return _sun._color;
}

float Atmosphere::GetSunAlpha() const
{
	return _sun._alpha;
}

void Atmosphere::OnSceneResetInitSunLight()
{
	VERUS_QREF_SM;
	// This should be called every time new scene is initialized.

	if (_sun._light)
		_sun._light.Detach(); // Clear dangling pointer from the old scene.

	sm.DeleteNode(NodeType::light, "Sun"); // Remove any existing light.

	Light::Desc desc;
	desc._name = "Sun";
	desc._data._lightType = CGI::LightType::dir;
	desc._data._dir = -_sun._dirTo;
	_sun._light.Init(desc);
	_sun._light->SetTransient();
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

void Atmosphere::BeginShadow(int split)
{
	_shadowMap.Begin(_sun._dirTo, split);
}

void Atmosphere::EndShadow(int split)
{
	_shadowMap.End(split);
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
