// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// SoundNode:

SoundNode::SoundNode()
{
	_type = NodeType::sound;
}

SoundNode::~SoundNode()
{
	Done();
}

void SoundNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : desc._url);

	_gain = desc._gain;
	_pitch = desc._pitch;
	_looping = desc._looping;

	Load(desc._url);
}

void SoundNode::Done()
{
	_sound.Done();

	VERUS_DONE(SoundNode);
}

void SoundNode::Duplicate(RBaseNode node)
{
	BaseNode::Duplicate(node);

	RSoundNode soundNode = static_cast<RSoundNode>(node);

	_gain = soundNode._gain;
	_pitch = soundNode._pitch;
	_looping = soundNode._looping;

	if (NodeType::sound == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = _C(soundNode.GetURL());
		desc._gain = soundNode._gain;
		desc._pitch = soundNode._pitch;
		desc._looping = soundNode._looping;
		Init(desc);
	}
}

void SoundNode::Update()
{
	VERUS_QREF_WM;
	const Point3 headPos = wm.GetHeadCamera()->GetEyePosition();
	if (_sound)
	{
		const bool inRange = VMath::distSqr(GetPosition(), headPos) < 15 * 15.f;
		if (inRange && !IsDisabled() && !_soundSource)
			Play();
		if (!inRange && _soundSource)
			_soundSource.Done();
	}
}

void SoundNode::Disable(bool disable)
{
	if (disable)
	{
		BaseNode::Disable(true);
		if (_soundSource)
			_soundSource->Stop();
	}
	else
	{
		Disable(true);
		BaseNode::Disable(false);
		if (!_sound || !_sound->IsLoaded())
			return;
		Audio::Source::Desc desc;
		desc._secOffset = _looping ? -1.f : 0.f;
		desc._gain = _gain;
		desc._pitch = _pitch;
		_sound->NewSource(&_soundSource, desc)->PlayAt(GetPosition());
		_soundSource->SetLooping(_looping);
	}
}

void SoundNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream.WriteString(_C(GetURL()));
	stream << _gain;
	stream << _pitch;
	stream << _looping;
}

void SoundNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	char url[IO::Stream::s_bufferSize] = {};

	stream.ReadString(url);
	stream >> _gain;
	stream >> _pitch;
	stream >> _looping;

	if (NodeType::sound == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = url;
		desc._gain = _gain;
		desc._pitch = _pitch;
		desc._looping = _looping;
		Init(desc);
	}
}

void SoundNode::Load(CSZ url)
{
	_sound.Done();
	if (url)
	{
		Audio::Sound::Desc desc(url);
		desc._is3D = true;
		desc._gain = 1;
		desc._pitch = 1;
		desc._looping = _looping;
		_sound.Init(desc);
	}
}

void SoundNode::SetGain(float gain)
{
	_gain = Math::Clamp<float>(gain, 0, 1);
	if (_soundSource)
		_soundSource->SetGain(_gain);
}

void SoundNode::SetPitch(float pitch)
{
	_pitch = Math::Clamp<float>(pitch, 0.5f, 2);
	if (_soundSource)
		_soundSource->SetPitch(_pitch);
}

void SoundNode::SetLooping(bool b)
{
	_looping = b;
	if (_soundSource)
		_soundSource->SetLooping(_looping);
}

void SoundNode::Play()
{
	Disable(false);
}

void SoundNode::Stop()
{
	Disable(true);
}

// SoundNodePtr:

void SoundNodePtr::Init(SoundNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertSoundNode();
	_p->Init(desc);
}

void SoundNodePtr::Duplicate(RBaseNode node)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertSoundNode();
	_p->Duplicate(node);
}

void SoundNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
