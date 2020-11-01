// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Emitter:

Emitter::Emitter()
{
	_type = NodeType::emitter;
}

Emitter::~Emitter()
{
	Done();
}

void Emitter::Init(RcDesc desc)
{
	VERUS_QREF_SM;
	_name = sm.EnsureUniqueName(desc._name ? desc._name : "Emitter");
	if (desc._urlParticles)
	{
		SceneParticles::Desc spDesc;
		spDesc._url = desc._urlParticles;
		_particles.Init(spDesc);
		_flow = 0;
	}
	if (desc._urlSound)
		_sound.Init(Audio::Sound::Desc(desc._urlSound).Set3D().SetLoop());
}

void Emitter::Done()
{
	_particles.Done();
	_sound.Done();
}

void Emitter::Update()
{
	VERUS_QREF_SM;
	const Point3 eyePos = sm.GetCamera()->GetEyePosition();
	if (_particles && VMath::distSqr(GetPosition(), eyePos) < 50 * 50.f)
		_particles->GetParticles().AddFlow(GetPosition(), _flowOffset, _flowScale, &_flowColor, &_flow);
	if (_sound)
	{
		const bool inRange = VMath::distSqr(GetPosition(), eyePos) < 15 * 15.f;
		if (inRange && !_src)
		{
			Audio::Source::Desc desc;
			desc._secOffset = -1;
			_sound->NewSource(&_src, desc)->PlayAt(GetPosition());
		}
		if (!inRange && _src)
			_src.Done();
	}
}

void Emitter::Serialize(IO::RSeekableStream stream)
{
	SceneNode::Serialize(stream);

	stream.WriteString(_C(_particles->GetParticles().GetUrl()));
	stream.WriteString(_C(_sound->GetUrl()));
	stream << _flowColor.GLM();
	stream << _flowOffset.GLM();
	stream << _flowScale;
}

void Emitter::Deserialize(IO::RStream stream)
{
	SceneNode::Deserialize(stream);
	const String savedName = _C(GetName());
	PreventNameCollision();

	if (stream.GetVersion() >= IO::Xxx::MakeVersion(3, 0))
	{
		char urlParticles[IO::Stream::s_bufferSize] = {};
		char urlAudio[IO::Stream::s_bufferSize] = {};
		glm::vec4 flowColor;
		glm::vec3 flowOffset;
		stream.ReadString(urlParticles);
		stream.ReadString(urlAudio);
		stream >> flowColor;
		stream >> flowOffset;
		stream >> _flowScale;

		_flowColor = flowColor;
		_flowOffset = flowOffset;

		Desc desc;
		desc._urlParticles = urlParticles;
		desc._urlSound = urlAudio;
		Init(desc);
	}
}

void Emitter::SaveXML(pugi::xml_node node)
{
	SceneNode::SaveXML(node);

	node.append_attribute("color") = _C(_flowColor.ToColorString());
	node.append_attribute("offset") = _C(_flowOffset.ToString(true));
	node.append_attribute("scale") = _flowScale;
	if (_particles)
		node.append_attribute("urlParticles") = _C(_particles->GetParticles().GetUrl());
	if (_sound)
		node.append_attribute("urlAudio") = _C(_sound->GetUrl());
}

void Emitter::LoadXML(pugi::xml_node node)
{
	SceneNode::LoadXML(node);
	PreventNameCollision();

	_flowColor = Vector4::Replicate(1);
	if (auto attr = node.attribute("color"))
		_flowColor.FromColorString(attr.value());
	_flowOffset = Vector3(0);
	if (auto attr = node.attribute("offset"))
		_flowOffset.FromString(attr.value());
	_flowScale = node.attribute("scale").as_float(_flowScale);

	Desc desc;
	if (auto attr = node.attribute("name"))
		desc._name = attr.value();
	desc._urlParticles = node.attribute("urlParticles").value();
	desc._urlSound = node.attribute("urlAudio").value();
	Init(desc);
}

// EmitterPtr:

void EmitterPtr::Init(Emitter::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertEmitter();
	if (desc._node)
		_p->LoadXML(desc._node);
	else
		_p->Init(desc);
}

void EmitterPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteEmitter(_p);
		_p = nullptr;
	}
}
