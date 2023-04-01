// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// PrefabNode:

PrefabNode::PrefabNode()
{
	_type = NodeType::prefab;
}

PrefabNode::~PrefabNode()
{
	Done();
}

void PrefabNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : "Prefab");
}

void PrefabNode::Done()
{
	DeleteAllInstances();

	VERUS_DONE(PrefabNode);
}

void PrefabNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RPrefabNode prefabNode = static_cast<RPrefabNode>(node);

	_seed = prefabNode._seed;

	if (NodeType::prefab == _type)
		Init(_C(_name));
}

void PrefabNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::prefab_insertInstanceNode));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::prefab_updateInstances));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::prefab_replaceSimilarWithInstances));
}

bool PrefabNode::CanSetParent(PBaseNode pNode) const
{
	return !pNode;
}

void PrefabNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _seed;
}

void PrefabNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	stream >> _seed;

	if (NodeType::prefab == _type)
		Init(_C(_name));
}

void PrefabNode::DeleteAllInstances()
{
	VERUS_QREF_WM;

	Vector<PInstanceNode> vInstanceNodes;
	vInstanceNodes.reserve(wm.GetNodeCount() / 10);
	WorldManager::Query query;
	query._type = NodeType::instance;
	wm.ForEachNode(query, [this, &vInstanceNodes](RBaseNode node)
		{
			RInstanceNode instanceNode = static_cast<RInstanceNode>(node);
			if (instanceNode.GetPrefabNode() == this)
				vInstanceNodes.push_back(&instanceNode);
			return Continue::yes;
		});
	for (auto pInstanceNode : vInstanceNodes)
		wm.DeleteNode(pInstanceNode);
}

// PrefabNodePtr:

void PrefabNodePtr::Init(PrefabNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPrefabNode();
	_p->Init(desc);
}

void PrefabNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPrefabNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void PrefabNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
