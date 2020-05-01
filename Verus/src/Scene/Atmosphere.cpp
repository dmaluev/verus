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

	_sun._matTilt = Matrix3::rotationX(-VERUS_PI / 10);
	_sun._dirToMidnight = _sun._matTilt * Vector3(0, -1, 0);
	_sun._dirTo = VMath::mulPerElem(_sun._dirToMidnight, Vector3(1, -1, 1));
}

void Atmosphere::Done()
{
	_shader->FreeDescriptorSet(_cshSkyFS);
	VERUS_DONE(Atmosphere);
}

void Atmosphere::UpdateSun(float time)
{
	// Update time and sun's color before calling this method.

	_sun._dirTo = Matrix3::rotationZ(time * VERUS_2PI) * _sun._dirToMidnight;

	_night = false;
	if (_sun._dirTo.getY() < 0) // Moon light:
	{
		_night = true;
		_sun._dirTo = Matrix3::rotationZ(time * VERUS_2PI + VERUS_PI) * _sun._dirToMidnight;
		_sun._color = VMath::mulPerElem(_sun._color, Vector3(0.19f, 0.21f, 0.45f));
	}

	// Reduce light's intensity when near horizon:
	const float intensity = Math::Clamp<float>(abs(_sun._dirTo.getY()) * 4, 0, 1);
	_sun._color *= intensity;
}

void Atmosphere::Update()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

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
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Sky", renderer.GetDS().GetRenderPassHandle_Compose(), renderer.GetDS().GetSubpass_Compose());
				pipeDesc._vertexInputBindingsFilter = (1 << 0);
				pipeDesc._depthWriteEnable = false;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
				_pipe[PIPE_SKY].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_skyDome.GetGeometry(), _shader, "#Clouds", renderer.GetDS().GetRenderPassHandle_Compose(), renderer.GetDS().GetSubpass_Compose());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
				pipeDesc._vertexInputBindingsFilter = (1 << 0);
				pipeDesc._depthWriteEnable = false;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
				_pipe[PIPE_CLOUDS].Init(pipeDesc);
			}

			_shader->FreeDescriptorSet(_cshSkyFS);
			_cshSkyFS = _shader->BindDescriptorSetTextures(1, { _tex[TEX_SKY], _tex[TEX_STARS], _tex[TEX_CLOUDS], _tex[TEX_CLOUDS_NM] });
		}
	}
	if (!_async_loaded)
		return;

	_time = glm::fract(_time + dt * _timeSpeed);

	_clouds._cloudiness.Update();

	float color[3];
	GetColor(64, color, 1.5f);  _fog._color = Vector3::MakeFromPointer(color) * _hdrScale;
	GetColor(208, color, 0.3f); _ambientColor = Vector3::MakeFromPointer(color) * _hdrScale;
	GetColor(240, color, 1);    _sun._color = Vector3::MakeFromPointer(color) * _hdrScale;

	glm::vec3 ambientColor = _ambientColor.GLM();
	glm::vec3 fogColor = _fog._color.GLM();
	_ambientColor = glm::saturation(0.75f, ambientColor);
	const float clearSky = Math::Min(1.f, 1.5f - _clouds._cloudiness);
	_fog._color = glm::saturation(clearSky * 0.75f, fogColor);
	_fog._colorInit = _fog._color;

	UpdateSun(_time);
}

void Atmosphere::DrawSky()
{
	if (!_async_loaded)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	auto cb = renderer.GetCommandBuffer();

	RcCamera cam = *sm.GetCamera();
	Matrix3 matS = Matrix3::scale(Vector3(4, 1, 4)); // Stretch sky dome.
	const Matrix4 matSkyDome = cam.GetMatrixVP() * Transform3(matS, Vector3(cam.GetEyePosition()));

	s_ubPerFrame._time_cloudiness_expo.x = _time;
	s_ubPerFrame._time_cloudiness_expo.y = _clouds._cloudiness;
	s_ubPerFrame._time_cloudiness_expo.z = 0;
	s_ubPerFrame._dirToSun = float4(_sun._dirTo.GLM(), 0);
	s_ubPerFrame._phaseAB = float4(
		_clouds._phaseA.getX(),
		_clouds._phaseA.getY(),
		_clouds._phaseB.getX(),
		_clouds._phaseB.getY());
	s_ubPerFrame._fogColor = float4(_fog._color.GLM(), 0);
	_skyDome.CopyPosDeqScale(&s_ubPerMeshVS._posDeqScale.x);
	_skyDome.CopyPosDeqBias(&s_ubPerMeshVS._posDeqBias.x);
	s_ubPerObject._matWVP = matSkyDome.UniformBufferFormat();

	_skyDome.BindGeo(cb);

	// <Sky>
	cb->BindPipeline(_pipe[PIPE_SKY]);

	_shader->BeginBindDescriptors();
	cb->BindDescriptors(_shader, 0);
	cb->BindDescriptors(_shader, 1, _cshSkyFS);
	cb->BindDescriptors(_shader, 2);
	cb->BindDescriptors(_shader, 3);
	_shader->EndBindDescriptors();

	cb->DrawIndexed(_skyDome.GetIndexCount());
	// </Sky>

	// <Clouds>
	cb->BindPipeline(_pipe[PIPE_CLOUDS]);

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

RcVector3 Atmosphere::GetDirToSun() const
{
	return _sun._dirTo;
}

RcVector3 Atmosphere::GetSunColor() const
{
	return _sun._color;
}

void Atmosphere::BeginShadow(int split)
{
	_shadowMap.Begin(_sun._dirTo, 1, 0, split);
}

void Atmosphere::EndShadow(int split)
{
	_shadowMap.End(split);
}

RcPoint3 Atmosphere::GetEyePosition(PVector3 pDirFront)
{
	VERUS_QREF_SM;
	if (_shadowMap.GetSceneCamera())
	{
		if (pDirFront)
			*pDirFront = _shadowMap.GetSceneCamera()->GetFrontDirection();
		return _shadowMap.GetSceneCamera()->GetEyePosition();
	}
	else
	{
		if (pDirFront)
			*pDirFront = sm.GetCamera()->GetFrontDirection();
		return sm.GetCamera()->GetEyePosition();
	}
}
