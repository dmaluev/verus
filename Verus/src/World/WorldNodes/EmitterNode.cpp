// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// EmitterNode:

EmitterNode::EmitterNode()
{
	_type = NodeType::emitter;
}

EmitterNode::~EmitterNode()
{
	Done();
}

void EmitterNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : desc._url);

	_particlesNode.Init(desc._url);
}

void EmitterNode::Done()
{
	_particlesNode.Done();

	VERUS_DONE(EmitterNode);
}

void EmitterNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	REmitterNode emitterNode = static_cast<REmitterNode>(node);

	_flowColor = emitterNode._flowColor;
	_flowOffset = emitterNode._flowOffset;
	_flowScale = emitterNode._flowScale;

	if (NodeType::emitter == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = _C(emitterNode._particlesNode->GetURL());
		Init(desc);
	}
}

void EmitterNode::Update()
{
	VERUS_QREF_WM;
	const Point3 headPos = wm.GetHeadCamera()->GetEyePosition();
	if (!IsDisabled() && _particlesNode && VMath::distSqr(GetPosition(), headPos) < 50 * 50.f)
		_particlesNode->GetParticles().AddFlow(GetPosition(), _flowOffset, _flowScale, &_flowColor, &_flow);
}

void EmitterNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _flowColor.GLM();
	stream << _flowOffset.GLM();
	stream.WriteString(_C(_particlesNode->GetURL()));
	stream << _flowScale;
}

void EmitterNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	glm::vec4 flowColor;
	glm::vec3 flowOffset;
	char particlesURL[IO::Stream::s_bufferSize] = {};

	stream >> flowColor;
	stream >> flowOffset;
	stream.ReadString(particlesURL);
	stream >> _flowScale;

	_flowColor = flowColor;
	_flowOffset = flowOffset;

	if (NodeType::emitter == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = particlesURL;
		Init(desc);
	}
}

void EmitterNode::Deserialize_LegacyXXX(IO::RStream stream)
{
	BaseNode::Deserialize_LegacyXXX(stream);

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

	if (NodeType::emitter == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = urlParticles;
		Init(desc);
		UpdateRigidBodyTransform();

		if (urlAudio[0])
		{
			SoundNode::Desc soundDesc;
			soundDesc._name = _C(_name);
			soundDesc._url = urlAudio;
			SoundNodePtr soundNode;
			soundNode.Init(soundDesc);
			soundNode->SetParent(this, true);
		}
	}
}

void EmitterNode::DeserializeXML_LegacyXXX(pugi::xml_node node)
{
	BaseNode::DeserializeXML_LegacyXXX(node);

	if (auto attr = node.attribute("name"))
		_name = attr.value();
	_flowColor = Vector4::Replicate(1);
	if (auto attr = node.attribute("color"))
		_flowColor.FromColorString(attr.value());
	_flowOffset = Vector3(0);
	if (auto attr = node.attribute("offset"))
		_flowOffset.FromString(attr.value());
	_flowScale = node.attribute("scale").as_float(_flowScale);

	if (NodeType::emitter == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = node.attribute("urlParticles").value();
		Init(desc);
		UpdateRigidBodyTransform();

		CSZ urlAudio = node.attribute("urlAudio").value();
		if (urlAudio)
		{
			SoundNode::Desc soundDesc;
			soundDesc._name = _C(_name);
			soundDesc._url = urlAudio;
			SoundNodePtr soundNode;
			soundNode.Init(soundDesc);
			soundNode->SetParent(this, true);
		}
	}
}

// EmitterNodePtr:

void EmitterNodePtr::Init(EmitterNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertEmitterNode();
	_p->Init(desc);
}

void EmitterNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertEmitterNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void EmitterNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
