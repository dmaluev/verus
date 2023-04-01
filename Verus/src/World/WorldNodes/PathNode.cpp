// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// PathNode:

PathNode::PathNode()
{
	_type = NodeType::path;
}

PathNode::~PathNode()
{
	Done();
}

void PathNode::Init(RcDesc desc)
{
	BaseNode::Init(desc._name ? desc._name : "Path");
}

void PathNode::Done()
{
	VERUS_DONE(PathNode);
}

void PathNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	if (NodeType::path == _type)
		Init(nullptr);
}

void PathNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::path_insertBlockChain));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::path_insertControlPoint));
}

void PathNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);
}

void PathNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	if (NodeType::path == _type)
		Init(_C(_name));
}

// PathNodePtr:

void PathNodePtr::Init(PathNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPathNode();
	_p->Init(desc);
}

void PathNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPathNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void PathNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
