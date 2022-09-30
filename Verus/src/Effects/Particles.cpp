// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Effects;

CGI::ShaderPwn                           Particles::s_shader;
CGI::PipelinePwns<Particles::PIPE_COUNT> Particles::s_pipe;
Particles::UB_ParticlesVS                Particles::s_ubParticlesVS;
Particles::UB_ParticlesFS                Particles::s_ubParticlesFS;

Particles::Particles()
{
	VERUS_CT_ASSERT(32 == sizeof(Vertex));
}

Particles::~Particles()
{
	Done();
}

void Particles::InitStatic()
{
	VERUS_QREF_CONST_SETTINGS;

	s_shader.Init("[Shaders]:Particles.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubParticlesVS, sizeof(s_ubParticlesVS), settings.GetLimits()._particles_ubVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader->CreateDescriptorSet(1, &s_ubParticlesFS, sizeof(s_ubParticlesFS), settings.GetLimits()._particles_ubFSCapacity,
		{
			CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreatePipelineLayout();
}

void Particles::DoneStatic()
{
	s_pipe.Done();
	s_shader.Done();
}

void Particles::Init(CSZ url)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_url = url;

	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	_capacity = root.child("capacity").text().as_int();

	if (auto node = root.child("type"))
	{
		Str type = node.text().as_string();
		if (type == "none")               _billboardType = BillboardType::none;
		else if (type == "screen-vplane") _billboardType = BillboardType::screenViewplaneOriented;
		else if (type == "screen-vpoint") _billboardType = BillboardType::screenViewpointOriented;
		else if (type == "world-vplane")  _billboardType = BillboardType::worldViewplaneOriented;
		else if (type == "world-vpoint")  _billboardType = BillboardType::worldViewpointOriented;
		else if (type == "axial")         _billboardType = BillboardType::axial;
		else
		{
			VERUS_RT_FAIL("Invalid particle type.");
		}
	}

	if (auto node = root.child("texture"))
		_tex.Init(node.text().as_string());

	if (auto node = root.child("tilesetSize"))
	{
		_tilesetX = node.attribute("x").as_int(_tilesetX);
		_tilesetY = node.attribute("y").as_int(_tilesetY);
		_tilesetSize = Vector4(
			static_cast<float>(_tilesetX),
			static_cast<float>(_tilesetY),
			1.f / _tilesetX,
			1.f / _tilesetY);
	}

	if (auto node = root.child("lifeTime"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetLifeTime(mn, mx);
	}

	if (auto node = root.child("speed"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetSpeed(mn, mx);
	}

	if (auto node = root.child("beginAdditive"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetBeginAdditive(mn, mx);
	}
	if (auto node = root.child("endAdditive"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetEndAdditive(mn, mx);
	}

	if (auto node = root.child("beginColor"))
	{
		Vector4 mn, mx;
		mn.FromColorString(node.attribute("min").value());
		mx.FromColorString(node.attribute("max").value());
		SetBeginColor(mn, mx);
	}
	if (auto node = root.child("endColor"))
	{
		Vector4 mn, mx;
		mn.FromColorString(node.attribute("min").value());
		mx.FromColorString(node.attribute("max").value());
		SetEndColor(mn, mx);
	}

	if (auto node = root.child("beginSize"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetBeginSize(mn, mx);
	}
	if (auto node = root.child("endSize"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetEndSize(mn, mx);
	}

	if (auto node = root.child("beginSpin"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetBeginSpin(mn, mx);
	}
	if (auto node = root.child("endSpin"))
	{
		const float mn = node.attribute("min").as_float();
		const float mx = node.attribute("max").as_float();
		SetEndSpin(mn, mx);
	}

	_brightness = root.child("brightness").text().as_float(_brightness);
	_bounceStrength = root.child("bounce").text().as_float(_bounceStrength);
	_gravityStrength = root.child("gravity").text().as_float(_gravityStrength);
	_windStrength = root.child("wind").text().as_float(_windStrength);
	_collide = root.child("collide").text().as_bool(_collide);
	_decal = root.child("decal").text().as_bool(_decal);
	_particlesPerSecond = root.child("particlesPerSecond").text().as_float(_particlesPerSecond);
	_zone = root.child("zone").text().as_float(_zone);

	_vParticles.resize(_capacity);

	if (BillboardType::none == _billboardType) // Use point sprites?
	{
		_vVB.resize(_capacity);

		CGI::GeometryDesc geoDesc;
		geoDesc._name = "Particles.Geo";
		const CGI::VertexInputAttrDesc viaDesc[] =
		{
			{0, offsetof(Vertex, pos),		/**/CGI::ViaType::floats, 4, CGI::ViaUsage::position, 0},
			{0, offsetof(Vertex, tc0),		/**/CGI::ViaType::floats, 2, CGI::ViaUsage::texCoord, 0},
			{0, offsetof(Vertex, color),	/**/CGI::ViaType::ubytes, 4, CGI::ViaUsage::color, 0},
			{0, offsetof(Vertex, psize),	/**/CGI::ViaType::floats, 1, CGI::ViaUsage::psize, 0},
			CGI::VertexInputAttrDesc::End()
		};
		geoDesc._pVertexInputAttrDesc = viaDesc;
		const int strides[] = { sizeof(Vertex), 0 };
		geoDesc._pStrides = strides;
		geoDesc._dynBindingsMask = 0x1;
		_geo.Init(geoDesc);
		_geo->CreateVertexBuffer(_capacity, 0);

		if (!s_pipe[PIPE_MAIN])
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader, "#", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_PA;
			pipeDesc._topology = CGI::PrimitiveTopology::pointList;
			pipeDesc._depthWriteEnable = false;
			s_pipe[PIPE_MAIN].Init(pipeDesc);
		}
	}
	else // Use billboards?
	{
		_capacity = Math::Min(_capacity * 4, 65536) / 4;
		_vVB.resize(_capacity * 4); // Each sprite has 4 vertices.
		_indexCount = _capacity * 6;
		_vIB.resize(_indexCount);

		CGI::GeometryDesc geoDesc;
		geoDesc._name = "Particles.Geo";
		const CGI::VertexInputAttrDesc viaDesc[] =
		{
			{0, offsetof(Vertex, pos),		/**/CGI::ViaType::floats, 4, CGI::ViaUsage::position, 0},
			{0, offsetof(Vertex, tc0),		/**/CGI::ViaType::floats, 2, CGI::ViaUsage::texCoord, 0},
			{0, offsetof(Vertex, color),	/**/CGI::ViaType::ubytes, 4, CGI::ViaUsage::color, 0},
			{0, offsetof(Vertex, psize),	/**/CGI::ViaType::floats, 1, CGI::ViaUsage::psize, 0},
			CGI::VertexInputAttrDesc::End()
		};
		geoDesc._pVertexInputAttrDesc = viaDesc;
		const int strides[] = { sizeof(Vertex), 0 };
		geoDesc._pStrides = strides;
		geoDesc._dynBindingsMask = 0x1 | (1 << 31);
		_geo.Init(geoDesc);
		_geo->CreateVertexBuffer(_capacity * 4, 0);
		_geo->CreateIndexBuffer(_indexCount);

		if (!s_pipe[PIPE_BILLBOARDS])
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader, "#Billboards", renderer.GetDS().GetRenderPassHandle_ForwardRendering());
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_PA;
			pipeDesc._depthWriteEnable = false;
			s_pipe[PIPE_BILLBOARDS].Init(pipeDesc);
		}
	}

	_ratio = static_cast<float>(_tilesetY) / static_cast<float>(_tilesetX);
}

void Particles::Done()
{
	VERUS_DONE(Particles);
}

void Particles::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	_drawCount = 0;

	if (!_csh.IsSet() && _tex->IsLoaded())
		_csh = s_shader->BindDescriptorSetTextures(1, { _tex });

	VERUS_QREF_TIMER;

	Vector3 wind = Vector3(0);
	if (World::Atmosphere::IsValidSingleton())
	{
		VERUS_QREF_ATMO;
		wind = atmo.GetWindVelocity();
	}

	if (BillboardType::none == _billboardType)
	{
		VERUS_FOR(i, _capacity)
		{
			if (_vParticles[i]._timeLeft >= 0)
			{
				_vParticles[i]._timeLeft -= dt;
				if (_vParticles[i]._timeLeft < 0)
				{
					_vParticles[i]._timeLeft = -1;
					continue;
				}

				if (_pDelegate)
				{
					Vector3 up, normal;
					Transform3 matrix;
					_pDelegate->Particles_OnUpdate(*this, i, _vParticles[i], up, normal, matrix);
				}
				else
				{
					Point3 hitPoint;
					Vector3 hitNormal;
					TimeCorrectedVerletIntegration(_vParticles[i], hitPoint, hitNormal);

					const float t = glm::quadraticEaseInOut(1 - _vParticles[i]._timeLeft * _vParticles[i]._invTotalTime);
					const Vector4 color = VMath::lerp(t, _vParticles[i]._beginColor, _vParticles[i]._endColor);
					const float size = Math::Lerp(_vParticles[i]._beginSize, _vParticles[i]._endSize, t);
					const float additive = Math::Lerp(_vParticles[i]._beginAdditive, _vParticles[i]._endAdditive, t);

					PushPointSprite(i, color, size, additive);
				}
			}
		}
		if (_drawCount)
			_geo->UpdateVertexBuffer(_vVB.data(), 0, nullptr, _drawCount);
	}
	else // Billboards:
	{
		VERUS_QREF_WM;
		World::PCamera pHeadCamera = wm.GetHeadCamera();
		Vector3 normal = pHeadCamera->GetFrontDirection();
		Vector3 up(0, 1, 0);
		switch (_billboardType)
		{
		case BillboardType::screenViewplaneOriented:
		case BillboardType::screenViewpointOriented:
		{
			up = pHeadCamera->GetUpDirection();
		}
		break;
		case BillboardType::worldViewplaneOriented:
		case BillboardType::worldViewpointOriented:
		{
			up = Vector3(0, 1, 0);
			const Vector3 right = VMath::cross(up, normal);
			normal = VMath::normalizeApprox(VMath::cross(right, up));
		}
		break;
		}

		Matrix3 matAim3;
		matAim3.TrackToZ(normal, &up);
		Transform3 matAim(matAim3, Vector3(0));

		VERUS_FOR(i, _capacity)
		{
			if (_vParticles[i]._timeLeft >= 0)
			{
				_vParticles[i]._timeLeft -= dt;
				if (_vParticles[i]._timeLeft < 0)
				{
					_vParticles[i]._timeLeft = -1;
					continue;
				}

				if (_pDelegate)
				{
					_pDelegate->Particles_OnUpdate(*this, i, _vParticles[i], up, normal, matAim);
				}
				else
				{
					if (!_decal)
					{
						Point3 hitPoint;
						Vector3 hitNormal;
						TimeCorrectedVerletIntegration(_vParticles[i], hitPoint, hitNormal);
					}

					const float t = glm::quadraticEaseInOut(1 - _vParticles[i]._timeLeft * _vParticles[i]._invTotalTime);
					const Vector4 color = VMath::lerp(t, _vParticles[i]._beginColor, _vParticles[i]._endColor);
					const float size = Math::Lerp(_vParticles[i]._beginSize, _vParticles[i]._endSize, t);
					const float spin = Math::Lerp(_vParticles[i]._beginSpin, _vParticles[i]._endSpin, t);
					const float additive = Math::Lerp(_vParticles[i]._beginAdditive, _vParticles[i]._endAdditive, t);

					if (BillboardType::axial == _billboardType && _decal)
						normal = _vParticles[i]._velocity;

					const Transform3 matW = GetBillboardMatrix(i, size, spin, up, normal, matAim);

					PushBillboard(i, color, matW, additive);
				}
			}
		}
		if (_drawCount)
		{
			_geo->UpdateVertexBuffer(_vVB.data(), 0, nullptr, _drawCount * 4);
			_geo->UpdateIndexBuffer(_vIB.data(), nullptr, _drawCount * 6);
		}
	}
}

void Particles::Draw()
{
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;
	VERUS_QREF_ATMO;

	if (!_drawCount || !_csh.IsSet())
		return;

	auto cb = renderer.GetCommandBuffer();

	float brightness = _brightness;
	if (brightness < 0)
	{
		const glm::vec3 gray = glm::saturation(0.f, atmo.GetSunColor().GLM() + atmo.GetAmbientColor().GLM());
		brightness = gray.x * abs(brightness);
	}

	s_ubParticlesVS._matP = wm.GetPassCamera()->GetMatrixP().UniformBufferFormat();
	s_ubParticlesVS._matWVP = wm.GetPassCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubParticlesVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubParticlesVS._brightness.x = brightness;
	s_ubParticlesFS._tilesetSize = _tilesetSize.GLM();

	switch (_billboardType)
	{
	case BillboardType::none:
		cb->BindPipeline(s_pipe[PIPE_MAIN]);
		break;
	default:
		cb->BindPipeline(s_pipe[PIPE_BILLBOARDS]);
	}
	cb->BindVertexBuffers(_geo);
	if (BillboardType::none != _billboardType)
		cb->BindIndexBuffer(_geo);
	s_shader->BeginBindDescriptors();
	cb->BindDescriptors(s_shader, 0);
	cb->BindDescriptors(s_shader, 1, _csh);
	if (BillboardType::none == _billboardType)
		cb->Draw(_drawCount);
	else
		cb->DrawIndexed(_drawCount * 6);
	s_shader->EndBindDescriptors();
}

void Particles::GetLifeTime(float& mn, float& mx) const
{
	mn = _lifeTimeRange.getX();
	mx = _lifeTimeRange.getY();
}

void Particles::SetLifeTime(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_lifeTimeRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetSpeed(float& mn, float& mx) const
{
	mn = _speedRange.getX();
	mx = _speedRange.getY();
}

void Particles::SetSpeed(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_speedRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetBeginAdditive(float& mn, float& mx) const
{
	mn = _beginAdditiveRange.getX();
	mx = _beginAdditiveRange.getY();
}

void Particles::SetBeginAdditive(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_beginAdditiveRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetBeginColor(RVector4 mn, RVector4 mx) const
{
	mn = _beginColorMin;
	mx = _beginColorMin + _beginColorDelta;
}

void Particles::SetBeginColor(RcVector4 mn, RcVector4 mx)
{
	_beginColorMin = mn;
	_beginColorDelta = mx - mn;
}

void Particles::GetEndColor(RVector4 mn, RVector4 mx) const
{
	mn = _endColorMin;
	mx = _endColorMin + _endColorDelta;
}

void Particles::SetEndColor(RcVector4 mn, RcVector4 mx)
{
	_endColorMin = mn;
	_endColorDelta = mx - mn;
}

void Particles::GetEndAdditive(float& mn, float& mx) const
{
	mn = _endAdditiveRange.getX();
	mx = _endAdditiveRange.getY();
}

void Particles::SetEndAdditive(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_endAdditiveRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetBeginSize(float& mn, float& mx) const
{
	mn = _beginSizeRange.getX();
	mx = _beginSizeRange.getY();
}

void Particles::SetBeginSize(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_beginSizeRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetEndSize(float& mn, float& mx) const
{
	mn = _endSizeRange.getX();
	mx = _endSizeRange.getY();
}

void Particles::SetEndSize(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_endSizeRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetBeginSpin(float& mn, float& mx) const
{
	mn = _beginSpinRange.getX();
	mx = _beginSpinRange.getY();
}

void Particles::SetBeginSpin(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_beginSpinRange = Vector3(mn, mx, mx - mn);
}

void Particles::GetEndSpin(float& mn, float& mx) const
{
	mn = _endSpinRange.getX();
	mx = _endSpinRange.getY();
}

void Particles::SetEndSpin(float mn, float mx)
{
	VERUS_RT_ASSERT(mx <= mx);
	_endSpinRange = Vector3(mn, mx, mx - mn);
}

int Particles::Add(RcPoint3 pos, RcVector3 dir, float scale, PcVector4 pUserColor, void* pUser)
{
	VERUS_QREF_UTILS;
	VERUS_QREF_TIMER;

	Vector3 wind = Vector3(0);
	if (World::Atmosphere::IsValidSingleton())
	{
		VERUS_QREF_ATMO;
		wind = atmo.GetWindVelocity();
	}

	const Vector3 accel = _gravity * _gravityStrength + wind * _windStrength;

	RRandom random = utils.GetRandom();
	RParticle particle = _vParticles[_addAt];
	particle._pUser = pUser;
	const int index = _addAt;

	const int tilesetX = random.Next() % _tilesetX;
	const int tilesetY = random.Next() % _tilesetY;
	const float lifeTime = scale * (_lifeTimeRange.getX() + _lifeTimeRange.getZ() * random.NextFloat());

	if (_decal)
	{
		particle._position = pos;
		particle._prevPosition = pos;
		particle._velocity = dir;
	}
	else
	{
		const float zone = (_pUserZone ? *_pUserZone : _zone) * scale;
		const Point3 posZone = pos + ((zone > 0) ? Vector3(glm::ballRand(zone)) : Vector3(0));
		particle._position = posZone;
		particle._velocity = dir * (scale * (_speedRange.getX() + _speedRange.getZ() * random.NextFloat()));
		particle._prevPosition = posZone - particle._velocity * dt - accel * (0.5f * timer.GetDeltaTimeSq());
	}

	particle._tcOffset = Vector4(tilesetX * _tilesetSize.getZ(), tilesetY * _tilesetSize.getW());

	particle._invTotalTime = 1 / lifeTime;
	particle._timeLeft = lifeTime;

	particle._beginAdditive = _beginAdditiveRange.getX() + _beginAdditiveRange.getZ() * random.NextFloat();
	particle._endAdditive = _endAdditiveRange.getX() + _endAdditiveRange.getZ() * random.NextFloat();

	particle._beginColor = _beginColorMin + _beginColorDelta * random.NextFloat();
	particle._endColor = _endColorMin + _endColorDelta * random.NextFloat();
	if (pUserColor)
	{
		particle._beginColor = VMath::mulPerElem(particle._beginColor, *pUserColor);
		particle._endColor = VMath::mulPerElem(particle._endColor, *pUserColor);
	}

	particle._beginSize = scale * (_beginSizeRange.getX() + _beginSizeRange.getZ() * random.NextFloat());
	particle._endSize = scale * (_endSizeRange.getX() + _endSizeRange.getZ() * random.NextFloat());

	particle._beginSpin = _beginSpinRange.getX() + _beginSpinRange.getZ() * random.NextFloat();
	particle._endSpin = _endSpinRange.getX() + _endSpinRange.getZ() * random.NextFloat();

	if (_decal)
		particle._endSpin = particle._beginSpin;

	_addAt++;
	_addAt %= _capacity;

	return index;
}

void Particles::AddFlow(RcPoint3 pos, RcVector3 dirOffset, float scale, PcVector4 pUserColor, float* pUserFlowRemainder, float intensity)
{
	if (!_flowEnabled)
		return;
	VERUS_QREF_TIMER;
	float& flowRemainder = pUserFlowRemainder ? *pUserFlowRemainder : _flowRemainder;
	flowRemainder += _particlesPerSecond * dt * intensity;
	int count = static_cast<int>(flowRemainder);
	if (count)
		flowRemainder = fmod(flowRemainder, 1.f);
	VERUS_FOR(i, count)
	{
		const Vector3 dir = VMath::normalizeApprox(Vector3(glm::ballRand(1.f)) + dirOffset);
		Add(pos, dir, scale, pUserColor);
	}
}

bool Particles::TimeCorrectedVerletIntegration(RParticle particle, RPoint3 point, RVector3 normal)
{
	VERUS_QREF_TIMER;

	bool hit = false;

	Vector3 wind = Vector3(0);
	if (World::Atmosphere::IsValidSingleton())
	{
		VERUS_QREF_ATMO;
		wind = atmo.GetWindVelocity();
	}

	const Vector3 accel = _gravity * _gravityStrength + wind * _windStrength;
	const Point3 currentPos = particle._position;
	particle._position += (currentPos - particle._prevPosition) * timer.GetVerletValue() + accel * timer.GetDeltaTimeSq();
	particle._prevPosition = currentPos;
	particle._velocity = (particle._position - currentPos) * timer.GetDeltaTimeInv();

	if (_collide)
	{
		if (particle._inContact && VMath::lengthSqr(particle._velocity) < 0.15 * 0.15f)
		{
			particle._velocity = Vector3(0);
			particle._position = currentPos;
			particle._prevPosition = currentPos;
		}
		else
		{
			VERUS_QREF_WM;
			if (wm.RayTestEx(particle._prevPosition, particle._position, nullptr, &point, &normal, nullptr, Physics::Bullet::I().GetMainMask()))
			{
				hit = true;
				particle._inContact = normal.getY() > 0.7071f;
				particle._velocity = particle._velocity.Reflect(normal) * _bounceStrength;
				particle._prevPosition = point;
				particle._position = point + particle._velocity * timer.GetDeltaTime();
			}
			else
				particle._inContact = false;
		}
	}

	if (BillboardType::axial == _billboardType && !_decal)
		particle._axis = VMath::normalizeApprox(particle._velocity);

	return hit;
}

bool Particles::EulerIntegration(RParticle particle, RPoint3 point, RVector3 normal)
{
	return false;
}

void Particles::PushPointSprite(int index, RcVector4 color, float size, float additive)
{
	_vVB[_drawCount].pos.x = _vParticles[index]._position.getX();
	_vVB[_drawCount].pos.y = _vParticles[index]._position.getY();
	_vVB[_drawCount].pos.z = _vParticles[index]._position.getZ();
	_vVB[_drawCount].pos.w = additive;
	_vVB[_drawCount].tc0.x = _vParticles[index]._tcOffset.getX();
	_vVB[_drawCount].tc0.y = _vParticles[index]._tcOffset.getY();
	_vVB[_drawCount].color = color.ToColor();
	_vVB[_drawCount].psize = size;
	_drawCount++;
}

void Particles::PushBillboard(int index, RcVector4 color, RcTransform3 matW, float additive)
{
	Point3 pos[4] =
	{
		Point3(+0.5f, +0.5f),
		Point3(-0.5f, +0.5f),
		Point3(-0.5f, -0.5f),
		Point3(+0.5f, -0.5f)
	};
	static const Vector4 tc[4] =
	{
		Vector4(1, 0),
		Vector4(0, 0),
		Vector4(0, 1),
		Vector4(1, 1)
	};

	VERUS_FOR(i, 4)
		pos[i] = matW * pos[i];

	const int vertexOffset = _drawCount << 2;
	VERUS_FOR(i, 4)
	{
		RVertex vertex = _vVB[vertexOffset + i];
		vertex.pos.x = pos[i].getX();
		vertex.pos.y = pos[i].getY();
		vertex.pos.z = pos[i].getZ();
		vertex.pos.w = additive;
		vertex.tc0[0] = tc[i].getX() * _tilesetSize.getZ() + _vParticles[index]._tcOffset.getX();
		vertex.tc0[1] = tc[i].getY() * _tilesetSize.getW() + _vParticles[index]._tcOffset.getY();
		vertex.color = color.ToColor();
		vertex.psize = 1;
	}

	const int indexOffset = _drawCount * 6;
	_vIB[indexOffset + 0] = vertexOffset + 0;
	_vIB[indexOffset + 1] = vertexOffset + 2;
	_vIB[indexOffset + 2] = vertexOffset + 1;
	_vIB[indexOffset + 3] = vertexOffset + 0;
	_vIB[indexOffset + 4] = vertexOffset + 3;
	_vIB[indexOffset + 5] = vertexOffset + 2;

	_drawCount++;
}

Transform3 Particles::GetBillboardMatrix(int index, float size, float spin, RcVector3 up, RcVector3 normal, RTransform3 matAim)
{
	Vector3 up2(up), normal2(normal);

	switch (_billboardType)
	{
	case BillboardType::screenViewpointOriented:
	{
	}
	break;
	case BillboardType::worldViewpointOriented:
	{
	}
	break;
	case BillboardType::axial:
	{
		if (_decal)
		{
			Matrix3 matAim3;
			matAim3.TrackToZ(-normal2, &up2);
			matAim = Transform3(matAim3, Vector3(0));
		}
		else
		{
			up2 = _vParticles[index]._axis;
			const Vector3 right = VMath::cross(up2, normal2);
			normal2 = VMath::normalizeApprox(VMath::cross(right, up2));
			Matrix3 matAim3;
			matAim3.TrackToZ(normal2, &up2);
			matAim = Transform3(matAim3, Vector3(0));
		}
	}
	break;
	}

	return Transform3::translation(Vector3(_vParticles[index]._position)) * matAim *
		Transform3(VMath::appendScale(Matrix3::rotationZ(spin), Vector3(size * _ratio, size, 0)), Vector3(0));
}
