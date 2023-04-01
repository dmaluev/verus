// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ShakerNode:

ShakerNode::ShakerNode()
{
	_type = NodeType::shaker;
}

ShakerNode::~ShakerNode()
{
	Done();
}

void ShakerNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : desc._url);

	_shaker.Load(desc._url);
	_shaker.Randomize();
}

void ShakerNode::Done()
{
	ApplyInitialValue();
	VERUS_DONE(ShakerNode);
}

void ShakerNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RShakerNode shakerNode = static_cast<RShakerNode>(node);

	_propertyName = shakerNode._propertyName;
	_initialValue = shakerNode._initialValue;

	if (NodeType::shaker == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = _C(shakerNode._shaker.GetURL());
		Init(desc);

		_shaker.SetSpeed(shakerNode._shaker.GetSpeed());
		_shaker.SetScale(shakerNode._shaker.GetScale());
		_shaker.SetBias(shakerNode._shaker.GetBias());
		_shaker.SetLooping(shakerNode._shaker.IsLooping());
	}
}

void ShakerNode::Update()
{
	if (_shaker.IsLoaded())
	{
		_shaker.Update();

		if (!IsDisabled() && _pParent)
			_pParent->SetPropertyByName(_C(_propertyName), _initialValue * _shaker.Get());
	}
}

void ShakerNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::shaker_presetForIntensity));
}

void ShakerNode::ExecuteEditorCommand(RcEditorCommand command)
{
	BaseNode::ExecuteEditorCommand(command);

	switch (command._code)
	{
	case EditorCommandCode::shaker_presetForIntensity:
	{
		SetPropertyName("intensity");
		_shaker.SetScale(0.5f);
		_shaker.SetBias(0.5f);
	}
	break;
	}
}

void ShakerNode::Disable(bool disable)
{
	BaseNode::Disable(disable);

	if (disable)
		ApplyInitialValue();
	else
		UpdateInitialValue();
}

void ShakerNode::OnNodeParentChanged(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeParentChanged(pNode, afterEvent);

	if (this == pNode)
	{
		if (afterEvent)
			UpdateInitialValue();
		else
			ApplyInitialValue();
	}
}

void ShakerNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream.WriteString(_C(_shaker.GetURL()));
	stream << _shaker.GetSpeed();
	stream << _shaker.GetScale();
	stream << _shaker.GetBias();
	stream << _shaker.IsLooping();
	stream.WriteString(_C(_propertyName));
}

void ShakerNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	char url[IO::Stream::s_bufferSize] = {};
	float speed;
	float scale;
	float bias;
	bool looping;
	char propertyName[IO::Stream::s_bufferSize] = {};

	stream.ReadString(url);
	stream >> speed;
	stream >> scale;
	stream >> bias;
	stream >> looping;
	stream.ReadString(propertyName);

	_propertyName = propertyName;

	if (NodeType::shaker == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = url;
		Init(desc);

		_shaker.SetSpeed(speed);
		_shaker.SetScale(scale);
		_shaker.SetBias(bias);
		_shaker.SetLooping(looping);
		UpdateInitialValue();
	}
}

void ShakerNode::SetPropertyName(CSZ name)
{
	ApplyInitialValue();
	_propertyName = name;
	UpdateInitialValue();
}

void ShakerNode::UpdateInitialValue()
{
	_initialValue = 0;
	if (_pParent)
		_initialValue = _pParent->GetPropertyByName(_C(_propertyName));
}

void ShakerNode::ApplyInitialValue()
{
	if (_pParent)
		_pParent->SetPropertyByName(_C(_propertyName), _initialValue);
}

// ShakerNodePtr:

void ShakerNodePtr::Init(ShakerNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertShakerNode();
	_p->Init(desc);
}

void ShakerNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertShakerNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void ShakerNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
