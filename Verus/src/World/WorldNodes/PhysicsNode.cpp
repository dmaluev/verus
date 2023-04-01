// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// PhysicsNode:

PhysicsNode::PhysicsNode()
{
	_type = NodeType::physics;
}

PhysicsNode::~PhysicsNode()
{
	Done();
}

void PhysicsNode::Init(RcDesc desc)
{
	if (!(_flags & Flags::readOnlyFlags))
	{
		SetDynamicFlag();
	}
	BaseNode::Init(desc._name ? desc._name : "Physics");

	SetShapeType(_shapeType);
	SetBodyType(_bodyType);
}

void PhysicsNode::Done()
{
	RemoveRigidBody();
	VERUS_SMART_DELETE(_pCollisionShape);

	VERUS_DONE(PhysicsNode);
}

void PhysicsNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RPhysicsNode physicsNode = static_cast<RPhysicsNode>(node);

	_size = physicsNode._size;
	_shapeURL = physicsNode._shapeURL;
	_shapeType = physicsNode._shapeType;
	_bodyType = physicsNode._bodyType;
	_mass = physicsNode._mass;
	_friction = physicsNode._friction;
	_restitution = physicsNode._restitution;
	_linearDamping = physicsNode._linearDamping;
	_angularDamping = physicsNode._angularDamping;
	_collisionFilterGroup = physicsNode._collisionFilterGroup;
	_collisionFilterMask = physicsNode._collisionFilterMask;

	if (NodeType::physics == _type)
		Init(_C(_name));
}

void PhysicsNode::Update()
{
	VERUS_QREF_BULLET;

	PBlockNode pBlockNode = nullptr;
	if (_pParent && NodeType::block == _pParent->GetType())
		pBlockNode = static_cast<PBlockNode>(_pParent);
	if (!_async_loadedModel && pBlockNode && pBlockNode->GetModelNode()->IsLoaded())
	{
		_async_loadedModel = true;
		if (ShapeType::parentBlock == _shapeType)
			AddNewRigidBody(pBlockNode->GetModelNode()->GetMesh().GetShape());
	}

	// Physics node can affect parent's transformation:
	if (BodyType::staticBody != _bodyType && pBlockNode && _pRigidBody && _pRigidBody->isActive() && !bullet.IsSimulationPaused())
	{
		btTransform btr;
		_pRigidBody->getMotionState()->getWorldTransform(btr);
		pBlockNode->OverrideGlobalTransform(Transform3(btr) * VMath::inverse(GetTransform(true)));
	}
}

void PhysicsNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	if (flags & DrawEditorOverlaysFlags::bounds)
	{
		if (_pRigidBody) // Show actual bounds from the physics engine?
		{
			VERUS_QREF_EO;
			VERUS_QREF_WM;
			btVector3 bmn, bmx;
			_pRigidBody->getAabb(bmn, bmx);
			const Vector3 extra = Vector3::Replicate(wm.GetPickingShapeHalfExtent() * 0.5f); // Fix block bounds overlapping.
			const Transform3 tr = Math::BoundsBoxMatrix(bmn - extra.Bullet(), bmx + extra.Bullet());
			switch (_bodyType)
			{
			case BodyType::staticBody:    eo.DrawBox(&tr, ~EditorOverlays::GetBasisColorX(false, 0)); break;
			case BodyType::dynamicBody:   eo.DrawBox(&tr, ~EditorOverlays::GetBasisColorY(false, 0)); break;
			case BodyType::kinematicBody: eo.DrawBox(&tr, ~EditorOverlays::GetBasisColorZ(false, 0)); break;
			}
		}
		else // Fallback:
		{
			BaseNode::DrawEditorOverlays(flags);
		}
	}
}

void PhysicsNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::physics_takeSizeFromParentBlock));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::physics_loadShapeFromFile));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::physics_quickDynamicBox));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::physics_quickStaticParentBlock));
}

void PhysicsNode::ExecuteEditorCommand(RcEditorCommand command)
{
	BaseNode::ExecuteEditorCommand(command);

	switch (command._code)
	{
	case EditorCommandCode::physics_takeSizeFromParentBlock:
	{
		TakeSizeFromParentBlock();
	}
	break;
	case EditorCommandCode::physics_quickDynamicBox:
	{
		_friction = 1;
		_restitution = 0;
		SetShapeType(ShapeType::box);
		TakeSizeFromParentBlock();
		MoveTo(Point3(0, _size.getY(), 0), true);
		SetBodyType(BodyType::dynamicBody);
	}
	break;
	case EditorCommandCode::physics_quickStaticParentBlock:
	{
		_friction = 0.5f;
		_restitution = 0.5f;
		if (_pParent && NodeType::block == _pParent->GetType())
			_pParent->UpdateLocalTransform();
		SetShapeType(ShapeType::parentBlock);
		_size = Vector3::Replicate(1);
		MoveTo(Point3(0), true);
		SetBodyType(BodyType::staticBody);
	}
	break;
	}
}

bool PhysicsNode::CanAutoSelectParentNode() const
{
	return (_pParent && NodeType::block == _pParent->GetType()) || BaseNode::CanAutoSelectParentNode();
}

void PhysicsNode::Disable(bool disable)
{
	BaseNode::Disable(disable);

	if (disable)
	{
		RemoveRigidBody();
		VERUS_SMART_DELETE(_pCollisionShape);
	}
	else
	{
		SetShapeType(_shapeType);
		SetBodyType(_bodyType);
	}
}

bool PhysicsNode::IsInCollisionGroup(int index) const
{
	return !!((_collisionFilterGroup >> index) & 0x1);
}

void PhysicsNode::ToggleCollisionGroup(int index)
{
	_collisionFilterGroup ^= 1 << index;
	SetCollisionFilter(_collisionFilterGroup, _collisionFilterMask);
}

bool PhysicsNode::IsInCollisionMask(int index) const
{
	return !!((_collisionFilterMask >> index) & 0x1);
}

void PhysicsNode::ToggleCollisionMask(int index)
{
	_collisionFilterMask ^= 1 << index;
	SetCollisionFilter(_collisionFilterGroup, _collisionFilterMask);
}

void PhysicsNode::UpdateBounds()
{
	if (_pRigidBody)
	{
		VERUS_QREF_WM;

		btVector3 bmn, bmx;
		_pRigidBody->getAabb(bmn, bmx);
		_bounds.Set(bmn, bmx);
		_bounds.FattenBy(wm.GetPickingShapeHalfExtent());

		if (!IsOctreeBindOnce())
		{
			wm.GetOctree().BindElement(Math::Octree::Element(_bounds, this), IsDynamic());
			SetOctreeBindOnceFlag(IsDynamic());
		}
		if (IsDynamic()) // Physics nodes are supposed to be dynamic:
			wm.GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
	}
	else // Fallback:
	{
		BaseNode::UpdateBounds();
	}
}

void PhysicsNode::RemoveRigidBody()
{
	if (_pLocalRigidBody.Get())
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pLocalRigidBody.Get());
		_pLocalRigidBody.Delete();
	}
	else
	{
		BaseNode::RemoveRigidBody();
	}
	_pRigidBody = nullptr;
}

void PhysicsNode::AddNewRigidBody(btCollisionShape* pShape, bool updateBounds)
{
	VERUS_RT_ASSERT(!_pRigidBody);
	VERUS_QREF_BULLET;

	const float mass = (BodyType::dynamicBody == _bodyType) ? _mass : 0;
	const btTransform tr = GetTransform().Bullet();
	_pLocalRigidBody = bullet.AddNewRigidBody(_pLocalRigidBody, mass, tr, pShape, _collisionFilterGroup, _collisionFilterMask);
	_pLocalRigidBody->setFriction(_friction);
	_pLocalRigidBody->setRestitution(_restitution);
	_pLocalRigidBody->setDamping(_linearDamping, _angularDamping);
	_pLocalRigidBody->setUserPointer(this);
	if (BodyType::kinematicBody == _bodyType)
	{
		_pLocalRigidBody->setCollisionFlags(_pLocalRigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		_pLocalRigidBody->setActivationState(DISABLE_DEACTIVATION);
	}
	_pRigidBody = _pLocalRigidBody.Get();
	if (updateBounds)
		UpdateBounds();
}

void PhysicsNode::ResetRigidBodyTransform()
{
	VERUS_QREF_WM;

	if (_pParent) // Reset parent:
	{
		wm.BroadcastOnNodeTransformed(_pParent, false);
		if (_pParent)
		{
			_pParent->UpdateGlobalTransform();
			_pParent->UpdateRigidBodyTransform();
		}
		wm.BroadcastOnNodeTransformed(_pParent, true);
	}
	// Reset this node:
	wm.BroadcastOnNodeTransformed(this, false);
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);

	UpdateRigidBodyTransform();
	if (_pRigidBody)
	{
		const btVector3 zeroVector(0, 0, 0);
		_pRigidBody->clearForces();
		_pRigidBody->setLinearVelocity(zeroVector);
		_pRigidBody->setAngularVelocity(zeroVector);
		_pRigidBody->activate();
	}
}

void PhysicsNode::OnNodeParentChanged(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeParentChanged(pNode, afterEvent);

	if (afterEvent && this == pNode)
	{
		if (ShapeType::parentBlock == _shapeType)
		{
			RemoveRigidBody();
			_async_loadedModel = false;
		}
	}
}

void PhysicsNode::OnNodeRigidBodyTransformUpdated(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeRigidBodyTransformUpdated(pNode, afterEvent);

	if (afterEvent && this == pNode)
	{
		UpdateBounds(); // Compute bounds of updated rigid body.
	}
}

void PhysicsNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _size.GLM();
	stream.WriteString(_C(_shapeURL));
	stream << _shapeType;
	stream << _bodyType;
	stream << _mass;
	stream << _friction;
	stream << _restitution;
	stream << _linearDamping;
	stream << _angularDamping;
	stream << _collisionFilterGroup;
	stream << _collisionFilterMask;
}

void PhysicsNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	glm::vec3 size;
	char url[IO::Stream::s_bufferSize] = {};

	stream >> size;
	stream.ReadString(url);
	stream >> _shapeType;
	stream >> _bodyType;
	stream >> _mass;
	stream >> _friction;
	stream >> _restitution;
	stream >> _linearDamping;
	stream >> _angularDamping;
	stream >> _collisionFilterGroup;
	stream >> _collisionFilterMask;

	_size = size;
	_shapeURL = url;

	if (NodeType::physics == _type)
		Init(_C(_name));
}

void PhysicsNode::SetShapeType(ShapeType shapeType)
{
	_shapeType = shapeType;
	_shapeURL.clear();

	if (!IsDisabled())
	{
		// Recreate rigid body with new shape:
		RemoveRigidBody();
		CreateCollisionShape();
		if (_pCollisionShape)
			AddNewRigidBody(_pCollisionShape);
		else
			_async_loadedModel = false;
	}
}

void PhysicsNode::CreateCollisionShape()
{
	VERUS_SMART_DELETE(_pCollisionShape);

	switch (_shapeType)
	{
	case ShapeType::parentBlock:
	{
		_pCollisionShape = nullptr;
	}
	break;
	case ShapeType::fromFile:
	{
		_pCollisionShape = nullptr;
	}
	break;
	case ShapeType::box:
	{
		_pCollisionShape = new btBoxShape(_size.Bullet());
	}
	break;
	case ShapeType::capsule:
	{
		_pCollisionShape = new btCapsuleShape(_size.getX(), _size.getY());
	}
	break;
	case ShapeType::cone:
	{
		_pCollisionShape = new btConeShape(_size.getX(), _size.getY());
	}
	break;
	case ShapeType::cylinder:
	{
		_pCollisionShape = new btCylinderShape(_size.Bullet());
	}
	break;
	case ShapeType::sphere:
	{
		_pCollisionShape = new btSphereShape(_size.getX());
	}
	break;
	}
}

void PhysicsNode::LoadCollisionShape(CSZ url)
{
	if (!IO::FileSystem::FileExist(url))
		return;

	RemoveRigidBody();
	_shapeType = ShapeType::fromFile;
	_shapeURL = url;

	Vector<BYTE> vMesh;
	IO::FileSystem::LoadResource(url, vMesh);

	btBulletWorldImporter bwi;
	if (!vMesh.empty() && bwi.loadFileFromMemory(reinterpret_cast<char*>(vMesh.data()), Utils::Cast32(vMesh.size())))
	{
		const int count = bwi.getNumCollisionShapes();
		if (count)
			_pCollisionShape = static_cast<btBvhTriangleMeshShape*>(bwi.getCollisionShapeByIndex(0));
	}

	AddNewRigidBody(_pCollisionShape);
}

void PhysicsNode::SetBodyType(BodyType bodyType)
{
	_bodyType = bodyType;

	_collisionFilterGroup = +Physics::Group::immovable;
	switch (bodyType)
	{
	case BodyType::dynamicBody:   _collisionFilterGroup = +Physics::Group::dynamic; break;
	case BodyType::kinematicBody: _collisionFilterGroup = +Physics::Group::kinematic; break;
	}
	_collisionFilterGroup |= +Physics::Group::node;

	if (!IsDisabled())
	{
		ResetRigidBodyTransform();

		// Recreate rigid body:
		RemoveRigidBody();
		if (_pCollisionShape)
			AddNewRigidBody(_pCollisionShape);
		else
			_async_loadedModel = false;
	}
}

void PhysicsNode::Resize(RcVector3 size)
{
	_size = size;

	if (!IsDisabled())
	{
		if (ShapeType::parentBlock != _shapeType &&
			ShapeType::fromFile != _shapeType)
		{
			RemoveRigidBody();
			CreateCollisionShape();
			AddNewRigidBody(_pCollisionShape);
		}
	}
}

void PhysicsNode::TakeSizeFromParentBlock()
{
	PBlockNode pBlockNode = nullptr;
	if (_pParent && NodeType::block == _pParent->GetType())
		pBlockNode = static_cast<PBlockNode>(_pParent);
	if (pBlockNode && pBlockNode->GetModelNode()->IsLoaded())
	{
		const Math::Bounds bounds = pBlockNode->GetModelNode()->GetMesh().GetBounds();
		Vector3 dim = bounds.GetDimensions();
		switch (_shapeType)
		{
		case ShapeType::capsule:
			dim = VMath::mulPerElem(dim, Vector3(0.5f, 1, 0.5f));
			dim.setY(Math::Max(0.001f, dim.getY() - dim.getX() * 2));
			break;
		case ShapeType::cone:
			dim = VMath::mulPerElem(dim, Vector3(0.5f, 1, 0.5f));
			break;
		default: dim *= 0.5f;
		}
		Resize(dim);
	}
}

void PhysicsNode::SetFriction(float friction)
{
	_friction = Math::Clamp<float>(friction, 0, 1);
	if (_pRigidBody)
		_pRigidBody->setFriction(friction);
}

void PhysicsNode::SetRestitution(float restitution)
{
	_restitution = Math::Clamp<float>(restitution, 0, 1);
	if (_pRigidBody)
		_pRigidBody->setRestitution(restitution);
}

void PhysicsNode::SetLinearDamping(float linearDamping)
{
	_linearDamping = Math::Clamp<float>(linearDamping, 0, 1);
	if (_pRigidBody)
		_pRigidBody->setDamping(_linearDamping, _angularDamping);
}

void PhysicsNode::SetAngularDamping(float angularDamping)
{
	_angularDamping = Math::Clamp<float>(angularDamping, 0, 1);
	if (_pRigidBody)
		_pRigidBody->setDamping(_linearDamping, _angularDamping);
}

// PhysicsNodePtr:

void PhysicsNodePtr::Init(PhysicsNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPhysicsNode();
	_p->Init(desc);
}

void PhysicsNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertPhysicsNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void PhysicsNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
