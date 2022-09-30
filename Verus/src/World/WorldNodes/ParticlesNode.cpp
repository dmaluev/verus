// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ParticlesNode:

ParticlesNode::ParticlesNode()
{
	_type = NodeType::particles;
}

ParticlesNode::~ParticlesNode()
{
	Done();
}

void ParticlesNode::Init(RcDesc desc)
{
	if (_refCount)
		return;

	BaseNode::Init(desc._url);
	_refCount = 1;

	_particles.Init(desc._url);
}

bool ParticlesNode::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		_particles.Done();

		VERUS_DONE(ParticlesNode);
		return true;
	}
	return false;
}

void ParticlesNode::Update()
{
	_particles.Update();
}

void ParticlesNode::Draw()
{
	_particles.Draw();
}

void ParticlesNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::particles_insertEmitterNode));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::particles_insertEmitterAndSoundNodes));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::particles_selectAllEmitters));
}

bool ParticlesNode::CanSetParent(PBaseNode pNode) const
{
	return !pNode;
}

void ParticlesNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream.WriteString(_C(GetURL()));
}

void ParticlesNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	char url[IO::Stream::s_bufferSize] = {};

	stream.ReadString(url);

	if (NodeType::particles == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = url;
		Init(desc);
	}
}

// ParticlesNodePtr:

void ParticlesNodePtr::Init(ParticlesNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertParticlesNode(desc._url);
	_p->Init(desc);
}

void ParticlesNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
