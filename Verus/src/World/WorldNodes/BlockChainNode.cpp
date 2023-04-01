// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// BlockChainNode::SourceModel:

bool BlockChainNode::SourceModel::IsAssignedToSlot(int slot) const
{
	for (const auto& x : _vSlots)
	{
		if (x.Includes(slot))
			return true;
	}
	return false;
}

// BlockChainNode:

BlockChainNode::BlockChainNode()
{
	_type = NodeType::blockChain;
}

BlockChainNode::~BlockChainNode()
{
	Done();
}

void BlockChainNode::Init(RcDesc desc)
{
	_pParent = desc._pPathNode;
	UpdateDepth();
	String name;
	if (!desc._name)
		name = String("Bc") + _C(desc._pPathNode->GetName());
	BaseNode::Init(desc._name ? desc._name : _C(name));

	_vSourceModels.reserve(8);
}

void BlockChainNode::Done()
{
	VERUS_DONE(BlockChainNode);
}

void BlockChainNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RBlockChainNode blockChainNode = static_cast<RBlockChainNode>(node);

	if (NodeType::block == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		Init(desc);
	}
}

void BlockChainNode::Update()
{
	if (!_async_generatedNodes)
	{
		bool allLoaded = true;
		for (const auto& x : _vSourceModels)
		{
			if (x._modelNode && !x._modelNode->GetMesh().IsLoaded())
			{
				allLoaded = false;
				break;
			}
		}
		if (allLoaded)
		{
			VERUS_QREF_WM;
			wm.GenerateBlockChainNodes(this);
			_async_generatedNodes = true;
		}
	}
}

void BlockChainNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::blockChain_generateNodes));
}

void BlockChainNode::ExecuteEditorCommand(RcEditorCommand command)
{
	BaseNode::ExecuteEditorCommand(command);

	switch (command._code)
	{
	case EditorCommandCode::blockChain_generateNodes:
	{
		_async_generatedNodes = false;
	}
	break;
	}
}

bool BlockChainNode::CanSetParent(PBaseNode pNode) const
{
	return false;
}

void BlockChainNode::OnNodeDeleted(PBaseNode pNode, bool afterEvent, bool hierarchy)
{
	BaseNode::OnNodeDeleted(pNode, afterEvent, hierarchy);

	if (!afterEvent && pNode->GetType() == NodeType::model)
		UnbindSourceModel(static_cast<PModelNode>(pNode));
}

void BlockChainNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	VERUS_QREF_WM;

	stream << GetSourceModelCount();
	for (const auto& x : _vSourceModels)
	{
		stream << wm.GetIndexOf(x._modelNode.Get(), true);
		stream << x._length;
		stream.WriteString(_C(x._arguments));
	}
}

void BlockChainNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	VERUS_QREF_WM;

	int sourceModelCount;

	stream >> sourceModelCount;
	VERUS_FOR(i, sourceModelCount)
	{
		SourceModel sourceModel;
		int modelIndex = -1;
		stream >> modelIndex;
		sourceModel._modelNode.Attach(static_cast<PModelNode>(wm.GetNodeByIndex(modelIndex)));
		stream >> sourceModel._length;
		char arguments[IO::Stream::s_bufferSize] = {};
		stream.ReadString(arguments);
		sourceModel._arguments = arguments;
		_vSourceModels.push_back(std::move(sourceModel));
	}

	if (NodeType::blockChain == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._pPathNode = GetParent();
		Init(desc);
	}
}

bool BlockChainNode::BindSourceModel(PModelNode pModelNode)
{
	for (const auto& x : _vSourceModels)
	{
		if (x._modelNode == pModelNode)
			return false;
	}

	SourceModel sourceModel;
	sourceModel._modelNode.Attach(pModelNode);
	sourceModel._length = pModelNode ? pModelNode->GetMesh().GetBounds().GetDimensions().getZ() : 1.f;
	_vSourceModels.push_back(std::move(sourceModel));
	return true;
}

bool BlockChainNode::UnbindSourceModel(PModelNode pModelNode)
{
	return _vSourceModels.end() != _vSourceModels.erase(std::remove_if(_vSourceModels.begin(), _vSourceModels.end(),
		[pModelNode](RSourceModel sourceModel)
		{
			return sourceModel._modelNode == pModelNode;
		}), _vSourceModels.end());
}

int BlockChainNode::GetSourceModelCount() const
{
	return Utils::Cast32(_vSourceModels.size());
}

BlockChainNode::PcSourceModel BlockChainNode::GetSourceModelForSlot(int slot) const
{
	if (_vSourceModels.empty())
		return nullptr;
	for (int i = 1; i < _vSourceModels.size(); ++i)
	{
		if (_vSourceModels[i].IsAssignedToSlot(slot))
			return &_vSourceModels[i];
	}
	return &_vSourceModels[0];
}

ModelNodePtr BlockChainNode::GetModelNodeForSlot(int slot) const
{
	if (_vSourceModels.empty())
		return ModelNodePtr();
	for (int i = 1; i < _vSourceModels.size(); ++i)
	{
		if (_vSourceModels[i].IsAssignedToSlot(slot))
			return _vSourceModels[i]._modelNode;
	}
	return _vSourceModels[0]._modelNode;
}

float BlockChainNode::GetLengthForSlot(int slot) const
{
	if (_vSourceModels.empty())
		return 0;
	for (int i = 1; i < _vSourceModels.size(); ++i)
	{
		if (_vSourceModels[i].IsAssignedToSlot(slot))
			return _vSourceModels[i]._length;
	}
	return _vSourceModels[0]._length;
}

void BlockChainNode::ParseArguments()
{
	for (auto& x : _vSourceModels)
	{
		x._vSlots.clear();
		x._noPhysicsNode = false;
		x._noShadowCaster = false;
		Vector<String> vArgs;
		Str::Explode(_C(x._arguments), ",", vArgs);
		for (const auto& arg : vArgs)
		{
			if (isdigit(arg[0]))
			{
				const size_t dash = arg.find('-');
				if (dash != String::npos)
					x._vSlots.push_back(Range(atoi(_C(arg)), atoi(_C(arg) + dash + 1) + 1));
				else
					x._vSlots.push_back(Range(atoi(_C(arg))));
			}
			else if (arg == "-nophy")
			{
				x._noPhysicsNode = true;
			}
			else if (arg == "-nosc")
			{
				x._noShadowCaster = true;
			}
		}
	}
}

// BlockChainNodePtr:

void BlockChainNodePtr::Init(BlockChainNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBlockChainNode();
	_p->Init(desc);
}

void BlockChainNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBlockChainNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void BlockChainNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
