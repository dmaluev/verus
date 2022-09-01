// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// SceneNode:

void SceneNode::PreventNameCollision()
{
	_name.clear();
}

SceneNode::SceneNode()
{
}

SceneNode::~SceneNode()
{
}

int SceneNode::UserPtr_GetType()
{
	return +_type;
}

void SceneNode::DrawBounds()
{
	if (!_bounds.IsNull())
	{
		VERUS_QREF_HELPERS;
		const Transform3 tr = _bounds.GetDrawTransform();
		helpers.DrawBox(&tr);
	}
}

void SceneNode::Rename(CSZ name)
{
	if (_name == name)
		return;
	VERUS_QREF_SM;
	_name = sm.EnsureUniqueName(name);
}

void SceneNode::SetDynamic(bool mode)
{
	_dynamic = mode;
	_octreeBindOnce = false;
	UpdateBounds();
}

void SceneNode::SetTransform(RcTransform3 tr)
{
	_tr = tr;
	// Also update the UI values and bounds:
	const Quat q(_tr.getUpper3x3());
	_uiRotation.EulerFromQuaternion(q);
	_uiScale = _tr.GetScale();
	UpdateBounds();
}

void SceneNode::RestoreTransform(RcTransform3 tr, RcVector3 rot, RcVector3 scale)
{
	_tr = tr;
	_uiRotation = rot;
	_uiScale = scale;
	UpdateBounds();
}

void SceneNode::ComputeTransform()
{
	Quat q;
	_uiRotation.EulerToQuaternion(q);
	_tr = VMath::appendScale(Transform3(q, _tr.getTranslation()), _uiScale);
}

Point3 SceneNode::GetPosition() const
{
	return _tr.getTranslation();
}

Vector3 SceneNode::GetRotation() const
{
	return _uiRotation;
}

Vector3 SceneNode::GetScale() const
{
	return _uiScale;
}

void SceneNode::MoveTo(RcPoint3 pos)
{
	_tr.setTranslation(Vector3(pos));
	UpdateBounds();
}

void SceneNode::RotateTo(RcVector3 v)
{
	_uiRotation = v;
	ComputeTransform();
	UpdateBounds();
}

void SceneNode::ScaleTo(RcVector3 v)
{
	_uiScale = v;
	ComputeTransform();
	UpdateBounds();
}

void SceneNode::RemoveRigidBody()
{
	if (_pBody)
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pBody);
		delete _pBody->getMotionState();
		delete _pBody;
		_pBody = nullptr;
	}
}

void SceneNode::SetCollisionGroup(Physics::Group g)
{
	if (_pBody && _pBody->getBroadphaseHandle()->m_collisionFilterGroup != +g)
	{
		// https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=7538
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pBody);
		bullet.GetWorld()->addRigidBody(_pBody, +g, -1);
	}
}

void SceneNode::MoveRigidBody()
{
	if (_pBody)
	{
		// https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=6729
		VERUS_QREF_BULLET;
		_pBody->setWorldTransform(_tr.Bullet());
		bullet.GetWorld()->updateSingleAabb(_pBody);
	}
}

void SceneNode::Serialize(IO::RSeekableStream stream)
{
	stream.WriteString(_C(_name));
	stream << _uiRotation.GLM();
	stream << _uiScale.GLM();
	stream << _tr.GLM4x3();
	_dict.Serialize(stream);
}

void SceneNode::Deserialize(IO::RStream stream)
{
	if (stream.GetVersion() >= IO::Xxx::MakeVersion(3, 0))
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
		_tr = tr;
	}
}

void SceneNode::SaveXML(pugi::xml_node node)
{
	node.append_attribute("name") = _C(_name);
	node.append_attribute("uiR") = _C(_uiRotation.ToString(true));
	node.append_attribute("uiS") = _C(_uiScale.ToString(true));
	node.append_attribute("tr") = _C(_tr.ToString());
}

void SceneNode::LoadXML(pugi::xml_node node)
{
	if (auto attr = node.attribute("name"))
		_name = attr.value();
	_uiRotation.FromString(node.attribute("uiR").value());
	_uiScale.FromString(node.attribute("uiS").value());
	_tr.FromString(node.attribute("tr").value());
}

float SceneNode::GetDistToHeadSq() const
{
	VERUS_QREF_SM;
	return VMath::distSqr(sm.GetHeadCamera()->GetEyePosition(), GetPosition());
}
