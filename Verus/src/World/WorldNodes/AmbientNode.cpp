// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// AmbientNode:

AmbientNode::AmbientNode()
{
	_type = NodeType::ambient;
}

AmbientNode::~AmbientNode()
{
	Done();
}

void AmbientNode::Init(RcDesc desc)
{
	if (!(_flags & Flags::readOnlyFlags))
		SetOctreeElementFlag();
	BaseNode::Init(desc._name ? desc._name : "Ambient");

	_trToBoxSpace = VMath::inverse(GetTransform());
}

void AmbientNode::Done()
{
	VERUS_DONE(AmbientNode);
}

void AmbientNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RAmbientNode ambientNode = static_cast<RAmbientNode>(node);

	_color = ambientNode._color;
	_wall = ambientNode._wall;
	_cylindrical = ambientNode._cylindrical;
	_spherical = ambientNode._spherical;
	_priority = ambientNode._priority;

	if (NodeType::ambient == _type)
		Init(_C(_name));
}

void AmbientNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	BaseNode::DrawEditorOverlays(flags);

	if (flags & DrawEditorOverlaysFlags::extras)
	{
		VERUS_QREF_EO;

		const Transform3 tr = GetTransform();
		const UINT32 color = EditorOverlays::GetBasisColorZ(false, 255);
		eo.DrawBox(&tr, color);

		const Transform3 trW = VMath::appendScale(GetTransform(), Vector3::Replicate(_wall));
		const UINT32 colorW = EditorOverlays::GetBasisColorZ(false, 64);
		eo.DrawBox(&trW, colorW);
	}
}

void AmbientNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_special_fitParentBlock));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_special_fillTheRoom));
}

void AmbientNode::ExecuteEditorCommand(RcEditorCommand command)
{
	BaseNode::ExecuteEditorCommand(command);

	switch (command._code)
	{
	case EditorCommandCode::node_special_fitParentBlock:
	{
		FitParentBlock();
	}
	break;
	case EditorCommandCode::node_special_fillTheRoom:
	{
		FillTheRoom();
	}
	break;
	}
}

void AmbientNode::UpdateBounds()
{
	VERUS_QREF_WM;

	const Math::Bounds bounds(Point3::Replicate(-0.5f), Point3::Replicate(+0.5f));

	if (!IsOctreeBindOnce())
	{
		_bounds = Math::Bounds::MakeFromOrientedBox(bounds, GetTransform());
		if (IsOctreeElement())
		{
			wm.GetOctree().BindElement(Math::Octree::Element(_bounds, this), IsDynamic());
			SetOctreeBindOnceFlag(IsDynamic());
		}
	}
	if (IsDynamic())
	{
		_bounds = Math::Bounds::MakeFromOrientedBox(bounds, GetTransform());
		if (IsOctreeElement())
			wm.GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
	}
}

void AmbientNode::OnNodeTransformed(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeTransformed(pNode, afterEvent);

	if (afterEvent)
	{
		if (pNode == this)
			_trToBoxSpace = VMath::inverse(GetTransform());
	}
}

void AmbientNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _color;
	stream << _wall;
	stream << _cylindrical;
	stream << _spherical;
	stream << _priority;
}

void AmbientNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	stream >> _color;
	stream >> _wall;
	stream >> _cylindrical;
	stream >> _spherical;
	stream >> _priority;

	if (NodeType::ambient == _type)
		Init(_C(_name));
}

AmbientNode::Priority AmbientNode::GetPriority() const
{
	return _priority;
}

void AmbientNode::SetPriority(Priority priority)
{
	_priority = priority;
}

Vector3 AmbientNode::GetColor() const
{
	return _color.getXYZ();
}

void AmbientNode::SetColor(RcVector3 color)
{
	_color.setXYZ(color);
}

float AmbientNode::GetIntensity() const
{
	return _color.getW();
}

void AmbientNode::SetIntensity(float i)
{
	_color.setW(i);
}

float AmbientNode::GetWall() const
{
	return _wall;
}

void AmbientNode::SetWall(float wall)
{
	_wall = wall;
}

float AmbientNode::GetCylindrical() const
{
	return _cylindrical;
}

void AmbientNode::SetCylindrical(float x)
{
	_cylindrical = x;
}

float AmbientNode::GetSpherical() const
{
	return _spherical;
}

void AmbientNode::SetSpherical(float x)
{
	_spherical = x;
}

Vector4 AmbientNode::GetInstData() const
{
	return Vector4(_color.getXYZ() * _color.getW(), (Priority::add == _priority) ? 0.f : 1.f);
}

void AmbientNode::FitParentBlock()
{
	PBlockNode pBlockNode = nullptr;
	if (_pParent && NodeType::block == _pParent->GetType())
		pBlockNode = static_cast<PBlockNode>(_pParent);
	if (pBlockNode && pBlockNode->GetModelNode()->IsLoaded())
	{
		const Math::Bounds bounds = pBlockNode->GetModelNode()->GetMesh().GetBounds();
		ScaleTo(bounds.GetDimensions());
		MoveTo(bounds.GetCenter(), true);
	}
}

void AmbientNode::FillTheRoom()
{
	VERUS_QREF_BULLET;
	VERUS_QREF_WM;

	const float maxDist = 100;

	Math::Bounds bounds;
	Point3 toA, toB;

	toA = GetPosition() - VMath::normalize(_trGlobal.getCol0()) * maxDist;
	toB = GetPosition() + VMath::normalize(_trGlobal.getCol0()) * maxDist;
	wm.RayTestEx(GetPosition(), toA, nullptr, &toA, nullptr, nullptr, bullet.GetStaticMask());
	wm.RayTestEx(GetPosition(), toB, nullptr, &toB, nullptr, nullptr, bullet.GetStaticMask());
	bounds.Set(-VMath::dist(GetPosition(), toA), VMath::dist(GetPosition(), toB), 0);

	toA = GetPosition() - VMath::normalize(_trGlobal.getCol1()) * maxDist;
	toB = GetPosition() + VMath::normalize(_trGlobal.getCol1()) * maxDist;
	wm.RayTestEx(GetPosition(), toA, nullptr, &toA, nullptr, nullptr, bullet.GetStaticMask());
	wm.RayTestEx(GetPosition(), toB, nullptr, &toB, nullptr, nullptr, bullet.GetStaticMask());
	bounds.Set(-VMath::dist(GetPosition(), toA), VMath::dist(GetPosition(), toB), 1);

	toA = GetPosition() - VMath::normalize(_trGlobal.getCol2()) * maxDist;
	toB = GetPosition() + VMath::normalize(_trGlobal.getCol2()) * maxDist;
	wm.RayTestEx(GetPosition(), toA, nullptr, &toA, nullptr, nullptr, bullet.GetStaticMask());
	wm.RayTestEx(GetPosition(), toB, nullptr, &toB, nullptr, nullptr, bullet.GetStaticMask());
	bounds.Set(-VMath::dist(GetPosition(), toA), VMath::dist(GetPosition(), toB), 2);

	ScaleTo(bounds.GetDimensions());
	Transform3 tr = GetTransform();
	tr.Normalize();
	MoveTo(tr.getTranslation() + tr.getUpper3x3() * Vector3(bounds.GetCenter()));
}

// AmbientNodePtr:

void AmbientNodePtr::Init(AmbientNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertAmbientNode();
	_p->Init(desc);
}

void AmbientNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertAmbientNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void AmbientNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
