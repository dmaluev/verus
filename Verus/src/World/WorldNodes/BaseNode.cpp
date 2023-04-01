// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// BaseNode:

BaseNode::BaseNode()
{
}

BaseNode::~BaseNode()
{
	Done();
}

int BaseNode::UserPtr_GetType()
{
	return +_type;
}

void BaseNode::Init(RcDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_WM;

	_name = wm.EnsureUniqueName(desc._name ? desc._name : "Node", this);
	wm.SortNodes();

	OnLocalTransformUpdated();
	UpdateGlobalTransform();
	AddDefaultPickingBody(); // Can be overridden by nodes that don't need this rigid body.

	_flags &= ~Flags::readOnlyFlags;
}

void BaseNode::Done()
{
	WorldManager::I().GetOctree().UnbindElement(this);
	RemoveRigidBody();

	VERUS_DONE(BaseNode);
}

void BaseNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	_trLocal = node._trLocal;
	_trGlobal = node._trGlobal;
	_uiRotation = node._uiRotation;
	_uiScale = node._uiScale;
	_bounds = node._bounds;
	_dict = node._dict;
	_pParent = node._pParent;
	_name = node._name;
	_type = node._type;
	_flags = node._flags | Flags::readOnlyFlags;
	_groups = node._groups;
	_depth = node._depth;

	if (NodeType::base == _type)
		Init(_C(_name));
}

void BaseNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	if (flags & DrawEditorOverlaysFlags::bounds)
	{
		VERUS_QREF_EO;
		VERUS_QREF_WM;
		const Transform3 tr = GetTransform() * Math::BoundsBoxMatrix(
			Point3::Replicate(-wm.GetPickingShapeHalfExtent()),
			Point3::Replicate(+wm.GetPickingShapeHalfExtent()));
		eo.DrawBox(&tr, ~EditorOverlays::GetBasisColorX(false, 128));
	}
}

void BaseNode::DrawBounds()
{
	if (!_bounds.IsNull())
	{
		VERUS_QREF_EO;
		const Transform3 tr = _bounds.GetBoxTransform();
		eo.DrawBox(&tr);
	}
}

void BaseNode::DrawRelationshipLines()
{
	if (_pParent)
	{
		VERUS_QREF_DD;
		dd.Begin(CGI::DebugDraw::Type::lines);
		dd.AddLine(GetPosition(), _pParent->GetPosition(),
			VERUS_COLOR_RGBA(255, 255, 255, 64),
			VERUS_COLOR_RGBA(255, 255, 255, 32));
		dd.End();
	}
}

void BaseNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_delete));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_deleteHierarchy));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_duplicate));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_duplicateHierarchy));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_goTo));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_updateLocalTransform));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_resetLocalTransform));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_resetLocalTransformKeepGlobal));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_pointAt));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::node_moveTo));
}

void BaseNode::ExecuteEditorCommand(RcEditorCommand command)
{
	switch (command._code)
	{
	case EditorCommandCode::node_updateLocalTransform:
	{
		UpdateLocalTransform();
	}
	break;
	case EditorCommandCode::node_resetLocalTransform:
	{
		SetTransform(Transform3::identity(), true);
		UpdateRigidBodyTransform();
	}
	break;
	case EditorCommandCode::node_resetLocalTransformKeepGlobal:
	{
		if (_pParent)
		{
			_pParent->SetTransform(_pParent->GetTransform(true) * _trLocal, true);
			_pParent->UpdateRigidBodyTransform();
		}
		SetTransform(Transform3::identity(), true);
		UpdateRigidBodyTransform();
	}
	break;
	}
}

bool BaseNode::CanAutoSelectParentNode() const
{
	return _pParent && NodeType::instance == _pParent->GetType();
}

void BaseNode::Rename(CSZ name)
{
	if (_name == name)
		return;
	VERUS_QREF_WM;
	_name = wm.EnsureUniqueName(name);
	wm.SortNodes();
}

void BaseNode::UpdateDepth()
{
	_depth = 0;
	PcBaseNode pParent = _pParent;
	while (pParent)
	{
		_depth++;
		pParent = pParent->GetParent();
	}
}

PBaseNode BaseNode::GetParent() const
{
	return _pParent;
}

bool BaseNode::SetParent(PBaseNode pNode, bool keepLocalTransform)
{
	if (WorldManager::IsAncestorOf(this, pNode))
		return false; // Illegal operation.
	if (!CanSetParent(pNode))
		return false;

	VERUS_QREF_WM;

	wm.BroadcastOnNodeParentChanged(this, false);
	wm.BroadcastOnNodeTransformed(this, false);

	_pParent = pNode;
	UpdateDepth();

	if (keepLocalTransform)
		UpdateGlobalTransform();
	else // Keep global transform by adjusting local transform?
		UpdateLocalTransform();

	wm.BroadcastOnNodeParentChanged(this, true);
	wm.BroadcastOnNodeTransformed(this, true);

	wm.SortNodes();

	return true;
}

bool BaseNode::IsOctreeElement(bool strict) const
{
	VERUS_QREF_WU;
	// In editor any node can be drawn as overlays, so all nodes should be bound to octree.
	return !!(_flags & Flags::octreeElement) || (!strict && wu.IsEditorMode());
}

void BaseNode::SetOctreeElementFlag(bool octreeElement)
{
	if (octreeElement)
		VERUS_BITMASK_SET(_flags, Flags::octreeElement);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::octreeElement);
}

bool BaseNode::IsOctreeBindOnce() const
{
	return !!(_flags & Flags::octreeBindOnce);
}

void BaseNode::SetOctreeBindOnceFlag(bool octreeBindOnce)
{
	if (octreeBindOnce)
		VERUS_BITMASK_SET(_flags, Flags::octreeBindOnce);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::octreeBindOnce);
}

bool BaseNode::IsDynamic() const
{
	return !!(_flags & Flags::dynamic);
}

void BaseNode::SetDynamicFlag(bool dynamic)
{
	if (dynamic)
		VERUS_BITMASK_SET(_flags, Flags::dynamic);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::dynamic);
	SetOctreeBindOnceFlag(false);
	UpdateBounds();
}

bool BaseNode::IsGenerated() const
{
	return !!(_flags & Flags::generated);
}

void BaseNode::SetGeneratedFlag(bool generated)
{
	if (generated)
		VERUS_BITMASK_SET(_flags, Flags::generated);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::generated);
}

bool BaseNode::IsDisabled() const
{
	return !!(_flags & Flags::disabled);
}

void BaseNode::Disable(bool disable)
{
	if (disable)
		VERUS_BITMASK_SET(_flags, Flags::disabled);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::disabled);
}

bool BaseNode::IsSelected() const
{
	return !!(_flags & Flags::selected);
}

void BaseNode::Select(bool select)
{
	if (select)
		VERUS_BITMASK_SET(_flags, Flags::selected);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::selected);
}

bool BaseNode::IsShadowCaster() const
{
	return !!(_flags & Flags::shadowCaster);
}

void BaseNode::SetShadowCasterFlag(bool shadowCaster)
{
	if (shadowCaster)
		VERUS_BITMASK_SET(_flags, Flags::shadowCaster);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::shadowCaster);
}

bool BaseNode::IsInGroup(int index) const
{
	return !!((_groups >> index) & 0x1);
}

void BaseNode::ToggleGroup(int index)
{
	_groups ^= 1 << index;
}

float BaseNode::GetPropertyByName(CSZ name) const
{
	return 0;
}

void BaseNode::SetPropertyByName(CSZ name, float value)
{
}

Point3 BaseNode::GetPosition(bool local) const
{
	return local ? _trLocal.getTranslation() : _trGlobal.getTranslation();
}

Vector3 BaseNode::GetRotation() const
{
	return _uiRotation;
}

Vector3 BaseNode::GetScale() const
{
	return _uiScale;
}

void BaseNode::MoveTo(RcPoint3 pos, bool local)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	if (local)
	{
		_trLocal.setTranslation(Vector3(pos));
		OnLocalTransformUpdated();
		UpdateGlobalTransform();
	}
	else
	{
		_trGlobal.setTranslation(Vector3(pos));
		UpdateLocalTransform(false);
		UpdateBounds();
	}
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::RotateTo(RcVector3 v)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_uiRotation = v;
	UiToLocalTransform();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::ScaleTo(RcVector3 v)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_uiScale = v;
	UiToLocalTransform();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::SetDirection(RcVector3 dir)
{
	const Matrix3 matR = Matrix3::MakeTrackToZ(dir);
	const Transform3 tr = Transform3(matR, Vector3(GetPosition()));
	SetTransform(tr);
}

RcTransform3 BaseNode::GetTransform(bool local) const
{
	return local ? _trLocal : _trGlobal;
}

void BaseNode::SetTransform(RcTransform3 tr, bool local)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	if (local)
	{
		_trLocal = tr;
		OnLocalTransformUpdated();
		UiFromLocalTransform();
		UpdateGlobalTransform();
	}
	else
	{
		_trGlobal = tr;
		UpdateLocalTransform();
		UpdateBounds();
	}
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::OverrideGlobalTransform(RcTransform3 tr)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_trGlobal = tr;
	UpdateBounds();
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::RestoreTransform(RcTransform3 trLocal, RcVector3 rot, RcVector3 scale)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_trLocal = trLocal;
	_uiRotation = rot;
	_uiScale = scale;
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

void BaseNode::UpdateLocalTransform(bool updateUiValues)
{
	_trLocal = _trGlobal;
	if (_pParent)
		_trLocal = VMath::inverse(_pParent->GetTransform()) * _trGlobal;
	OnLocalTransformUpdated();
	if (updateUiValues)
		UiFromLocalTransform();
}

void BaseNode::UpdateGlobalTransform(bool updateBounds)
{
	_trGlobal = _trLocal;
	if (_pParent)
		_trGlobal = _pParent->GetTransform() * _trLocal;
	if (updateBounds)
		UpdateBounds();
}

void BaseNode::UiToLocalTransform()
{
	Quat q;
	_uiRotation.EulerToQuaternion(q);
	_trLocal = VMath::appendScale(Transform3(q, _trLocal.getTranslation()), _uiScale);
	OnLocalTransformUpdated();
}

void BaseNode::UiFromLocalTransform()
{
	const Quat q(_trLocal.getUpper3x3());
	_uiRotation.EulerFromQuaternion(q);
	_uiScale = _trLocal.GetScale();
}

void BaseNode::UpdateBounds()
{
	VERUS_QREF_WM;

	const Math::Bounds bounds(
		Point3::Replicate(-2 * wm.GetPickingShapeHalfExtent()),
		Point3::Replicate(+2 * wm.GetPickingShapeHalfExtent()));

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

void BaseNode::AddDefaultPickingBody()
{
	VERUS_QREF_BULLET;
	VERUS_QREF_WM;
	VERUS_QREF_WU;
	RemoveRigidBody();
	if (wu.IsEditorMode())
	{
		const btTransform tr = GetTransform().Bullet();
		_pRigidBody = bullet.AddNewRigidBody(0, tr, wm.GetPickingShape(), +Physics::Group::node, +Physics::Group::ray);
		_pRigidBody->setUserPointer(this);
	}
}

void BaseNode::RemoveRigidBody()
{
	if (_pRigidBody)
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pRigidBody);
		delete _pRigidBody->getMotionState();
		delete _pRigidBody;
		_pRigidBody = nullptr;
	}
}

void BaseNode::SetCollisionFilter(UINT32 group, UINT32 mask)
{
	if (_pRigidBody && (
		_pRigidBody->getBroadphaseHandle()->m_collisionFilterGroup != group ||
		_pRigidBody->getBroadphaseHandle()->m_collisionFilterMask != mask))
	{
		// https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=7538
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pRigidBody);
		bullet.GetWorld()->addRigidBody(_pRigidBody, group, mask);
	}
}

void BaseNode::UpdateRigidBodyTransform()
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeRigidBodyTransformUpdated(this, false);
	if (_pRigidBody)
	{
		// https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=6729
		VERUS_QREF_BULLET;
		_pRigidBody->setWorldTransform(_trGlobal.Bullet());
		_pRigidBody->getMotionState()->setWorldTransform(_trGlobal.Bullet());
		bullet.GetWorld()->updateSingleAabb(_pRigidBody);
	}
	wm.BroadcastOnNodeRigidBodyTransformUpdated(this, true);
}

void BaseNode::OnNodeDeleted(PBaseNode pNode, bool afterEvent, bool hierarchy)
{
	if (!afterEvent && pNode == _pParent && hierarchy)
	{
		VERUS_QREF_WM;
		wm.DeleteNode(this, hierarchy);
	}
}

void BaseNode::OnNodeDuplicated(PBaseNode pNode, bool afterEvent, PBaseNode pDuplicatedNode, HierarchyDuplication hierarchyDuplication)
{
	if (afterEvent && pNode == _pParent && (hierarchyDuplication & HierarchyDuplication::yes))
	{
		VERUS_QREF_WM;
		if (PBaseNode pThisDuplicatedNode = wm.DuplicateNode(this, hierarchyDuplication))
			pThisDuplicatedNode->_pParent = pDuplicatedNode;
	}
}

void BaseNode::OnNodeParentChanged(PBaseNode pNode, bool afterEvent)
{
	if (afterEvent && pNode == _pParent)
	{
		VERUS_QREF_WM;
		wm.BroadcastOnNodeParentChanged(this, false);
		UpdateDepth();
		wm.BroadcastOnNodeParentChanged(this, true);
	}
}

void BaseNode::OnNodeRigidBodyTransformUpdated(PBaseNode pNode, bool afterEvent)
{
	if (afterEvent && pNode == _pParent)
	{
		VERUS_QREF_WM;
		UpdateRigidBodyTransform();
	}
}

void BaseNode::OnNodeTransformed(PBaseNode pNode, bool afterEvent)
{
	if (afterEvent && pNode == _pParent)
	{
		VERUS_QREF_WM;
		wm.BroadcastOnNodeTransformed(this, false);
		UpdateGlobalTransform();
		wm.BroadcastOnNodeTransformed(this, true);
	}
}

void BaseNode::Serialize(IO::RSeekableStream stream)
{
	VERUS_QREF_WM;
	stream << _trLocal.GLM4x3();
	stream << _uiRotation.GLM();
	stream << _uiScale.GLM();
	_dict.Serialize(stream);
	stream << wm.GetIndexOf(_pParent, true);
	stream.WriteString(_C(_name));
	stream << (_flags & Flags::serializedMask);
	stream << _groups;
}

void BaseNode::Deserialize(IO::RStream stream)
{
	VERUS_QREF_WM;
	glm::mat4x3 trLocal;
	glm::vec3 uiRotation;
	glm::vec3 uiScale;
	int parentIndex = -1;
	char name[IO::Stream::s_bufferSize] = {};

	stream >> trLocal;
	stream >> uiRotation;
	stream >> uiScale;
	_dict.Deserialize(stream);
	stream >> parentIndex;
	stream.ReadString(name);
	stream >> _flags;
	stream >> _groups;

	_trLocal = trLocal;
	_uiRotation = uiRotation;
	_uiScale = uiScale;
	_pParent = wm.GetNodeByIndex(parentIndex);
	_name = name;
	_flags |= Flags::readOnlyFlags;

	UpdateDepth();

	if (NodeType::base == _type)
		Init(_C(_name));
}

void BaseNode::Deserialize_LegacyXXX(IO::RStream stream)
{
	char name[IO::Stream::s_bufferSize] = {};
	glm::vec3 uiRotation;
	glm::vec3 uiScale;
	glm::mat4x3 tr;

	stream.ReadString(name);
	stream >> uiRotation;
	stream >> uiScale;
	stream >> tr;
	_dict.Deserialize(stream);

	_name = name;
	_uiRotation = uiRotation;
	_uiScale = uiScale;
	_trLocal = tr;

	if (NodeType::base == _type)
		Init(_C(_name));
}

void BaseNode::Deserialize_LegacyXXXPrefab(IO::RStream stream)
{
	Deserialize_LegacyXXX(stream);

	char url[IO::Stream::s_bufferSize] = {};
	stream.ReadString(url);

	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(url, vData);

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	int fragCount = 0;
	for (auto node : root.children("frag"))
		fragCount++;

	int i = 0;
	for (auto node : root.children("frag"))
	{
		CSZ txt = node.text().as_string();
		CSZ mat = node.attribute("mat").value();

		String urlFrag = url;
		String matFrag;
		Str::ReplaceFilename(urlFrag, txt);
		urlFrag += ".x3d";
		if (strchr(mat, ':'))
		{
			matFrag = mat;
		}
		else
		{
			matFrag = url;
			Str::ReplaceFilename(matFrag, mat);
			matFrag += ".vml";
		}

		Transform3 trLocal;
		CSZ tr = node.attribute("tr").value();
		trLocal.FromString(tr);

		BlockNode::Desc desc;
		desc._modelURL = _C(urlFrag);
		desc._materialURL = _C(matFrag);
		BlockNodePtr blockNode;
		blockNode.Init(desc);
		blockNode->SetTransform(trLocal, true);
		blockNode->UpdateRigidBodyTransform();
		blockNode->SetParent(this, true);

		PhysicsNodePtr physicsNode;
		physicsNode.Init(_C(blockNode->GetName()));
		physicsNode->SetParent(blockNode.Get(), true);

		i++;
	}
}

void BaseNode::SerializeXML(pugi::xml_node node)
{
	VERUS_QREF_WM;
	node.append_attribute("trLocal") = _C(_trLocal.ToString());
	node.append_attribute("uiR") = _C(_uiRotation.ToString(true));
	node.append_attribute("uiS") = _C(_uiScale.ToString(true));
	node.append_attribute("parent") = wm.GetIndexOf(_pParent, true);
	node.append_attribute("name") = _C(_name);
	node.append_attribute("flags") = (_flags & Flags::serializedMask);
	node.append_attribute("groups") = _groups;
}

void BaseNode::DeserializeXML(pugi::xml_node node)
{
	VERUS_QREF_WM;
	_trLocal.FromString(node.attribute("trLocal").value());
	_uiRotation.FromString(node.attribute("uiR").value());
	_uiScale.FromString(node.attribute("uiS").as_string("1 1 1"));
	const int parentIndex = node.attribute("parent").as_int(-1);
	if (auto attr = node.attribute("name"))
		_name = attr.value();
	_flags = static_cast<Flags>(node.attribute("flags").as_uint());
	_groups = node.attribute("groups").as_uint();

	_pParent = wm.GetNodeByIndex(parentIndex);

	if (NodeType::base == _type)
		Init(_C(_name));
}

void BaseNode::DeserializeXML_LegacyXXX(pugi::xml_node node)
{
	if (auto attr = node.attribute("name"))
		_name = attr.value();
	_uiRotation.FromString(node.attribute("uiR").value());
	_uiScale.FromString(node.attribute("uiS").as_string("1 1 1"));
	_trLocal.FromString(node.attribute("tr").value());

	if (NodeType::base == _type)
		Init(_C(_name));
}

float BaseNode::GetDistToHeadSq() const
{
	VERUS_QREF_WM;
	return VMath::distSqr(wm.GetHeadCamera()->GetEyePosition(), GetPosition());
}

// BaseNodePtr:

void BaseNodePtr::Init(BaseNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBaseNode();
	_p->Init(desc);
}

void BaseNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBaseNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void BaseNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
