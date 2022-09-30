// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// InstanceNode:

InstanceNode::InstanceNode()
{
	_type = NodeType::instance;
}

InstanceNode::~InstanceNode()
{
	Done();
}

void InstanceNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : _C(desc._pPrefabNode->GetName()));

	_pPrefabNode = desc._pPrefabNode;
}

void InstanceNode::Done()
{
	VERUS_DONE(InstanceNode);
}

void InstanceNode::Duplicate(RBaseNode node)
{
	BaseNode::Duplicate(node);

	RInstanceNode instanceNode = static_cast<RInstanceNode>(node);

	_pPrefabNode = instanceNode._pPrefabNode;

	if (NodeType::instance == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._pPrefabNode = instanceNode._pPrefabNode;
		Init(desc);
	}
}

void InstanceNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	VERUS_QREF_WM;

	stream << wm.GetIndexOf(_pPrefabNode, true);
}

void InstanceNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	VERUS_QREF_WM;

	int prefabIndex = -1;

	stream >> prefabIndex;

	_pPrefabNode = wm.GetNodeByIndex(prefabIndex);

	if (NodeType::instance == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._pPrefabNode = _pPrefabNode;
		Init(desc);
	}
}

// InstanceNodePtr:

void InstanceNodePtr::Init(InstanceNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertInstanceNode();
	_p->Init(desc);
}

void InstanceNodePtr::Duplicate(RBaseNode node)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertInstanceNode();
	_p->Duplicate(node);
}

void InstanceNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
